// services/occt/build/embind/bindings/namer.cpp
//
// A batched shape-history query, so the TopoNamer stops paying for a JS<->WASM
// crossing per subshape.
//
// namer.propagate.ts walks every subshape of an operand and, for each, calls
// FindKey, FindIndex, Modified, then Size/First_1/ShapeType/FindIndex/delete/
// RemoveFirst per list element — and again for Generated. On a gear that is
// ~100k crossings per boolean, and it dominated the naming path (3x slower
// than the 7.7 build, which pays the same crossings but with a cheaper call
// stub). The loop itself is pure OCCT, so it belongs here.
//
// `propagate` answers the whole question in ONE call, returning a flat Int32
// array. Layout, repeated once per operand subshape i = 1..opMap.Extent():
//
//     [ selfIdx, modCount, mod... , genCount, gen... ]
//
// All indices are 0-based indices into `resultMap` (-1 when absent), already
// filtered to the requested shape kind. The TypeScript keeps every naming
// decision; this only moves the lookups.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <BRepTools_History.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Shape.hxx>

#include <vector>

#include "namerCore.hxx"

using namespace emscripten;

using ShapeList = NCollection_List<TopoDS_Shape>;

// Append the result-map indices of every shape in `list` that is of `kind`.
static void append_indices(const ShapeList& list, const ShapeIndexedMap& resultMap,
                           TopAbs_ShapeEnum kind, std::vector<int>& out) {
    const size_t countAt = out.size();
    out.push_back(0);
    int count = 0;
    for (ShapeList::Iterator it(list); it.More(); it.Next()) {
        const TopoDS_Shape& s = it.Value();
        if (s.ShapeType() != kind) continue;
        const int idx = resultMap.FindIndex(s);
        if (idx <= 0) continue;
        out.push_back(idx - 1);
        ++count;
    }
    out[countAt] = count;
}

// The two shape-history providers the app names against: every builder algo
// (booleans, fillets, prisms, thick solids) and the unify history object.
template <typename Op>
static std::vector<int> propagate_impl(Op& op, const ShapeIndexedMap& opMap,
                                       const ShapeIndexedMap& resultMap, int kind) {
    const auto want = static_cast<TopAbs_ShapeEnum>(kind);
    const int opN = opMap.Extent();
    std::vector<int> out;
    // A rough reservation: three ints of header per subshape, plus a couple of
    // results each. Growth is amortised anyway; this just avoids the early
    // reallocations on the big maps.
    out.reserve(static_cast<size_t>(opN) * 6);

    for (int i = 1; i <= opN; ++i) {
        const TopoDS_Shape& s = opMap.FindKey(i);

        // Preserved unchanged: OCCT keeps S in the result and reports nothing.
        const int self = resultMap.FindIndex(s);
        out.push_back(self > 0 ? self - 1 : -1);

        append_indices(op.Modified(s), resultMap, want, out);
        append_indices(op.Generated(s), resultMap, want, out);
    }
    return out;
}

// Copy into a JS Int32Array. One crossing, one copy — versus ~100k crossings.
template <typename Op>
static val propagate(Op& op, const ShapeIndexedMap& opMap, const ShapeIndexedMap& resultMap,
                     int kind) {
    const std::vector<int> flat = propagate_impl(op, opMap, resultMap, kind);
    return val(typed_memory_view(flat.size(), flat.data())).call<val>("slice");
}

// --- batched geometric signatures -------------------------------------------
//
// computeKindProps (namer.helpers.ts) built one BRepAdaptor per subshape and
// then crossed for GetType, Plane, Axis, Direction, X/Y/Z, Mass, CentreOfMass
// and half a dozen deletes: 15+ crossings per face, 800 faces per operation.
// These three return every signature for a whole map in one Float64Array.
//
// The layouts mirror FaceProps / EdgeProps / the vertex position exactly, and
// the arithmetic below must stay bit-identical to namer.geometry.ts — the
// signatures it produces are matched across regenerations.

// The surface class of a face, as the code `FaceSurfaceType` (topo.types.ts)
// reads it: 0 plane, 1 cylinder, 2 cone, 3 sphere, 4 torus, 5 bspline (a
// B-spline or a Bezier surface), 6 anything else. The mapping must stay the
// one `surfaceTypeOfAdaptor` (namer.geometry.ts) applies to the same adaptor
// answer: the class is part of every face's signature.
static double surface_type_code(GeomAbs_SurfaceType t) {
    switch (t) {
        case GeomAbs_Plane: return 0;
        case GeomAbs_Cylinder: return 1;
        case GeomAbs_Cone: return 2;
        case GeomAbs_Sphere: return 3;
        case GeomAbs_Torus: return 4;
        case GeomAbs_BSplineSurface:
        case GeomAbs_BezierSurface: return 5;
        default: return 6;
    }
}

// The doubles `faceProps` writes per face (`kFacePropsStride`, namerCore.hxx)
// are exposed as `facePropsStride` so the boot can refuse a kernel linked
// before the surface class was bound (its faces are 8 doubles wide, and
// decoding them 9 at a time would be silent garbage).

// 9 doubles per face: isPlanar, normal xyz, centroid xyz, area, surface class
// (surface_type_code). The class rides in the same call so a curved face costs
// no crossing of its own (doc 34, D-N5): before it, each asked its class
// through a JS adaptor, ~5 crossings a face.
namespace kapy_namer {

void face_row(const TopoDS_Face& face, double* out) {
    // Non-planar faces keep the [0,0,1] default, exactly as the TS did.
    double nx = 0, ny = 0, nz = 1;
    bool isPlanar = false;
    double surfaceType = 6;
    {
        BRepAdaptor_Surface adaptor(face, false);
        const GeomAbs_SurfaceType t = adaptor.GetType();
        surfaceType = surface_type_code(t);
        if (t == GeomAbs_Plane) {
            isPlanar = true;
            // Keep the plane alive: binding a reference to a subobject of
            // the temporary gp_Pln would dangle at the end of the statement.
            const gp_Pln pln = adaptor.Plane();
            const gp_Dir d = pln.Axis().Direction();
            nx = d.X();
            ny = d.Y();
            nz = d.Z();
        }
    }
    // A reversed face flips its normal — including the default one.
    if (face.Orientation() == TopAbs_REVERSED) {
        nx = -nx;
        ny = -ny;
        nz = -nz;
    }
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props, false, false);
    const gp_Pnt com = props.CentreOfMass();
    out[0] = isPlanar ? 1.0 : 0.0;
    out[1] = nx;
    out[2] = ny;
    out[3] = nz;
    out[4] = com.X();
    out[5] = com.Y();
    out[6] = com.Z();
    out[7] = props.Mass();
    out[8] = surfaceType;
}

std::vector<double> face_props(const ShapeIndexedMap& map) {
    const int n = map.Extent();
    std::vector<double> out(static_cast<size_t>(n) * kFacePropsStride);
    for (int i = 1; i <= n; ++i) {
        face_row(TopoDS::Face(map.FindKey(i)),
                 out.data() + static_cast<size_t>(i - 1) * kFacePropsStride);
    }
    return out;
}

// 16 doubles per edge: type (0 line, 1 circle, 2 other), p0 xyz, p1 xyz,
// mid xyz, length, hasCircle, centre xyz, radius.
void edge_row(const TopoDS_Edge& edge, double* out) {
    BRepAdaptor_Curve curve(edge);
    double type = 2, cx = 0, cy = 0, cz = 0, radius = 0, hasCircle = 0;
    const GeomAbs_CurveType t = curve.GetType();
    if (t == GeomAbs_Line) {
        type = 0;
    } else if (t == GeomAbs_Circle) {
        type = 1;
        hasCircle = 1;
        const gp_Circ circ = curve.Circle();
        radius = circ.Radius();
        const gp_Pnt loc = circ.Location();
        cx = loc.X();
        cy = loc.Y();
        cz = loc.Z();
    }
    const double u0 = curve.FirstParameter();
    const double u1 = curve.LastParameter();
    gp_Pnt p0, p1, pM;
    curve.D0(u0, p0);
    curve.D0(u1, p1);
    curve.D0(0.5 * (u0 + u1), pM);

    GProp_GProps props;
    BRepGProp::LinearProperties(edge, props, false, false);

    out[0] = type;
    out[1] = p0.X(); out[2] = p0.Y(); out[3] = p0.Z();
    out[4] = p1.X(); out[5] = p1.Y(); out[6] = p1.Z();
    out[7] = pM.X(); out[8] = pM.Y(); out[9] = pM.Z();
    out[10] = props.Mass();
    out[11] = hasCircle;
    out[12] = cx; out[13] = cy; out[14] = cz;
    out[15] = radius;
}

std::vector<double> edge_props(const ShapeIndexedMap& map) {
    const int n = map.Extent();
    std::vector<double> out(static_cast<size_t>(n) * kEdgePropsStride);
    for (int i = 1; i <= n; ++i) {
        edge_row(TopoDS::Edge(map.FindKey(i)),
                 out.data() + static_cast<size_t>(i - 1) * kEdgePropsStride);
    }
    return out;
}

// 3 doubles per vertex: world position.
void vertex_row(const TopoDS_Vertex& vertex, double* out) {
    const gp_Pnt p = BRep_Tool::Pnt(vertex);
    out[0] = p.X();
    out[1] = p.Y();
    out[2] = p.Z();
}

std::vector<double> vertex_props(const ShapeIndexedMap& map) {
    const int n = map.Extent();
    std::vector<double> out(static_cast<size_t>(n) * kVertexPropsStride);
    for (int i = 1; i <= n; ++i) {
        vertex_row(TopoDS::Vertex(map.FindKey(i)),
                   out.data() + static_cast<size_t>(i - 1) * kVertexPropsStride);
    }
    return out;
}

// The history of an operand against a result, by the two providers the app
// names against (the flat layout is documented at the top of this file).
std::vector<int> propagate_maker(BRepBuilderAPI_MakeShape& op, const ShapeIndexedMap& opMap,
                                 const ShapeIndexedMap& resultMap, int kind) {
    return propagate_impl(op, opMap, resultMap, kind);
}
std::vector<int> propagate_history(BRepTools_History& op, const ShapeIndexedMap& opMap,
                                   const ShapeIndexedMap& resultMap, int kind) {
    return propagate_impl(op, opMap, resultMap, kind);
}

}  // namespace kapy_namer

using namespace kapy_namer;

static val props_view(const std::vector<double>& flat) {
    return val(typed_memory_view(flat.size(), flat.data())).call<val>("slice");
}
static val faceProps(const ShapeIndexedMap& m) { return props_view(face_props(m)); }
static val edgeProps(const ShapeIndexedMap& m) { return props_view(edge_props(m)); }
static val vertexProps(const ShapeIndexedMap& m) { return props_view(vertex_props(m)); }
static int facePropsStride() { return kFacePropsStride; }

// `oc.KapyNamer.propagate(op, opMap, resultMap, kindEnum)`. Presence of this
// object is what namer.propagate.ts probes before taking the fast path, so a
// build without it still works — just slowly (see audit-capabilities.mjs).
struct KapyNamer_Package {};

EMSCRIPTEN_BINDINGS(kapy_occt_namer) {
    class_<KapyNamer_Package>("KapyNamer")
        .class_function("propagate", &propagate<BRepBuilderAPI_MakeShape>)
        .class_function("propagateHistory", &propagate<BRepTools_History>)
        .class_function("faceProps", &faceProps)
        .class_function("facePropsStride", &facePropsStride)
        .class_function("edgeProps", &edgeProps)
        .class_function("vertexProps", &vertexProps);
}
