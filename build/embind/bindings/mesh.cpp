// services/occt/build/embind/bindings/mesh.cpp
//
// Tessellation and curve/surface adaptors: the path from a B-Rep face to the
// triangles the viewport renders, plus the analytic queries the sketch layer
// makes of edges and faces.
//
// Note the types reached only through a handle — Poly_Triangulation,
// Poly_Triangle — never appear as `new oc.X` in the TypeScript, so
// ../extract-symbols.mjs does not list them among the 78 classes. They still
// have to be bound: `BRep_Tool.Triangulation(...).get()` returns one.
//
// BRep_Tool lives here (rather than in ../kapy_bindings.cpp) because its
// Triangulation member is what drags Poly_* in.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Geom_Surface.hxx>
#include <BRepLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_UniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

using namespace emscripten;

using TriangulationHandle = occ::handle<Poly_Triangulation>;

static Poly_Triangulation* Handle_Tri_get(TriangulationHandle& h) { return h.get(); }
static bool Handle_Tri_IsNull(const TriangulationHandle& h) { return h.IsNull(); }
static Poly_Triangle Tri_Triangle(Poly_Triangulation& t, int i) { return t.Triangle(i); }

// `oc.BRep_Tool.Triangulation(face, loc, purpose)` / `.Pnt(v)` / `.Parameter_2(v, e)`.
struct BRep_Tool_Package {};
static gp_Pnt BRep_Tool_Pnt(const TopoDS_Vertex& v) { return BRep_Tool::Pnt(v); }
static double BRep_Tool_Parameter_2(const TopoDS_Vertex& v, const TopoDS_Edge& e) {
    return BRep_Tool::Parameter(v, e);
}
static occ::handle<Geom_Surface> BRep_Tool_Surface_2(const TopoDS_Face& f) {
    return BRep_Tool::Surface(f);
}
static TriangulationHandle BRep_Tool_Triangulation(const TopoDS_Face& f, TopLoc_Location& l,
                                                   int purpose) {
    return BRep_Tool::Triangulation(f, l, static_cast<Poly_MeshPurpose>(purpose));
}

struct BRepMesh_IncrementalMesh_2 : public BRepMesh_IncrementalMesh {
    BRepMesh_IncrementalMesh_2(const TopoDS_Shape& s, double lin, bool relative, double ang,
                               bool inParallel)
        : BRepMesh_IncrementalMesh(s, lin, relative, ang, inParallel) {}
};
static bool Mesh_IsDone(BRepMesh_IncrementalMesh& m) { return m.IsDone(); }

// The adaptors' D0/D1 write through their out-parameters, so the gp_ values
// must be passed by reference from JS.
static void Curve_D0(const BRepAdaptor_Curve& c, double u, gp_Pnt& p) { c.D0(u, p); }
static void Curve_D1(const BRepAdaptor_Curve& c, double u, gp_Pnt& p, gp_Vec& v) { c.D1(u, p, v); }
// GetType / First / LastParameter are inherited from Adaptor3d_Curve, and
// GetType from Adaptor3d_Surface — base member pointers again.
static GeomAbs_CurveType Curve_GetType(const BRepAdaptor_Curve& c) { return c.GetType(); }
static double Curve_FirstParameter(const BRepAdaptor_Curve& c) { return c.FirstParameter(); }
static double Curve_LastParameter(const BRepAdaptor_Curve& c) { return c.LastParameter(); }
static GeomAbs_SurfaceType Surface_GetType(const BRepAdaptor_Surface& s) { return s.GetType(); }
// GeomAdaptor_Surface: the test layer classifies a face's surface (plane vs
// cylinder) straight off its Geom_Surface, without a face restriction.
static GeomAbs_SurfaceType GeomSurf_GetType(const GeomAdaptor_Surface& a) { return a.GetType(); }
struct GeomAdaptor_Surface_2 : public GeomAdaptor_Surface {
    explicit GeomAdaptor_Surface_2(const occ::handle<Geom_Surface>& s) : GeomAdaptor_Surface(s) {}
};
static gp_Circ Curve_Circle(const BRepAdaptor_Curve& c) { return c.Circle(); }
static gp_Lin Curve_Line(const BRepAdaptor_Curve& c) { return c.Line(); }
static gp_Pln Surface_Plane(const BRepAdaptor_Surface& s) { return s.Plane(); }
static gp_Cylinder Surface_Cylinder(const BRepAdaptor_Surface& s) { return s.Cylinder(); }
struct BRepAdaptor_Curve_2 : public BRepAdaptor_Curve {
    explicit BRepAdaptor_Curve_2(const TopoDS_Edge& e) : BRepAdaptor_Curve(e) {}
};
struct BRepAdaptor_Surface_2 : public BRepAdaptor_Surface {
    BRepAdaptor_Surface_2(const TopoDS_Face& f, bool restriction)
        : BRepAdaptor_Surface(f, restriction) {}
};

static void GCPnts_Initialize_3(GCPnts_UniformDeflection& g, const BRepAdaptor_Curve& c,
                                double deflection, double u1, double u2, bool withControl) {
    g.Initialize(c, deflection, u1, u2, withControl);
}

// `oc.BRepLib.BuildCurves3d_2(shape)`.
struct BRepLib_Package {};
static bool BRepLib_BuildCurves3d_2(const TopoDS_Shape& s) { return BRepLib::BuildCurves3d(s); }

EMSCRIPTEN_BINDINGS(kapy_occt_mesh) {
    class_<gp_Lin>("gp_Lin").function("Direction", &gp_Lin::Direction);
    class_<gp_Pln>("gp_Pln").function("Axis", &gp_Pln::Axis).function("Location", &gp_Pln::Location);
    // Plane from an origin point + normal direction (mesh importer). Returned
    // by value like gp_Pnt_3 / gp_Dir_4; `new oc.gp_Pln_3(p, d)` yields it.
    function("gp_Pln_3", +[](const gp_Pnt& p, const gp_Dir& d) { return gp_Pln(p, d); });
    class_<gp_Cylinder>("gp_Cylinder")
        .function("Axis", &gp_Cylinder::Axis)
        .function("Radius", select_overload<double() const>(&gp_Cylinder::Radius));

    class_<Poly_Triangle>("Poly_Triangle")
        .function("Value", select_overload<int(int) const>(&Poly_Triangle::Value));
    class_<Poly_Triangulation>("Poly_Triangulation")
        .function("NbNodes", &Poly_Triangulation::NbNodes)
        .function("NbTriangles", &Poly_Triangulation::NbTriangles)
        .function("Node", &Poly_Triangulation::Node)
        .function("Triangle", &Tri_Triangle);
    class_<TriangulationHandle>("Handle_Poly_Triangulation")
        .function("get", &Handle_Tri_get, allow_raw_pointers())
        .function("IsNull", &Handle_Tri_IsNull);

    class_<BRep_Tool_Package>("BRep_Tool")
        .class_function("Pnt", &BRep_Tool_Pnt)
        .class_function("Parameter_2", &BRep_Tool_Parameter_2)
        .class_function("Surface_2", &BRep_Tool_Surface_2)
        .class_function("Triangulation", &BRep_Tool_Triangulation);

    class_<BRepMesh_IncrementalMesh>("BRepMesh_IncrementalMesh").function("IsDone", &Mesh_IsDone);
    class_<BRepMesh_IncrementalMesh_2, base<BRepMesh_IncrementalMesh>>(
        "BRepMesh_IncrementalMesh_2")
        .constructor<const TopoDS_Shape&, double, bool, double, bool>();

    class_<BRepAdaptor_Curve>("BRepAdaptor_Curve")
        .function("GetType", &Curve_GetType)
        .function("FirstParameter", &Curve_FirstParameter)
        .function("LastParameter", &Curve_LastParameter)
        .function("D0", &Curve_D0)
        .function("D1", &Curve_D1)
        .function("Circle", &Curve_Circle)
        .function("Line", &Curve_Line);
    class_<BRepAdaptor_Curve_2, base<BRepAdaptor_Curve>>("BRepAdaptor_Curve_2")
        .constructor<const TopoDS_Edge&>();

    class_<BRepAdaptor_Surface>("BRepAdaptor_Surface")
        .function("GetType", &Surface_GetType)
        .function("Plane", &Surface_Plane)
        .function("Cylinder", &Surface_Cylinder);
    class_<BRepAdaptor_Surface_2, base<BRepAdaptor_Surface>>("BRepAdaptor_Surface_2")
        .constructor<const TopoDS_Face&, bool>();

    class_<GeomAdaptor_Surface>("GeomAdaptor_Surface").function("GetType", &GeomSurf_GetType);
    class_<GeomAdaptor_Surface_2, base<GeomAdaptor_Surface>>("GeomAdaptor_Surface_2")
        .constructor<const occ::handle<Geom_Surface>&>();

    class_<GCPnts_UniformDeflection>("GCPnts_UniformDeflection")
        .constructor<>()
        .function("Initialize_3", &GCPnts_Initialize_3)
        .function("IsDone", &GCPnts_UniformDeflection::IsDone)
        .function("NbPoints", &GCPnts_UniformDeflection::NbPoints)
        .function("Value", &GCPnts_UniformDeflection::Value);
    function("GCPnts_UniformDeflection_1", +[]() { return GCPnts_UniformDeflection(); });

    class_<BRepLib_Package>("BRepLib").class_function("BuildCurves3d_2", &BRepLib_BuildCurves3d_2);
}
