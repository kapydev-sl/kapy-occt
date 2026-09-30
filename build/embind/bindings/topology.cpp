// services/occt/build/embind/bindings/topology.cpp
//
// Shape traversal and assembly: the explorers our meshing / naming code
// walks with, the compound builder, and the location type.
//
// Every inherited member is wrapped in a free function. A pointer-to-member
// of a BASE class registers against a type JS never sees, and the method
// then goes missing at runtime with no compile error — the single easiest
// way to ship a broken binding.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <TopAbs_State.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Trsf.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>

using namespace emscripten;

// TopExp_Explorer: `Init` is unsuffixed in the 7.7 typings (single overload),
// while `Current`/`More`/`Next` come from the class itself.
struct TopExp_Explorer_2 : public TopExp_Explorer {
    TopExp_Explorer_2(const TopoDS_Shape& s, TopAbs_ShapeEnum toFind, TopAbs_ShapeEnum toAvoid)
        : TopExp_Explorer(s, toFind, toAvoid) {}
};
static TopoDS_Shape Explorer_Current(TopExp_Explorer& e) { return e.Current(); }
static TopoDS_Vertex Explorer_CurrentVertex(BRepTools_WireExplorer& e) { return e.CurrentVertex(); }

// BRepTools_WireExplorer::Init is overloaded (wire, wire+face, ...);
// opencascade.js numbers the single-wire one `_1`.
static void WireExplorer_Init_1(BRepTools_WireExplorer& e, const TopoDS_Wire& w) { e.Init(w); }
static TopoDS_Edge WireExplorer_Current(BRepTools_WireExplorer& e) { return e.Current(); }

struct TopoDS_Iterator_2 : public TopoDS_Iterator {
    TopoDS_Iterator_2(const TopoDS_Shape& s, bool cumOri, bool cumLoc)
        : TopoDS_Iterator(s, cumOri, cumLoc) {}
};
static TopoDS_Shape Iterator_Value(TopoDS_Iterator& i) { return i.Value(); }

// TopoDS_Builder declares MakeCompound/Add; BRep_Builder only inherits them.
static void Builder_MakeCompound(BRep_Builder& b, TopoDS_Compound& c) { b.MakeCompound(c); }
static void Builder_Add(BRep_Builder& b, TopoDS_Shape& s, const TopoDS_Shape& c) { b.Add(s, c); }

EMSCRIPTEN_BINDINGS(kapy_occt_topology) {
    enum_<TopAbs_State>("TopAbs_State")
        .value("TopAbs_IN", TopAbs_IN)
        .value("TopAbs_OUT", TopAbs_OUT)
        .value("TopAbs_ON", TopAbs_ON)
        .value("TopAbs_UNKNOWN", TopAbs_UNKNOWN);

    class_<TopExp_Explorer>("TopExp_Explorer")
        .function("More", &TopExp_Explorer::More)
        .function("Next", &TopExp_Explorer::Next)
        .function("Current", &Explorer_Current);
    class_<TopExp_Explorer_2, base<TopExp_Explorer>>("TopExp_Explorer_2")
        .constructor<const TopoDS_Shape&, TopAbs_ShapeEnum, TopAbs_ShapeEnum>();

    class_<BRepTools_WireExplorer>("BRepTools_WireExplorer")
        .constructor<>()
        .function("Init_1", &WireExplorer_Init_1)
        .function("More", &BRepTools_WireExplorer::More)
        .function("Next", &BRepTools_WireExplorer::Next)
        .function("Current", &WireExplorer_Current)
        .function("CurrentVertex", &Explorer_CurrentVertex);
    function("BRepTools_WireExplorer_1", +[]() { return BRepTools_WireExplorer(); });

    class_<TopoDS_Iterator>("TopoDS_Iterator")
        .function("More", &TopoDS_Iterator::More)
        .function("Next", &TopoDS_Iterator::Next)
        .function("Value", &Iterator_Value);
    class_<TopoDS_Iterator_2, base<TopoDS_Iterator>>("TopoDS_Iterator_2")
        .constructor<const TopoDS_Shape&, bool, bool>();

    // The shape subtypes not already registered next to TopoDS_Shape.
    class_<TopoDS_Compound, base<TopoDS_Shape>>("TopoDS_Compound").constructor<>();
    class_<TopoDS_Wire, base<TopoDS_Shape>>("TopoDS_Wire").constructor<>();

    class_<TopLoc_Location>("TopLoc_Location")
        .constructor<>()
        .function("Transformation", &TopLoc_Location::Transformation);
    function("TopLoc_Location_1", +[]() { return TopLoc_Location(); });

    class_<BRep_Builder>("BRep_Builder")
        .constructor<>()
        .function("MakeCompound", &Builder_MakeCompound)
        .function("Add", &Builder_Add);
}
