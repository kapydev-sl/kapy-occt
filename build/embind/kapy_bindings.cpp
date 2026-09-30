// services/occt/build/embind/kapy_bindings.cpp
//
// Hand-written Embind registrations for the OCCT subset our worker uses
// (Option C — see ../../README.md). This file is the part opencascade.js
// auto-generates; here we own it, against OCCT 8.0.
//
// SCOPE: this is a STARTER that demonstrates the binding pattern for
// every CATEGORY the codebase needs (value class + overloaded ctors,
// enum, static/namespace functions, builder algo, handle, the Shape
// type). The full class list + the exact ctor overloads / static members
// to register are in ./BINDINGS_SPEC.md (generated from the code by
// ../extract-methods.mjs). Completing it is mechanical: walk the spec,
// mirror a pattern below per class.
//
// ┌──────────────────────────────────────────────────────────────────┐
// │ CRITICAL — overload numbering MUST match opencascade.js.           │
// │ Our TS calls the suffixed names opencascade.js emits (gp_Pnt_3,    │
// │ BRepBuilderAPI_MakeEdge_10, TopoDS.Face_1, ...). Embind does NOT   │
// │ number overloads for you — each name below is chosen BY HAND. To   │
// │ stay a drop-in (no churn in the ~333 call sites), every `_N` here  │
// │ must equal the number opencascade.js assigned for the SAME OCCT    │
// │ signature. opencascade.js numbers overloads in header-declaration  │
// │ order starting at _1; cross-check each against node_modules/       │
// │ opencascade.js/dist/opencascade.full.d.ts (the current 7.7 types)  │
// │ AND the OCCT 8.0 header — if 8.0 added/reordered an overload, the  │
// │ number shifts and you must either preserve the old number or       │
// │ update the call site. This mapping is the main correctness risk of │
// │ Option C; keep it in one place and test it (see README "Verify").  │
// └──────────────────────────────────────────────────────────────────┘

#include <emscripten/bind.h>

#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>

#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopAbs.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopAbs_Orientation.hxx>

#include <BRepBuilderAPI_Transform.hxx>

using namespace emscripten;

// --- factory helpers: embind allows ONE .constructor<> per class, so the
// additional overloads opencascade.js exposes as `Class_N` are registered
// as free functions returning the value. The function NAME carries the
// opencascade.js overload number. Non-copyable classes (the algorithms)
// cannot use this and get a derived struct instead — see bindings/boolean.cpp.

// TopoDS down-casts: our code calls oc.TopoDS.Face_1(shape). In OCCT 8.0
// TopoDS is a NAMESPACE of free functions (it was a class of statics in
// 7.7), so there is nothing to hang class_functions on. An empty tag type
// gives JS the `oc.TopoDS` object opencascade.js exposed, with the same
// member names.
struct TopoDS_Package {};
static TopoDS_Face TopoDS_Face_1(const TopoDS_Shape& s) { return TopoDS::Face(s); }
static TopoDS_Edge TopoDS_Edge_1(const TopoDS_Shape& s) { return TopoDS::Edge(s); }
static TopoDS_Vertex TopoDS_Vertex_1(const TopoDS_Shape& s) { return TopoDS::Vertex(s); }
static TopoDS_Wire TopoDS_Wire_1(const TopoDS_Shape& s) { return TopoDS::Wire(s); }
static TopoDS_Shell TopoDS_Shell_1(const TopoDS_Shape& s) { return TopoDS::Shell(s); }
static TopoDS_Solid TopoDS_Solid_1(const TopoDS_Shape& s) { return TopoDS::Solid(s); }

// `Reversed`/`Moved`/`Located` return by value; `Location` is an overloaded
// getter/setter pair, so it needs disambiguating like `Orientation`.
static TopoDS_Shape Shape_Reversed(const TopoDS_Shape& s) { return s.Reversed(); }
static TopoDS_Shape Shape_Moved(const TopoDS_Shape& s, const TopLoc_Location& l) {
    return s.Moved(l);
}
static TopoDS_Shape Shape_Located(const TopoDS_Shape& s, const TopLoc_Location& l) {
    return s.Located(l);
}
static TopLoc_Location Shape_Location_1(const TopoDS_Shape& s) { return s.Location(); }

EMSCRIPTEN_BINDINGS(kapy_occt) {
    // PATTERN 1 (value class + numbered ctor factories) now lives in
    // bindings/geometry.cpp, together with the rest of the gp_* types.

    // ---- PATTERN 2: enum ----
    // opencascade.js exposes enums as an object of static values, reached
    // as `oc.TopAbs_ShapeEnum.TopAbs_FACE`. Embind enum_ mirrors that.
    enum_<TopAbs_ShapeEnum>("TopAbs_ShapeEnum")
        .value("TopAbs_COMPOUND", TopAbs_COMPOUND)
        .value("TopAbs_SOLID", TopAbs_SOLID)
        .value("TopAbs_SHELL", TopAbs_SHELL)
        .value("TopAbs_FACE", TopAbs_FACE)
        .value("TopAbs_WIRE", TopAbs_WIRE)
        .value("TopAbs_EDGE", TopAbs_EDGE)
        .value("TopAbs_VERTEX", TopAbs_VERTEX)
        .value("TopAbs_SHAPE", TopAbs_SHAPE);
    enum_<TopAbs_Orientation>("TopAbs_Orientation")
        .value("TopAbs_FORWARD", TopAbs_FORWARD)
        .value("TopAbs_REVERSED", TopAbs_REVERSED)
        .value("TopAbs_INTERNAL", TopAbs_INTERNAL)
        .value("TopAbs_EXTERNAL", TopAbs_EXTERNAL);

    // ---- PATTERN 3: the Shape type + its methods ----
    // TopoDS_Shape is passed by handle everywhere; register the members
    // the code calls (Orientation_1, IsNull, ...). See the global method
    // list in BINDINGS_SPEC.md for the full member set.
    // `Orientation` is overloaded (const getter + setter), so the member
    // pointer is ambiguous and must be disambiguated. opencascade.js numbers
    // the getter `_1` because it is declared first; select_overload picks the
    // same one. Every overloaded member below needs this treatment.
    class_<TopoDS_Shape>("TopoDS_Shape")
        .constructor<>()
        .function("IsNull", &TopoDS_Shape::IsNull)
        .function("Orientation_1",
                  select_overload<TopAbs_Orientation() const>(&TopoDS_Shape::Orientation))
        .function("ShapeType", &TopoDS_Shape::ShapeType)
        .function("IsSame", &TopoDS_Shape::IsSame)
        .function("IsEqual", &TopoDS_Shape::IsEqual)
        .function("Reversed", &Shape_Reversed)
        .function("Located", &Shape_Located)
        .function("Location_1", &Shape_Location_1)
        .function("Moved", &Shape_Moved);
    class_<TopoDS_Face, base<TopoDS_Shape>>("TopoDS_Face").constructor<>();
    class_<TopoDS_Edge, base<TopoDS_Shape>>("TopoDS_Edge").constructor<>();
    class_<TopoDS_Vertex, base<TopoDS_Shape>>("TopoDS_Vertex").constructor<>();
    class_<TopoDS_Shell, base<TopoDS_Shape>>("TopoDS_Shell").constructor<>();
    class_<TopoDS_Solid, base<TopoDS_Shape>>("TopoDS_Solid").constructor<>();

    // ---- PATTERN 4: package of static functions ----
    // `oc.TopoDS.Face_1(...)`. class_function on a tag type reproduces the
    // object opencascade.js exposed, so no TS shim is needed.
    class_<TopoDS_Package>("TopoDS")
        .class_function("Face_1", &TopoDS_Face_1)
        .class_function("Edge_1", &TopoDS_Edge_1)
        .class_function("Vertex_1", &TopoDS_Vertex_1)
        .class_function("Shell_1", &TopoDS_Shell_1)
        .class_function("Solid_1", &TopoDS_Solid_1)
        .class_function("Wire_1", &TopoDS_Wire_1);

    // PATTERN 5 (builder algorithm) now lives in bindings/builders.cpp,
    // together with the rest of the BRepBuilderAPI / BRepPrimAPI shapes.

    // PATTERN 6 (package of static readers) now lives in bindings/mesh.cpp,
    // where BRep_Tool sits next to the Poly_* types its Triangulation returns.

    // ======================================================================
    // TODO: remaining classes from BINDINGS_SPEC.md — each maps to one of
    // the six patterns above. Groups still to bind:
    //   gp_*        : gp_Pnt2d, gp_Ax1/2/3, gp_Circ, gp_Elips, gp_Dir2d, gp_Lin2d
    //   TopoDS/Top* : TopoDS_Iterator, TopExp(_Explorer), TopTools_* maps,
    //                 TopLoc_Location, TopAbs_State
    //   BRepBuilderAPI_* : MakeEdge (_3,_8,_10,_12,_30), MakeWire, MakeFace_15,
    //                      Transform_2, Copy_2
    //   BRepPrimAPI_*    : MakePrism_1, MakeRevol_1
    //   BRepAlgoAPI_*    : Fuse_1, Cut_1/_3, Common_1, BOPAlgo_GlueEnum
    //   BRepFilletAPI_*  : MakeFillet, MakeChamfer, ChFi3d_FilletShape
    //   BRepOffsetAPI_*  : MakePipe, MakePipeShell, MakeThickSolid, ThruSections
    //   props/checks     : GProp_GProps, BRepGProp, BRepExtrema_DistShapeShape,
    //                      BRepClass3d_SolidClassifier, BRepCheck_Analyzer
    //   healing          : ShapeFix_Shape, ShapeAnalysis, ShapeUpgrade_UnifySameDomain
    //   mesh/adaptor     : BRepMesh_IncrementalMesh, BRepAdaptor_Curve/Surface,
    //                      BRepTools_WireExplorer, GCPnts_UniformDeflection
    //   I/O              : STEPControl_Reader/Writer, StlAPI, IFSelect_ReturnStatus,
    //                      Message_ProgressRange
    //   HLR              : HLRBRep_Algo, HLRBRep_PolyAlgo, HLRBRep_HLRToShape,
    //                      HLRAlgo_Projector
    //   geom + handles   : Geom_CylindricalSurface, Geom2d_Line/TrimmedCurve,
    //                      Handle_Geom*_*, GeomAbs_* enums
    //   shape-history    : Modified()/Generated()/IsDeleted() on the boolean +
    //                      fillet + unify algos — THE naming-critical methods;
    //                      bind them exactly (propagationSelfTest exercises them)
    // ======================================================================
}
