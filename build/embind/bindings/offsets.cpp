// services/occt/build/embind/bindings/offsets.cpp
//
// Sweeps and offsets: pipes, pipe shells, thick solids (shell) and lofts.
//
// `BRepOffsetAPI_MakePipe::Generated` is overloaded in OCCT 8.0 — `_1` takes
// one shape and returns the history list, `_2` takes spine+profile and
// returns a shape. The sweep role assignment calls `Generated_1`, so the
// number matters here; ../compare-methods.py confirms it did not shift.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepBuilderAPI_MakeShape.hxx>

#include <BRepFill_TypeOfContact.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Message_ProgressRange.hxx>
#include <NCollection_List.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>

using namespace emscripten;

using ShapeList = NCollection_List<TopoDS_Shape>;

template <typename T>
static TopoDS_Shape sw_Shape(T& t) {
    return t.Shape();
}
template <typename T>
static bool sw_IsDone(T& t) {
    return t.IsDone();
}
template <typename T>
static ShapeList sw_Modified(T& t, const TopoDS_Shape& s) {
    return t.Modified(s);
}
template <typename T>
static TopoDS_Shape sw_FirstShape(T& t) {
    return t.FirstShape();
}
template <typename T>
static TopoDS_Shape sw_LastShape(T& t) {
    return t.LastShape();
}
template <typename T>
static void sw_Build(T& t, const Message_ProgressRange& r) {
    t.Build(r);
}
// Shape history: the TopoNamer needs all three on every sweep, not just
// Modified. Omitting one does not fail the build — it fails at runtime with
// `op.Generated is not a function`, which the app reports as a geometry error.
template <typename T>
static ShapeList sw_Generated(T& t, const TopoDS_Shape& s) {
    return t.Generated(s);
}
template <typename T>
static bool sw_IsDeleted(T& t, const TopoDS_Shape& s) {
    return t.IsDeleted(s);
}

// MakePipe: `_1` is the history list, `_2` the spine/profile lookup.
static ShapeList Pipe_Generated_1(BRepOffsetAPI_MakePipe& p, const TopoDS_Shape& s) {
    return p.Generated(s);
}
struct BRepOffsetAPI_MakePipe_1 : public BRepOffsetAPI_MakePipe {
    BRepOffsetAPI_MakePipe_1(const TopoDS_Wire& spine, const TopoDS_Shape& profile)
        : BRepOffsetAPI_MakePipe(spine, profile) {}
};

// MakePipeShell: SetMode is overloaded five ways; `_3` takes a bi-normal
// direction. Add_1 places a profile, Add_2 pins it to a vertex.
static void Shell_SetMode_3(BRepOffsetAPI_MakePipeShell& s, const gp_Dir& biNormal) {
    s.SetMode(biNormal);
}
// `_5` drives the section's orientation from an auxiliary spine: at each
// station the frame is built so a reference direction points from the main
// spine to the aux spine. A helical aux spine coaxial with a straight main
// spine therefore rotates the section linearly along the sweep — the twist
// extrude. KeepContact stays NoContact so the section rotates in place
// (isn't translated toward the aux spine). Matches opencascade.js's
// SetMode_5(AuxiliarySpine, CurvilinearEquivalence, KeepContact).
static void Shell_SetMode_5(BRepOffsetAPI_MakePipeShell& s, const TopoDS_Wire& auxSpine,
                            bool curvilinearEquivalence, BRepFill_TypeOfContact keepContact) {
    s.SetMode(auxSpine, curvilinearEquivalence, keepContact);
}
static void Shell_Add_1(BRepOffsetAPI_MakePipeShell& s, const TopoDS_Shape& profile,
                        bool withContact, bool withCorrection) {
    s.Add(profile, withContact, withCorrection);
}
static void Shell_Add_2(BRepOffsetAPI_MakePipeShell& s, const TopoDS_Shape& profile,
                        const TopoDS_Vertex& location, bool withContact, bool withCorrection) {
    s.Add(profile, location, withContact, withCorrection);
}
static ShapeList Shell_Generated(BRepOffsetAPI_MakePipeShell& s, const TopoDS_Shape& x) {
    return s.Generated(x);
}

static void Thick_MakeThickSolidByJoin(BRepOffsetAPI_MakeThickSolid& t, const TopoDS_Shape& s,
                                       const ShapeList& closingFaces, double offset, double tol,
                                       BRepOffset_Mode mode, bool intersection, bool selfInter,
                                       GeomAbs_JoinType join, bool removeIntEdges,
                                       const Message_ProgressRange& range) {
    t.MakeThickSolidByJoin(s, closingFaces, offset, tol, mode, intersection, selfInter, join,
                           removeIntEdges, range);
}
// Whole-solid uniform offset with NO faces removed (Offset Body / the
// clearance grow). Unlike ByJoin with an empty opening list, this keeps
// the solid orientation correct, so the result works as a boolean tool.
static void Thick_MakeThickSolidBySimple(BRepOffsetAPI_MakeThickSolid& t, const TopoDS_Shape& s,
                                         double offset) {
    t.MakeThickSolidBySimple(s, offset);
}
static ShapeList Thick_Modified(BRepOffsetAPI_MakeThickSolid& t, const TopoDS_Shape& s) {
    return t.Modified(s);
}

// BRepOffset_MakeOffset — per-face offset (Offset Faces tool). Unlike the
// BRepOffsetAPI_* algos it is NOT a BRepBuilderAPI_MakeShape, so it gets a
// plain class binding. Usage: Initialize(shape, globalOffset=0, ...), then
// SetOffsetOnFace(face, d) on each picked face (unset faces stay at 0),
// MakeOffsetShape(), Shape(). The trailing Message_ProgressRange on
// Initialize / MakeOffsetShape in OCCT 8.0 is defaulted, so it is omitted
// here. Names are chosen freely (this class is not in opencascade.js, so
// there is no `_N` numbering to match) and mirror runOffsetFaces.ts.
static void Off_Initialize(BRepOffset_MakeOffset& o, const TopoDS_Shape& s, double offset,
                           double tol, BRepOffset_Mode mode, bool intersection, bool selfInter,
                           GeomAbs_JoinType join, bool thickening, bool removeIntEdges) {
    o.Initialize(s, offset, tol, mode, intersection, selfInter, join, thickening, removeIntEdges);
}
static void Off_SetOffsetOnFace(BRepOffset_MakeOffset& o, const TopoDS_Face& f, double off) {
    o.SetOffsetOnFace(f, off);
}
static void Off_MakeOffsetShape(BRepOffset_MakeOffset& o) { o.MakeOffsetShape(); }
static bool Off_IsDone(BRepOffset_MakeOffset& o) { return o.IsDone(); }
static TopoDS_Shape Off_Shape(BRepOffset_MakeOffset& o) { return o.Shape(); }

EMSCRIPTEN_BINDINGS(kapy_occt_offsets) {
    class_<BRepOffsetAPI_MakePipe, base<BRepBuilderAPI_MakeShape>>("BRepOffsetAPI_MakePipe")
        .function("Shape", &sw_Shape<BRepOffsetAPI_MakePipe>)
        .function("IsDone", &sw_IsDone<BRepOffsetAPI_MakePipe>)
        .function("Modified", &sw_Modified<BRepOffsetAPI_MakePipe>)
        .function("IsDeleted", &sw_IsDeleted<BRepOffsetAPI_MakePipe>)
        .function("Generated_1", &Pipe_Generated_1)
        .function("FirstShape", &sw_FirstShape<BRepOffsetAPI_MakePipe>)
        .function("LastShape", &sw_LastShape<BRepOffsetAPI_MakePipe>);
    class_<BRepOffsetAPI_MakePipe_1, base<BRepOffsetAPI_MakePipe>>("BRepOffsetAPI_MakePipe_1")
        .constructor<const TopoDS_Wire&, const TopoDS_Shape&>();

    class_<BRepOffsetAPI_MakePipeShell, base<BRepBuilderAPI_MakeShape>>("BRepOffsetAPI_MakePipeShell")
        .constructor<const TopoDS_Wire&>()
        .function("SetMode_3", &Shell_SetMode_3)
        .function("SetMode_5", &Shell_SetMode_5)
        .function("SetForceApproxC1", &BRepOffsetAPI_MakePipeShell::SetForceApproxC1)
        .function("Add_1", &Shell_Add_1)
        .function("Add_2", &Shell_Add_2)
        .function("IsReady", &BRepOffsetAPI_MakePipeShell::IsReady)
        .function("MakeSolid", &BRepOffsetAPI_MakePipeShell::MakeSolid)
        .function("Build", &sw_Build<BRepOffsetAPI_MakePipeShell>)
        .function("Shape", &sw_Shape<BRepOffsetAPI_MakePipeShell>)
        .function("IsDone", &sw_IsDone<BRepOffsetAPI_MakePipeShell>)
        .function("Modified", &sw_Modified<BRepOffsetAPI_MakePipeShell>)
        .function("IsDeleted", &sw_IsDeleted<BRepOffsetAPI_MakePipeShell>)
        .function("Generated", &Shell_Generated)
        .function("FirstShape", &sw_FirstShape<BRepOffsetAPI_MakePipeShell>)
        .function("LastShape", &sw_LastShape<BRepOffsetAPI_MakePipeShell>);

    class_<BRepOffsetAPI_MakeThickSolid, base<BRepBuilderAPI_MakeShape>>("BRepOffsetAPI_MakeThickSolid")
        .constructor<>()
        .function("MakeThickSolidByJoin", &Thick_MakeThickSolidByJoin)
        .function("MakeThickSolidBySimple", &Thick_MakeThickSolidBySimple)
        .function("Build", &sw_Build<BRepOffsetAPI_MakeThickSolid>)
        .function("Shape", &sw_Shape<BRepOffsetAPI_MakeThickSolid>)
        .function("IsDone", &sw_IsDone<BRepOffsetAPI_MakeThickSolid>)
        .function("Modified", &Thick_Modified)
        .function("Generated", &sw_Generated<BRepOffsetAPI_MakeThickSolid>)
        .function("IsDeleted", &sw_IsDeleted<BRepOffsetAPI_MakeThickSolid>);

    class_<BRepOffset_MakeOffset>("BRepOffset_MakeOffset")
        .constructor<>()
        .function("Initialize", &Off_Initialize)
        .function("SetOffsetOnFace", &Off_SetOffsetOnFace)
        .function("MakeOffsetShape", &Off_MakeOffsetShape)
        .function("IsDone", &Off_IsDone)
        .function("Shape", &Off_Shape);

    class_<BRepOffsetAPI_ThruSections, base<BRepBuilderAPI_MakeShape>>("BRepOffsetAPI_ThruSections")
        .constructor<bool, bool, double>()
        .function("AddWire", &BRepOffsetAPI_ThruSections::AddWire)
        .function("Build", &sw_Build<BRepOffsetAPI_ThruSections>)
        .function("Shape", &sw_Shape<BRepOffsetAPI_ThruSections>)
        .function("IsDone", &sw_IsDone<BRepOffsetAPI_ThruSections>)
        .function("Modified", &sw_Modified<BRepOffsetAPI_ThruSections>)
        .function("Generated", &sw_Generated<BRepOffsetAPI_ThruSections>)
        .function("IsDeleted", &sw_IsDeleted<BRepOffsetAPI_ThruSections>)
        .function("FirstShape", &sw_FirstShape<BRepOffsetAPI_ThruSections>)
        .function("LastShape", &sw_LastShape<BRepOffsetAPI_ThruSections>);
}
