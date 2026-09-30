// services/occt/build/embind/bindings/bspline.cpp
//
// B-spline curve fitting, for gear involute tooth flanks: a point array,
// GeomAPI_PointsToBSpline (least-squares fit), the Geom_Curve handle
// chain, and the Handle_Geom_Curve overload of MakeEdge. Without these
// the gear extrude falls back to a faceted line polyline (hundreds of
// tiny planar faces); with them each flank is one smooth spline face.
//
// Overload numbering MIRRORS opencascade.js (see kapy_bindings.cpp): the
// TS calls `TColgp_Array1OfPnt_2`, `GeomAPI_PointsToBSpline_2`,
// `Handle_Geom_Curve_2`, `BRepBuilderAPI_MakeEdge_24` — the same suffixes
// opencascade.full.d.ts assigns for these exact OCCT signatures.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Curve.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomAbs_Shape.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <gp_Pnt.hxx>

using namespace emscripten;

template <typename T>
using H = opencascade::handle<T>;

// SetValue is declared on the NCollection_Array1 base, so a
// pointer-to-member would register against a type JS never sees; wrap it
// against TColgp_Array1OfPnt (the type embind exposes).
static void Array1OfPnt_SetValue(TColgp_Array1OfPnt& a, int i, const gp_Pnt& p) {
    a.SetValue(i, p);
}

// Curve() returns Handle(Geom_BSplineCurve); hand JS a fresh handle so
// `.delete()` on it merely drops a reference (see geom.cpp MEMORY note).
static H<Geom_BSplineCurve> PointsToBSpline_Curve(GeomAPI_PointsToBSpline& f) {
    return f.Curve();
}
static bool PointsToBSpline_IsDone(GeomAPI_PointsToBSpline& f) { return f.IsDone(); }

template <typename T>
static T* handle_get(H<T>& h) {
    return h.get();
}

// _24 = the Handle_Geom_Curve overload — turns the fitted spline into an
// edge. A derived struct gives embind a concrete constructor to bind,
// exactly like the other MakeEdge_N in builders.cpp.
struct BRepBuilderAPI_MakeEdge_24 : public BRepBuilderAPI_MakeEdge {
    explicit BRepBuilderAPI_MakeEdge_24(const H<Geom_Curve>& c)
        : BRepBuilderAPI_MakeEdge(c) {}
};

// _2 = the (points, degMin, degMax, continuity, tol) fit.
struct GeomAPI_PointsToBSpline_2 : public GeomAPI_PointsToBSpline {
    GeomAPI_PointsToBSpline_2(const TColgp_Array1OfPnt& pts, int degMin, int degMax,
                             GeomAbs_Shape continuity, double tol3d)
        : GeomAPI_PointsToBSpline(pts, degMin, degMax, continuity, tol3d) {}
};

EMSCRIPTEN_BINDINGS(kapy_occt_bspline) {
    class_<TColgp_Array1OfPnt>("TColgp_Array1OfPnt")
        .function("SetValue", &Array1OfPnt_SetValue);
    function("TColgp_Array1OfPnt_2",
             +[](int lower, int upper) { return TColgp_Array1OfPnt(lower, upper); });

    enum_<GeomAbs_Shape>("GeomAbs_Shape")
        .value("GeomAbs_C0", GeomAbs_C0)
        .value("GeomAbs_G1", GeomAbs_G1)
        .value("GeomAbs_C1", GeomAbs_C1)
        .value("GeomAbs_G2", GeomAbs_G2)
        .value("GeomAbs_C2", GeomAbs_C2)
        .value("GeomAbs_C3", GeomAbs_C3)
        .value("GeomAbs_CN", GeomAbs_CN);

    class_<Geom_Curve>("Geom_Curve");
    class_<Geom_BSplineCurve, base<Geom_Curve>>("Geom_BSplineCurve");

    class_<H<Geom_Curve>>("Handle_Geom_Curve")
        .function("get", &handle_get<Geom_Curve>, allow_raw_pointers());
    class_<H<Geom_BSplineCurve>>("Handle_Geom_BSplineCurve")
        .function("get", &handle_get<Geom_BSplineCurve>, allow_raw_pointers());
    function("Handle_Geom_Curve_2", +[](Geom_Curve* p) { return H<Geom_Curve>(p); },
             allow_raw_pointers());

    class_<GeomAPI_PointsToBSpline>("GeomAPI_PointsToBSpline")
        .function("Curve", &PointsToBSpline_Curve)
        .function("IsDone", &PointsToBSpline_IsDone);
    class_<GeomAPI_PointsToBSpline_2, base<GeomAPI_PointsToBSpline>>("GeomAPI_PointsToBSpline_2")
        .constructor<const TColgp_Array1OfPnt&, int, int, GeomAbs_Shape, double>();

    class_<BRepBuilderAPI_MakeEdge_24, base<BRepBuilderAPI_MakeEdge>>(
        "BRepBuilderAPI_MakeEdge_24")
        .constructor<const H<Geom_Curve>&>();
}
