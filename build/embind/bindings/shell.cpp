// services/occt/build/embind/bindings/shell.cpp
//
// Sewing + solid assembly, used by the shell (hollow) operation's micro-wall
// cleanup fallback: a fully-rounded fillet leaves sub-threshold sliver faces
// that make BRepOffsetAPI_MakeThickSolid fail, so runShell drops them, sews
// the survivors and rebuilds the solid. See runShell.helpers.ts (cleanMicroFaces).
//
// BRepBuilderAPI_Sewing derives from Standard_Transient but the app owns it
// outright (`new` + `.delete()`), never through a handle, so a plain class_
// registration is correct — the same shape opencascade.js exposes.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <Message_ProgressRange.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>

using namespace emscripten;

static void Sewing_Add(BRepBuilderAPI_Sewing& s, const TopoDS_Shape& shape) { s.Add(shape); }
static void Sewing_Perform(BRepBuilderAPI_Sewing& s, const Message_ProgressRange& r) {
    s.Perform(r);
}
static TopoDS_Shape Sewing_SewedShape(BRepBuilderAPI_Sewing& s) { return s.SewedShape(); }
// Free (single-face-owned) edges left after sewing — zero means the shell is
// watertight, which the mesh importer uses to decide solid vs open shell.
static Standard_Integer Sewing_NbFreeEdges(BRepBuilderAPI_Sewing& s) { return s.NbFreeEdges(); }

static void MakeSolid_Add(BRepBuilderAPI_MakeSolid& m, const TopoDS_Shell& sh) { m.Add(sh); }
static TopoDS_Solid MakeSolid_Solid(BRepBuilderAPI_MakeSolid& m) { return m.Solid(); }
static bool MakeSolid_IsDone(BRepBuilderAPI_MakeSolid& m) { return m.IsDone(); }
// Solid straight from a shell (mesh importer). opencascade.js's `_3`.
struct BRepBuilderAPI_MakeSolid_3 : public BRepBuilderAPI_MakeSolid {
    explicit BRepBuilderAPI_MakeSolid_3(const TopoDS_Shell& sh) : BRepBuilderAPI_MakeSolid(sh) {}
};

EMSCRIPTEN_BINDINGS(kapy_occt_shell) {
    class_<BRepBuilderAPI_Sewing>("BRepBuilderAPI_Sewing")
        .constructor<double, bool, bool, bool, bool>()
        .function("Add", &Sewing_Add)
        .function("Perform", &Sewing_Perform)
        .function("SewedShape", &Sewing_SewedShape)
        .function("NbFreeEdges", &Sewing_NbFreeEdges);

    // `Solid()`, `Add(shell)` and `IsDone()` sit on MakeSolid itself; the
    // no-arg ctor is opencascade.js's `_1`, the shell ctor its `_3`.
    class_<BRepBuilderAPI_MakeSolid>("BRepBuilderAPI_MakeSolid_1")
        .constructor<>()
        .function("Add", &MakeSolid_Add)
        .function("IsDone", &MakeSolid_IsDone)
        .function("Solid", &MakeSolid_Solid);
    class_<BRepBuilderAPI_MakeSolid_3, base<BRepBuilderAPI_MakeSolid>>("BRepBuilderAPI_MakeSolid_3")
        .constructor<const TopoDS_Shell&>();
}
