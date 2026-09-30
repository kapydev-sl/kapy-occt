// services/occt/build/embind/bindings/exactRegistry.cpp
//
// The registry of exactRegistry.hxx: every class the binary shape set writes,
// by exact class, with how its frame travels. The list is what OCCT 8.0's
// `BinTools_SurfaceSet` / `BinTools_CurveSet` / `BinTools_Curve2dSet` write;
// a kernel bump that adds one is refused until it is added here, with its
// record in exactGeometry.cpp if it has a frame.
//
// Who includes this: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the frames themselves (exactGeometry.cpp).

#include "exactRegistry.hxx"

#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_BezierCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_Hyperbola.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_OffsetCurve.hxx>
#include <Geom2d_Parabola.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_Circle.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_Hyperbola.hxx>
#include <Geom_Line.hxx>
#include <Geom_OffsetCurve.hxx>
#include <Geom_OffsetSurface.hxx>
#include <Geom_Parabola.hxx>
#include <Geom_Plane.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_SurfaceOfLinearExtrusion.hxx>
#include <Geom_SurfaceOfRevolution.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <Geom_TrimmedCurve.hxx>

#include <utility>
#include <vector>

namespace kapy_exact {

namespace {

using Entry = std::pair<occ::handle<Standard_Type>, Kind>;

const std::vector<Entry>& registry(Table table) {
    static const std::vector<Entry> surfaces = {
        {STANDARD_TYPE(Geom_Plane), FRAME},
        {STANDARD_TYPE(Geom_CylindricalSurface), FRAME},
        {STANDARD_TYPE(Geom_ConicalSurface), FRAME},
        {STANDARD_TYPE(Geom_SphericalSurface), FRAME},
        {STANDARD_TYPE(Geom_ToroidalSurface), FRAME},
        {STANDARD_TYPE(Geom_SurfaceOfLinearExtrusion), EXTRUSION},
        {STANDARD_TYPE(Geom_SurfaceOfRevolution), REVOLUTION},
        {STANDARD_TYPE(Geom_OffsetSurface), OFFSET},
        {STANDARD_TYPE(Geom_RectangularTrimmedSurface), TRIMMED},
        {STANDARD_TYPE(Geom_BezierSurface), NONE},
        {STANDARD_TYPE(Geom_BSplineSurface), NONE},
    };
    static const std::vector<Entry> curves = {
        {STANDARD_TYPE(Geom_Line), FRAME},
        {STANDARD_TYPE(Geom_Circle), CONIC},
        {STANDARD_TYPE(Geom_Ellipse), CONIC},
        {STANDARD_TYPE(Geom_Hyperbola), CONIC},
        {STANDARD_TYPE(Geom_Parabola), CONIC},
        {STANDARD_TYPE(Geom_TrimmedCurve), TRIMMED},
        {STANDARD_TYPE(Geom_OffsetCurve), OFFSET},
        {STANDARD_TYPE(Geom_BezierCurve), NONE},
        {STANDARD_TYPE(Geom_BSplineCurve), NONE},
    };
    static const std::vector<Entry> curves2d = {
        {STANDARD_TYPE(Geom2d_Line), FRAME},
        {STANDARD_TYPE(Geom2d_Circle), CONIC},
        {STANDARD_TYPE(Geom2d_Ellipse), CONIC},
        {STANDARD_TYPE(Geom2d_Hyperbola), CONIC},
        {STANDARD_TYPE(Geom2d_Parabola), CONIC},
        {STANDARD_TYPE(Geom2d_TrimmedCurve), TRIMMED},
        {STANDARD_TYPE(Geom2d_OffsetCurve), OFFSET},
        {STANDARD_TYPE(Geom2d_BezierCurve), NONE},
        {STANDARD_TYPE(Geom2d_BSplineCurve), NONE},
    };
    return table == SURFACE ? surfaces : table == CURVE ? curves : curves2d;
}

std::string& excluded() {
    static std::string name;
    return name;
}

} // namespace

int registeredKind(Table table, const occ::handle<Standard_Type>& type) {
    if (type.IsNull()) return -1;
    if (!excluded().empty() && excluded() == type->Name()) return -1;
    for (const Entry& e : registry(table)) {
        if (e.first == type) return e.second;
    }
    return -1;
}

void excludeForTest(const std::string& name) {
    excluded() = name;
}

} // namespace kapy_exact
