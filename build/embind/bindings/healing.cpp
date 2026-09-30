// services/occt/build/embind/bindings/healing.cpp
//
// Shape repair + simplification, and the first OCCT handle we expose.
//
// THE HANDLE PATTERN. `Handle_X` classes do not exist in OCCT — opencascade.js
// synthesises them around `opencascade::handle<X>`, giving JS a `.get()` that
// returns the pointee and an `.IsNull()`. Our TS relies on that shape
// (unifyFuse.ts: `const history = u.History_1().get()`), so we register the
// handle template instantiation under the same name with the same members.
// `get()` hands back a raw pointer owned by the handle, hence
// allow_raw_pointers(); JS must not delete it, only the handle.
//
// ShapeUpgrade_UnifySameDomain::History is overloaded (const + non-const) in
// OCCT 8.0, so it is numbered — `_1` is the const one, declared first.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepTools_History.hxx>
#include <Message_ProgressRange.hxx>
#include <NCollection_List.hxx>
#include <ShapeAnalysis.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeFix_ShapeTolerance.hxx>
#include <ShapeFix_Solid.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

using namespace emscripten;

using ShapeList = NCollection_List<TopoDS_Shape>;
using HistoryHandle = occ::handle<BRepTools_History>;

static BRepTools_History* Handle_History_get(HistoryHandle& h) { return h.get(); }
static bool Handle_History_IsNull(const HistoryHandle& h) { return h.IsNull(); }

static ShapeList History_Modified(BRepTools_History& h, const TopoDS_Shape& s) {
    return h.Modified(s);
}
static ShapeList History_Generated(BRepTools_History& h, const TopoDS_Shape& s) {
    return h.Generated(s);
}
static bool History_IsRemoved(BRepTools_History& h, const TopoDS_Shape& s) {
    return h.IsRemoved(s);
}

static HistoryHandle Unify_History_1(const ShapeUpgrade_UnifySameDomain& u) { return u.History(); }
static TopoDS_Shape Unify_Shape(const ShapeUpgrade_UnifySameDomain& u) { return u.Shape(); }
struct ShapeUpgrade_UnifySameDomain_2 : public ShapeUpgrade_UnifySameDomain {
    ShapeUpgrade_UnifySameDomain_2(const TopoDS_Shape& s, bool edges, bool faces, bool concat)
        : ShapeUpgrade_UnifySameDomain(s, edges, faces, concat) {}
};

static bool Fix_Perform(ShapeFix_Shape& f, const Message_ProgressRange& r) { return f.Perform(r); }
struct ShapeFix_Shape_2 : public ShapeFix_Shape {
    explicit ShapeFix_Shape_2(const TopoDS_Shape& s) : ShapeFix_Shape(s) {}
};

// Tolerance trimming. An offset or a join-variant thick-solid can hand back a
// valid solid whose vertices carry the escalated tolerance the algorithm needed
// to close it; the next boolean then reads those fat vertices as coincident
// with geometry they are nowhere near. LimitTolerance caps them back down.
// Default arguments do not survive embind, so the wrapper spells them out.
static bool Tolerance_Limit(ShapeFix_ShapeTolerance& t,
                            const TopoDS_Shape& s,
                            double tmin,
                            double tmax) {
    return t.LimitTolerance(s, tmin, tmax, TopAbs_SHAPE);
}

// Shell -> solid. An offset or a thick-solid can hand back a bare shell that
// BRepCheck calls valid, and every boolean against it then fails to converge.
// SolidFromShell wraps it AND orients it from the enclosed volume, which
// BRepBuilderAPI_MakeSolid does not: MakeSolid keeps the shell's own winding,
// so an inward-wound shell becomes an inside-out solid.
static TopoDS_Solid Solid_FromShell(ShapeFix_Solid& f, const TopoDS_Shell& shell) {
    return f.SolidFromShell(shell);
}
static bool Solid_Perform(ShapeFix_Solid& f, const Message_ProgressRange& r) {
    return f.Perform(r);
}
struct ShapeFix_Solid_1 : public ShapeFix_Solid {
    ShapeFix_Solid_1() : ShapeFix_Solid() {}
};

// `oc.ShapeAnalysis.OuterWire(face)`.
struct ShapeAnalysis_Package {};
static TopoDS_Wire Analysis_OuterWire(const TopoDS_Face& f) { return ShapeAnalysis::OuterWire(f); }

EMSCRIPTEN_BINDINGS(kapy_occt_healing) {
    class_<BRepTools_History>("BRepTools_History")
        .function("Modified", &History_Modified)
        .function("Generated", &History_Generated)
        .function("IsRemoved", &History_IsRemoved);

    class_<HistoryHandle>("Handle_BRepTools_History")
        .function("get", &Handle_History_get, allow_raw_pointers())
        .function("IsNull", &Handle_History_IsNull);

    class_<ShapeUpgrade_UnifySameDomain>("ShapeUpgrade_UnifySameDomain")
        .function("Build", &ShapeUpgrade_UnifySameDomain::Build)
        .function("Shape", &Unify_Shape)
        .function("History_1", &Unify_History_1)
        .function("SetSafeInputMode", &ShapeUpgrade_UnifySameDomain::SetSafeInputMode)
        .function("SetLinearTolerance", &ShapeUpgrade_UnifySameDomain::SetLinearTolerance)
        .function("SetAngularTolerance", &ShapeUpgrade_UnifySameDomain::SetAngularTolerance);
    class_<ShapeUpgrade_UnifySameDomain_2, base<ShapeUpgrade_UnifySameDomain>>(
        "ShapeUpgrade_UnifySameDomain_2")
        .constructor<const TopoDS_Shape&, bool, bool, bool>();

    class_<ShapeFix_Shape>("ShapeFix_Shape")
        .function("Perform", &Fix_Perform)
        .function("Shape", &ShapeFix_Shape::Shape);
    class_<ShapeFix_Shape_2, base<ShapeFix_Shape>>("ShapeFix_Shape_2")
        .constructor<const TopoDS_Shape&>();

    class_<ShapeFix_ShapeTolerance>("ShapeFix_ShapeTolerance")
        .constructor<>()
        .function("LimitTolerance", &Tolerance_Limit);

    class_<ShapeFix_Solid>("ShapeFix_Solid")
        .function("SolidFromShell", &Solid_FromShell)
        .function("Init", &ShapeFix_Solid::Init)
        .function("Perform", &Solid_Perform)
        .function("Solid", &ShapeFix_Solid::Solid)
        .function("Shape", &ShapeFix_Solid::Shape);
    class_<ShapeFix_Solid_1, base<ShapeFix_Solid>>("ShapeFix_Solid_1").constructor<>();

    class_<ShapeAnalysis_Package>("ShapeAnalysis").class_function("OuterWire", &Analysis_OuterWire);
}
