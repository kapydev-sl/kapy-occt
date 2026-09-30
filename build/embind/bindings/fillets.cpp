// services/occt/build/embind/bindings/fillets.cpp
//
// Fillets and chamfers. Both are ChFi3d_Builder subclasses whose shape
// history (Modified / Generated / IsDeleted) the TopoNamer consumes, so the
// inherited members are wrapped rather than taken as base member pointers.
//
// `Add` is heavily overloaded on both. In OCCT 8.0's declaration order:
//   MakeFillet::Add  -> _1 (edge), _2 (radius, edge), _3 (r1, r2, edge), ...
//   MakeChamfer::Add -> _1 (edge), _2 (distance, edge), ...
// which matches the 7.7 numbering our call sites use (`op.Add_2(param, edge)`).
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepBuilderAPI_MakeShape.hxx>

#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <ChFi3d_FilletShape.hxx>
#include <Message_ProgressRange.hxx>
#include <NCollection_List.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

using namespace emscripten;

using ShapeList = NCollection_List<TopoDS_Shape>;

template <typename T>
static TopoDS_Shape chfi_Shape(T& t) {
    return t.Shape();
}
template <typename T>
static bool chfi_IsDone(T& t) {
    return t.IsDone();
}
template <typename T>
static void chfi_Build(T& t, const Message_ProgressRange& r) {
    t.Build(r);
}
template <typename T>
static ShapeList chfi_Modified(T& t, const TopoDS_Shape& s) {
    return t.Modified(s);
}
template <typename T>
static ShapeList chfi_Generated(T& t, const TopoDS_Shape& s) {
    return t.Generated(s);
}
template <typename T>
static bool chfi_IsDeleted(T& t, const TopoDS_Shape& s) {
    return t.IsDeleted(s);
}

static void Fillet_Add_1(BRepFilletAPI_MakeFillet& f, const TopoDS_Edge& e) { f.Add(e); }
static void Fillet_Add_2(BRepFilletAPI_MakeFillet& f, double r, const TopoDS_Edge& e) {
    f.Add(r, e);
}
static void Chamfer_Add_1(BRepFilletAPI_MakeChamfer& c, const TopoDS_Edge& e) { c.Add(e); }
static void Chamfer_Add_2(BRepFilletAPI_MakeChamfer& c, double d, const TopoDS_Edge& e) {
    c.Add(d, e);
}

EMSCRIPTEN_BINDINGS(kapy_occt_fillets) {
    class_<BRepFilletAPI_MakeFillet, base<BRepBuilderAPI_MakeShape>>("BRepFilletAPI_MakeFillet")
        .constructor<const TopoDS_Shape&, ChFi3d_FilletShape>()
        .function("Add_1", &Fillet_Add_1)
        .function("Add_2", &Fillet_Add_2)
        .function("Build", &chfi_Build<BRepFilletAPI_MakeFillet>)
        .function("Shape", &chfi_Shape<BRepFilletAPI_MakeFillet>)
        .function("IsDone", &chfi_IsDone<BRepFilletAPI_MakeFillet>)
        .function("Modified", &chfi_Modified<BRepFilletAPI_MakeFillet>)
        .function("Generated", &chfi_Generated<BRepFilletAPI_MakeFillet>)
        .function("IsDeleted", &chfi_IsDeleted<BRepFilletAPI_MakeFillet>);

    class_<BRepFilletAPI_MakeChamfer, base<BRepBuilderAPI_MakeShape>>("BRepFilletAPI_MakeChamfer")
        .constructor<const TopoDS_Shape&>()
        .function("Add_1", &Chamfer_Add_1)
        .function("Add_2", &Chamfer_Add_2)
        .function("Build", &chfi_Build<BRepFilletAPI_MakeChamfer>)
        .function("Shape", &chfi_Shape<BRepFilletAPI_MakeChamfer>)
        .function("IsDone", &chfi_IsDone<BRepFilletAPI_MakeChamfer>)
        .function("Modified", &chfi_Modified<BRepFilletAPI_MakeChamfer>)
        .function("Generated", &chfi_Generated<BRepFilletAPI_MakeChamfer>)
        .function("IsDeleted", &chfi_IsDeleted<BRepFilletAPI_MakeChamfer>);
}
