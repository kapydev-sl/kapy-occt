// services/occt/build/embind/bindings/enums.cpp
//
// The OCCT enumerations the code reads. opencascade.js exposes each as an
// object of named values (`oc.TopAbs_ShapeEnum.TopAbs_FACE`), which embind's
// enum_ mirrors exactly. Only the values the code names are registered; the
// spec (../embind/BINDINGS_SPEC.md) lists them per enum.
//
// TopAbs_ShapeEnum / TopAbs_Orientation live in ../kapy_bindings.cpp next to
// TopoDS_Shape; TopAbs_State in topology.cpp next to the classifier's users.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BOPAlgo_GlueEnum.hxx>
#include <BRepFill_TypeOfContact.hxx>
#include <BRepOffset_Mode.hxx>
#include <ChFi3d_FilletShape.hxx>
#include <Extrema_ExtAlgo.hxx>
#include <Extrema_ExtFlag.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_JoinType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_StepModelType.hxx>

using namespace emscripten;

EMSCRIPTEN_BINDINGS(kapy_occt_enums) {
    enum_<BOPAlgo_GlueEnum>("BOPAlgo_GlueEnum")
        .value("BOPAlgo_GlueOff", BOPAlgo_GlueOff)
        .value("BOPAlgo_GlueShift", BOPAlgo_GlueShift)
        .value("BOPAlgo_GlueFull", BOPAlgo_GlueFull);

    enum_<ChFi3d_FilletShape>("ChFi3d_FilletShape")
        .value("ChFi3d_Rational", ChFi3d_Rational)
        .value("ChFi3d_QuasiAngular", ChFi3d_QuasiAngular)
        .value("ChFi3d_Polynomial", ChFi3d_Polynomial);

    enum_<BRepOffset_Mode>("BRepOffset_Mode")
        .value("BRepOffset_Skin", BRepOffset_Skin)
        .value("BRepOffset_Pipe", BRepOffset_Pipe)
        .value("BRepOffset_RectoVerso", BRepOffset_RectoVerso);

    // KeepContact selector for MakePipeShell's auxiliary-spine SetMode_5
    // (twist extrude). NoContact = rotate the section in place.
    enum_<BRepFill_TypeOfContact>("BRepFill_TypeOfContact")
        .value("BRepFill_NoContact", BRepFill_NoContact)
        .value("BRepFill_Contact", BRepFill_Contact)
        .value("BRepFill_ContactOnBorder", BRepFill_ContactOnBorder);

    // What BRepExtrema_DistShapeShape searches for, and with which
    // algorithm. MIN + Tree is the cheap pairing (see measure.cpp).
    enum_<Extrema_ExtFlag>("Extrema_ExtFlag")
        .value("Extrema_ExtFlag_MIN", Extrema_ExtFlag_MIN)
        .value("Extrema_ExtFlag_MAX", Extrema_ExtFlag_MAX)
        .value("Extrema_ExtFlag_MINMAX", Extrema_ExtFlag_MINMAX);

    enum_<Extrema_ExtAlgo>("Extrema_ExtAlgo")
        .value("Extrema_ExtAlgo_Grad", Extrema_ExtAlgo_Grad)
        .value("Extrema_ExtAlgo_Tree", Extrema_ExtAlgo_Tree);

    enum_<GeomAbs_JoinType>("GeomAbs_JoinType")
        .value("GeomAbs_Arc", GeomAbs_Arc)
        .value("GeomAbs_Tangent", GeomAbs_Tangent)
        .value("GeomAbs_Intersection", GeomAbs_Intersection);

    enum_<GeomAbs_CurveType>("GeomAbs_CurveType")
        .value("GeomAbs_Line", GeomAbs_Line)
        .value("GeomAbs_Circle", GeomAbs_Circle)
        .value("GeomAbs_Ellipse", GeomAbs_Ellipse)
        .value("GeomAbs_Hyperbola", GeomAbs_Hyperbola)
        .value("GeomAbs_Parabola", GeomAbs_Parabola)
        .value("GeomAbs_BezierCurve", GeomAbs_BezierCurve)
        .value("GeomAbs_BSplineCurve", GeomAbs_BSplineCurve)
        .value("GeomAbs_OffsetCurve", GeomAbs_OffsetCurve)
        .value("GeomAbs_OtherCurve", GeomAbs_OtherCurve);

    enum_<GeomAbs_SurfaceType>("GeomAbs_SurfaceType")
        .value("GeomAbs_Plane", GeomAbs_Plane)
        .value("GeomAbs_Cylinder", GeomAbs_Cylinder)
        .value("GeomAbs_Cone", GeomAbs_Cone)
        .value("GeomAbs_Sphere", GeomAbs_Sphere)
        .value("GeomAbs_Torus", GeomAbs_Torus)
        .value("GeomAbs_BezierSurface", GeomAbs_BezierSurface)
        .value("GeomAbs_BSplineSurface", GeomAbs_BSplineSurface)
        .value("GeomAbs_SurfaceOfRevolution", GeomAbs_SurfaceOfRevolution)
        .value("GeomAbs_SurfaceOfExtrusion", GeomAbs_SurfaceOfExtrusion)
        .value("GeomAbs_OffsetSurface", GeomAbs_OffsetSurface)
        .value("GeomAbs_OtherSurface", GeomAbs_OtherSurface);

    enum_<IFSelect_ReturnStatus>("IFSelect_ReturnStatus")
        .value("IFSelect_RetVoid", IFSelect_RetVoid)
        .value("IFSelect_RetDone", IFSelect_RetDone)
        .value("IFSelect_RetError", IFSelect_RetError)
        .value("IFSelect_RetFail", IFSelect_RetFail)
        .value("IFSelect_RetStop", IFSelect_RetStop);

    enum_<STEPControl_StepModelType>("STEPControl_StepModelType")
        .value("STEPControl_AsIs", STEPControl_AsIs)
        .value("STEPControl_ManifoldSolidBrep", STEPControl_ManifoldSolidBrep)
        .value("STEPControl_ShellBasedSurfaceModel", STEPControl_ShellBasedSurfaceModel)
        .value("STEPControl_GeometricCurveSet", STEPControl_GeometricCurveSet);
}
