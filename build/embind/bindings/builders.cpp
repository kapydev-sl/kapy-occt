// services/occt/build/embind/bindings/builders.cpp
//
// The BRepBuilderAPI / BRepPrimAPI shape constructors: edges, wires, faces,
// transforms, copies, boxes, prisms and revolutions.
//
// Everything here derives from BRepBuilderAPI_MakeShape, whose members
// (Shape, IsDone, Build, Modified, Generated, First/LastShape) are declared
// on the BASE. A base member pointer registers against a type JS never sees
// and the method silently vanishes, so `bind_make_shape` wraps them all.
// Non-copyable, so each numbered ctor is a derived struct, not a factory.
//
// BRepBuilderAPI_MakeEdge_30 (Handle_Geom2d_Curve, Handle_Geom_Surface) is
// bound in geom_handles.cpp, where those handle types are registered.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <NCollection_List.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

using namespace emscripten;

using ShapeList = NCollection_List<TopoDS_Shape>;

// Inherited from BRepBuilderAPI_MakeShape.
template <typename T>
static TopoDS_Shape ms_Shape(T& t) {
    return t.Shape();
}
template <typename T>
static bool ms_IsDone(T& t) {
    return t.IsDone();
}
template <typename T>
static void ms_Build(T& t) {
    t.Build();
}
template <typename T>
static ShapeList ms_Modified(T& t, const TopoDS_Shape& s) {
    return t.Modified(s);
}
template <typename T>
static ShapeList ms_Generated(T& t, const TopoDS_Shape& s) {
    return t.Generated(s);
}
template <typename T>
static bool ms_IsDeleted(T& t, const TopoDS_Shape& s) {
    return t.IsDeleted(s);
}
template <typename T>
static class_<T, base<BRepBuilderAPI_MakeShape>> bind_make_shape(const char* name) {
    return class_<T, base<BRepBuilderAPI_MakeShape>>(name)
        .function("IsDeleted", &ms_IsDeleted<T>)
        .function("Shape", &ms_Shape<T>)
        .function("IsDone", &ms_IsDone<T>)
        .function("Build", &ms_Build<T>)
        .function("Modified", &ms_Modified<T>)
        .function("Generated", &ms_Generated<T>);
}

// Inherited from BRepPrimAPI_MakeSweep.
template <typename T>
static TopoDS_Shape sweep_FirstShape(T& t) {
    return t.FirstShape();
}
template <typename T>
static TopoDS_Shape sweep_LastShape(T& t) {
    return t.LastShape();
}

// --- numbered constructors, as derived structs (non-copyable classes) ---

struct BRepBuilderAPI_MakeEdge_3 : public BRepBuilderAPI_MakeEdge {
    BRepBuilderAPI_MakeEdge_3(const gp_Pnt& a, const gp_Pnt& b) : BRepBuilderAPI_MakeEdge(a, b) {}
};
struct BRepBuilderAPI_MakeEdge_8 : public BRepBuilderAPI_MakeEdge {
    explicit BRepBuilderAPI_MakeEdge_8(const gp_Circ& c) : BRepBuilderAPI_MakeEdge(c) {}
};
struct BRepBuilderAPI_MakeEdge_10 : public BRepBuilderAPI_MakeEdge {
    BRepBuilderAPI_MakeEdge_10(const gp_Circ& c, const gp_Pnt& p1, const gp_Pnt& p2)
        : BRepBuilderAPI_MakeEdge(c, p1, p2) {}
};
struct BRepBuilderAPI_MakeEdge_12 : public BRepBuilderAPI_MakeEdge {
    explicit BRepBuilderAPI_MakeEdge_12(const gp_Elips& e) : BRepBuilderAPI_MakeEdge(e) {}
};
struct BRepBuilderAPI_MakeFace_15 : public BRepBuilderAPI_MakeFace {
    BRepBuilderAPI_MakeFace_15(const TopoDS_Wire& w, bool onlyPlane)
        : BRepBuilderAPI_MakeFace(w, onlyPlane) {}
};
// A planar face from a fitted plane + its outer wire (mesh importer): the
// plane pins the surface so co-planar-but-imperfect boundary points still
// yield a strictly planar face.
struct BRepBuilderAPI_MakeFace_16 : public BRepBuilderAPI_MakeFace {
    BRepBuilderAPI_MakeFace_16(const gp_Pln& p, const TopoDS_Wire& w, bool inside)
        : BRepBuilderAPI_MakeFace(p, w, inside) {}
};
struct BRepBuilderAPI_MakeWire_1 : public BRepBuilderAPI_MakeWire {
    BRepBuilderAPI_MakeWire_1() : BRepBuilderAPI_MakeWire() {}
};
// Empty polygon builder: the mesh importer appends boundary points with
// Add(gp_Pnt), closes, then reads the wire.
struct BRepBuilderAPI_MakePolygon_1 : public BRepBuilderAPI_MakePolygon {
    BRepBuilderAPI_MakePolygon_1() : BRepBuilderAPI_MakePolygon() {}
};
struct BRepBuilderAPI_Transform_2 : public BRepBuilderAPI_Transform {
    BRepBuilderAPI_Transform_2(const TopoDS_Shape& s, const gp_Trsf& t, bool copy)
        : BRepBuilderAPI_Transform(s, t, copy) {}
};
struct BRepBuilderAPI_Copy_2 : public BRepBuilderAPI_Copy {
    BRepBuilderAPI_Copy_2(const TopoDS_Shape& s, bool copyGeom, bool copyMesh)
        : BRepBuilderAPI_Copy(s, copyGeom, copyMesh) {}
};
struct BRepPrimAPI_MakeBox_2 : public BRepPrimAPI_MakeBox {
    BRepPrimAPI_MakeBox_2(double dx, double dy, double dz) : BRepPrimAPI_MakeBox(dx, dy, dz) {}
};
struct BRepPrimAPI_MakeBox_3 : public BRepPrimAPI_MakeBox {
    BRepPrimAPI_MakeBox_3(const gp_Pnt& p, double dx, double dy, double dz)
        : BRepPrimAPI_MakeBox(p, dx, dy, dz) {}
};
struct BRepPrimAPI_MakeBox_4 : public BRepPrimAPI_MakeBox {
    BRepPrimAPI_MakeBox_4(const gp_Pnt& p1, const gp_Pnt& p2) : BRepPrimAPI_MakeBox(p1, p2) {}
};
struct BRepPrimAPI_MakeCylinder_3 : public BRepPrimAPI_MakeCylinder {
    BRepPrimAPI_MakeCylinder_3(const gp_Ax2& a, double r, double h)
        : BRepPrimAPI_MakeCylinder(a, r, h) {}
};
struct BRepPrimAPI_MakePrism_1 : public BRepPrimAPI_MakePrism {
    BRepPrimAPI_MakePrism_1(const TopoDS_Shape& s, const gp_Vec& v, bool copy, bool canonize)
        : BRepPrimAPI_MakePrism(s, v, copy, canonize) {}
};
struct BRepPrimAPI_MakeRevol_1 : public BRepPrimAPI_MakeRevol {
    BRepPrimAPI_MakeRevol_1(const TopoDS_Shape& s, const gp_Ax1& a, double d, bool copy)
        : BRepPrimAPI_MakeRevol(s, a, d, copy) {}
};

// MakeWire::Add is overloaded (edge, wire, list); `_1` takes an edge and
// `_2` a wire, in header-declaration order.
static void MakeWire_Add_1(BRepBuilderAPI_MakeWire& w, const TopoDS_Edge& e) { w.Add(e); }
static void MakeWire_Add_2(BRepBuilderAPI_MakeWire& w, const TopoDS_Wire& o) { w.Add(o); }
static TopoDS_Wire MakeWire_Wire(BRepBuilderAPI_MakeWire& w) { return w.Wire(); }
static TopoDS_Edge MakeEdge_Edge(BRepBuilderAPI_MakeEdge& e) { return e.Edge(); }
static TopoDS_Face MakeFace_Face(BRepBuilderAPI_MakeFace& f) { return f.Face(); }
// Adds an inner wire (a hole) to the face being built.
static void MakeFace_Add(BRepBuilderAPI_MakeFace& f, const TopoDS_Wire& w) { f.Add(w); }
// MakePolygon::Add is overloaded (gp_Pnt, TopoDS_Vertex); `_1` is the point
// one, declared first.
static void MakePolygon_Add_1(BRepBuilderAPI_MakePolygon& p, const gp_Pnt& pt) { p.Add(pt); }
static void MakePolygon_Close(BRepBuilderAPI_MakePolygon& p) { p.Close(); }
static TopoDS_Wire MakePolygon_Wire(BRepBuilderAPI_MakePolygon& p) { return p.Wire(); }

EMSCRIPTEN_BINDINGS(kapy_occt_builders) {
    // Registered so embind can upcast any algo to it — KapyNamer.propagate
    // (bindings/namer.cpp) takes the base, not each concrete operator.
    class_<BRepBuilderAPI_MakeShape>("BRepBuilderAPI_MakeShape");

    bind_make_shape<BRepBuilderAPI_MakeEdge>("BRepBuilderAPI_MakeEdge")
        .function("Edge", &MakeEdge_Edge);
    class_<BRepBuilderAPI_MakeEdge_3, base<BRepBuilderAPI_MakeEdge>>("BRepBuilderAPI_MakeEdge_3")
        .constructor<const gp_Pnt&, const gp_Pnt&>();
    class_<BRepBuilderAPI_MakeEdge_8, base<BRepBuilderAPI_MakeEdge>>("BRepBuilderAPI_MakeEdge_8")
        .constructor<const gp_Circ&>();
    class_<BRepBuilderAPI_MakeEdge_10, base<BRepBuilderAPI_MakeEdge>>("BRepBuilderAPI_MakeEdge_10")
        .constructor<const gp_Circ&, const gp_Pnt&, const gp_Pnt&>();
    class_<BRepBuilderAPI_MakeEdge_12, base<BRepBuilderAPI_MakeEdge>>("BRepBuilderAPI_MakeEdge_12")
        .constructor<const gp_Elips&>();

    bind_make_shape<BRepBuilderAPI_MakeWire>("BRepBuilderAPI_MakeWire")
        .function("Add_1", &MakeWire_Add_1)
        .function("Add_2", &MakeWire_Add_2)
        .function("Wire", &MakeWire_Wire);
    class_<BRepBuilderAPI_MakeWire_1, base<BRepBuilderAPI_MakeWire>>("BRepBuilderAPI_MakeWire_1")
        .constructor<>();

    bind_make_shape<BRepBuilderAPI_MakeFace>("BRepBuilderAPI_MakeFace")
        .function("Face", &MakeFace_Face)
        .function("Add", &MakeFace_Add);
    class_<BRepBuilderAPI_MakeFace_15, base<BRepBuilderAPI_MakeFace>>("BRepBuilderAPI_MakeFace_15")
        .constructor<const TopoDS_Wire&, bool>();
    class_<BRepBuilderAPI_MakeFace_16, base<BRepBuilderAPI_MakeFace>>("BRepBuilderAPI_MakeFace_16")
        .constructor<const gp_Pln&, const TopoDS_Wire&, bool>();

    bind_make_shape<BRepBuilderAPI_MakePolygon>("BRepBuilderAPI_MakePolygon")
        .function("Add_1", &MakePolygon_Add_1)
        .function("Close", &MakePolygon_Close)
        .function("Wire", &MakePolygon_Wire);
    class_<BRepBuilderAPI_MakePolygon_1, base<BRepBuilderAPI_MakePolygon>>(
        "BRepBuilderAPI_MakePolygon_1")
        .constructor<>();

    bind_make_shape<BRepBuilderAPI_Transform>("BRepBuilderAPI_Transform");
    class_<BRepBuilderAPI_Transform_2, base<BRepBuilderAPI_Transform>>(
        "BRepBuilderAPI_Transform_2")
        .constructor<const TopoDS_Shape&, const gp_Trsf&, bool>();

    bind_make_shape<BRepBuilderAPI_Copy>("BRepBuilderAPI_Copy");
    class_<BRepBuilderAPI_Copy_2, base<BRepBuilderAPI_Copy>>("BRepBuilderAPI_Copy_2")
        .constructor<const TopoDS_Shape&, bool, bool>();

    bind_make_shape<BRepPrimAPI_MakeBox>("BRepPrimAPI_MakeBox");
    class_<BRepPrimAPI_MakeBox_2, base<BRepPrimAPI_MakeBox>>("BRepPrimAPI_MakeBox_2")
        .constructor<double, double, double>();
    class_<BRepPrimAPI_MakeBox_3, base<BRepPrimAPI_MakeBox>>("BRepPrimAPI_MakeBox_3")
        .constructor<const gp_Pnt&, double, double, double>();
    class_<BRepPrimAPI_MakeBox_4, base<BRepPrimAPI_MakeBox>>("BRepPrimAPI_MakeBox_4")
        .constructor<const gp_Pnt&, const gp_Pnt&>();

    bind_make_shape<BRepPrimAPI_MakeCylinder>("BRepPrimAPI_MakeCylinder");
    class_<BRepPrimAPI_MakeCylinder_3, base<BRepPrimAPI_MakeCylinder>>(
        "BRepPrimAPI_MakeCylinder_3")
        .constructor<const gp_Ax2&, double, double>();

    bind_make_shape<BRepPrimAPI_MakePrism>("BRepPrimAPI_MakePrism")
        .function("FirstShape", &sweep_FirstShape<BRepPrimAPI_MakePrism>)
        .function("LastShape", &sweep_LastShape<BRepPrimAPI_MakePrism>);
    class_<BRepPrimAPI_MakePrism_1, base<BRepPrimAPI_MakePrism>>("BRepPrimAPI_MakePrism_1")
        .constructor<const TopoDS_Shape&, const gp_Vec&, bool, bool>();

    bind_make_shape<BRepPrimAPI_MakeRevol>("BRepPrimAPI_MakeRevol")
        .function("FirstShape", &sweep_FirstShape<BRepPrimAPI_MakeRevol>)
        .function("LastShape", &sweep_LastShape<BRepPrimAPI_MakeRevol>);
    class_<BRepPrimAPI_MakeRevol_1, base<BRepPrimAPI_MakeRevol>>("BRepPrimAPI_MakeRevol_1")
        .constructor<const TopoDS_Shape&, const gp_Ax1&, double, bool>();
}
