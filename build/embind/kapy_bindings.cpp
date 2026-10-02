// services/occt/build/embind/kapy_bindings.cpp
//
// The Embind core of the module: the TopoDS shape types, their enums and the
// `TopoDS` down-casts. Everything else the TypeScript side still touches is
// registered by the domain files under bindings/ (geometry, builders, boolean,
// mesh, ...), and the C API the Rust core uses is declared in
// bindings/kapy_capi.h.
//
// Who builds it: CMakeLists.txt, linked with every bindings/*.cpp.
// What does NOT belong here: a new binding for a class that only Rust needs
// (that is a C API function), or a domain registration (it goes in its own
// bindings/ file).
//
// Overload names keep the numeric suffixes the TypeScript call sites were
// written against (`TopoDS.Face_1`, `Orientation_1`); embind does not number
// overloads, so each name is chosen by hand.

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
// additional overloads are registered as `Class_N` free functions returning
// the value. The function NAME carries the overload number. Non-copyable
// classes (the algorithms) cannot use this and get a derived struct instead;
// see bindings/boolean.cpp.

// TopoDS down-casts: our code calls oc.TopoDS.Face_1(shape). In OCCT 8.0
// TopoDS is a NAMESPACE of free functions (it was a class of statics in
// 7.7), so there is nothing to hang class_functions on. An empty tag type
// gives JS the `oc.TopoDS` object the call sites expect.
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
    // The value classes with numbered ctor factories live in
    // bindings/geometry.cpp, together with the rest of the gp_* types.

    // ---- enums ----
    // Reached from JS as `oc.TopAbs_ShapeEnum.TopAbs_FACE`.
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

    // ---- the Shape type and its methods ----
    // TopoDS_Shape is passed by handle everywhere; register the members
    // the code calls (Orientation_1, IsNull, ...).
    // `Orientation` is overloaded (const getter + setter), so the member
    // pointer is ambiguous and must be disambiguated; the getter is `_1`
    // because it is declared first. Every overloaded member below needs this treatment.
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

    // ---- the TopoDS down-casts ----
    // `oc.TopoDS.Face_1(...)`. class_function on a tag type reproduces the
    // object the call sites expect.
    class_<TopoDS_Package>("TopoDS")
        .class_function("Face_1", &TopoDS_Face_1)
        .class_function("Edge_1", &TopoDS_Edge_1)
        .class_function("Vertex_1", &TopoDS_Vertex_1)
        .class_function("Shell_1", &TopoDS_Shell_1)
        .class_function("Solid_1", &TopoDS_Solid_1)
        .class_function("Wire_1", &TopoDS_Wire_1);

    // The builder algorithms live in bindings/builders.cpp,
    // together with the rest of the BRepBuilderAPI / BRepPrimAPI shapes.

    // The BRep_Tool readers live in bindings/mesh.cpp, next to the Poly_*
    // types its Triangulation returns.
}
