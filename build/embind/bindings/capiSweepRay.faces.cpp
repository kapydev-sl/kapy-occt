// services/occt/build/embind/bindings/capiSweepRay.faces.cpp
//
// What the swept volume reads of a face and builds from it: its triangles, its
// class against the direction, the exact prism of a monotone face and the
// inflated wedge of one triangle (`runSweepRay.faces.ts`,
// `runSweepRay.prisms.ts`). Lengths use `jsHypot3`, the arithmetic of
// `Math.hypot`, so a normalised vector is the one the binding computes.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the cylinder split, the fuse, the entry points.

#include <cmath>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include "capiMeshCore.hxx"
#include "capiSweepRay.hxx"

namespace kapy_capi {

namespace {

// `Math.hypot(...) || 1`: a zero or NaN length reads as one.
double lengthOrOne(const double v[3]) {
    const double len = jsHypot3(v[0], v[1], v[2]);
    return (len == 0 || std::isnan(len)) ? 1.0 : len;
}

// The bevel-join in-plane offset of a triangle by `eps`: every edge pushed
// outward along its in-plane normal, consecutive endpoints joined directly
// (a hexagon, immune to the miter spikes of sliver triangles).
std::vector<gp_Pnt> inflateTriangle(const SweepTri& tri, double eps) {
    double n[3];
    sweepTriNormal(tri, n);
    const double nl = lengthOrOne(n);
    const double un[3] = {n[0] / nl, n[1] / nl, n[2] / nl};
    std::vector<gp_Pnt> out;
    for (int i = 0; i < 3; ++i) {
        const double* a = tri.p[i];
        const double* b = tri.p[(i + 1) % 3];
        const double e[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        double m[3] = {e[1] * un[2] - e[2] * un[1], e[2] * un[0] - e[0] * un[2],
                       e[0] * un[1] - e[1] * un[0]};
        const double ml = lengthOrOne(m);
        for (double& v : m) v = (v / ml) * eps;
        out.emplace_back(a[0] + m[0], a[1] + m[1], a[2] + m[2]);
        out.emplace_back(b[0] + m[0], b[1] + m[1], b[2] + m[2]);
    }
    return out;
}

// The prism of `face` along `vec` as a solid, or null when it is empty or too
// thin to matter; reversed when its volume reads negative.
TopoDS_Shape orientedPrism(const TopoDS_Shape& face, const gp_Vec& vec) {
    BRepPrimAPI_MakePrism op(face, vec, false, true);
    const TopoDS_Shape shape = op.Shape();
    if (shape.IsNull()) return TopoDS_Shape();
    const double vol = sweepClosedVolume(shape);
    if (std::fabs(vol) < SWEEP_MIN_PRISM_VOLUME) return TopoDS_Shape();
    return vol < 0 ? shape.Reversed() : shape;
}

}  // namespace

void sweepTriNormal(const SweepTri& t, double out[3]) {
    const double ux = t.p[1][0] - t.p[0][0], uy = t.p[1][1] - t.p[0][1],
                 uz = t.p[1][2] - t.p[0][2];
    const double vx = t.p[2][0] - t.p[0][0], vy = t.p[2][1] - t.p[0][1],
                 vz = t.p[2][2] - t.p[0][2];
    out[0] = uy * vz - uz * vy;
    out[1] = uz * vx - ux * vz;
    out[2] = ux * vy - uy * vx;
}

bool sweepFaceTriangles(const TopoDS_Shape& faceShape, bool fine, std::vector<SweepTri>& out) {
    const TopoDS_Face& face = TopoDS::Face(faceShape);
    TopLoc_Location loc;
    bool needMesh = fine;
    if (!fine) {
        const occ::handle<Poly_Triangulation> probe =
            BRep_Tool::Triangulation(face, loc, Poly_MeshPurpose_NONE);
        needMesh = probe.IsNull();
    }
    if (needMesh) {
        BRepMesh_IncrementalMesh mesher(face, fine ? SWEEP_LIN_DEFLECTION : SWEEP_CLASSIFY_LIN,
                                        false,
                                        fine ? SWEEP_ANG_DEFLECTION : SWEEP_CLASSIFY_ANG, false);
    }
    const occ::handle<Poly_Triangulation> tri =
        BRep_Tool::Triangulation(face, loc, Poly_MeshPurpose_NONE);
    if (tri.IsNull()) return false;
    const gp_Trsf trsf = loc.Transformation();
    out.clear();
    for (int t = 1; t <= tri->NbTriangles(); ++t) {
        const Poly_Triangle tr = tri->Triangle(t);
        SweepTri st;
        for (int k = 0; k < 3; ++k) {
            const gp_Pnt p = tri->Node(tr.Value(k + 1)).Transformed(trsf);
            st.p[k][0] = p.X();
            st.p[k][1] = p.Y();
            st.p[k][2] = p.Z();
        }
        out.push_back(st);
    }
    return true;
}

FaceClass sweepClassify(const std::vector<SweepTri>& tris, const double dir[3]) {
    bool pos = false, neg = false;
    for (const SweepTri& t : tris) {
        double n[3];
        sweepTriNormal(t, n);
        const double len = jsHypot3(n[0], n[1], n[2]);
        if (!(len > 0)) continue;
        const double d = (n[0] * dir[0] + n[1] * dir[1] + n[2] * dir[2]) / len;
        if (d > SWEEP_PARALLEL_TOL) pos = true;
        else if (d < -SWEEP_PARALLEL_TOL) neg = true;
        if (pos && neg) return FaceClass::kMixed;
    }
    if (!pos && !neg) return FaceClass::kParallel;
    return FaceClass::kMonotone;
}

double sweepClosedVolume(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props, true, false, false);
    return props.Mass();
}

TopoDS_Shape sweepFacePrism(const TopoDS_Shape& face, const gp_Vec& vec) {
    try {
        return orientedPrism(face, vec);
    } catch (...) {
        return TopoDS_Shape();
    }
}

TopoDS_Shape sweepTriangleWedge(const SweepTri& tri, const double dir[3], const gp_Vec& vec) {
    double n[3];
    sweepTriNormal(tri, n);
    const double nLen = jsHypot3(n[0], n[1], n[2]);
    if (!(nLen > 1e-12)) return TopoDS_Shape();
    // Triangles parallel to the sweep have a degenerate wedge.
    const double d = std::fabs(n[0] * dir[0] + n[1] * dir[1] + n[2] * dir[2]) / nLen;
    if (d < SWEEP_PARALLEL_TOL) return TopoDS_Shape();
    try {
        const std::vector<gp_Pnt> pts = inflateTriangle(tri, SWEEP_LIN_DEFLECTION * 1.5);
        BRepBuilderAPI_MakeWire wire;
        for (size_t i = 0; i < pts.size(); ++i) {
            BRepBuilderAPI_MakeEdge edge(pts[i], pts[(i + 1) % pts.size()]);
            wire.Add(edge.Edge());
        }
        const TopoDS_Wire w = wire.Wire();
        BRepBuilderAPI_MakeFace maker(w, true);
        const TopoDS_Face face = maker.Face();
        return orientedPrism(face, vec);
    } catch (...) {
        return TopoDS_Shape();
    }
}

}  // namespace kapy_capi
