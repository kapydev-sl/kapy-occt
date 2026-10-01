// services/occt/build/embind/bindings/capiGuards.cpp
//
// The offset-family gates (see capiGuards.hxx), written to make the same OCCT
// calls in the same order as `healShape.ts` and `solidGuards.ts`.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: running an offset, naming a result.

#include "capiGuards.hxx"

#include <cmath>

#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Message_ProgressRange.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeFix_ShapeTolerance.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>

#include "capiFinish.hxx"

namespace {

// Relative agreement between the volume a shell encloses and the solid built
// from it, and the relative change that counts as "the body changed".
constexpr double SOLID_VOLUME_REL_TOL = 1e-3;

}  // namespace

namespace kapy_capi {

TopoDS_Shape healShape(const TopoDS_Shape& shape) {
    try {
        const Message_ProgressRange range;
        opencascade::handle<ShapeFix_Shape> fix = new ShapeFix_Shape(shape);
        fix->Perform(range);
        const TopoDS_Shape fixed = fix->Shape();
        if (fixed.IsNull() || !isValid(fixed)) return TopoDS_Shape();
        return fixed;
    } catch (...) {
        return TopoDS_Shape();
    }
}

TopoDS_Shape validOrHealed(const TopoDS_Shape& shape) {
    if (isValid(shape)) return shape;
    return healShape(shape);
}

bool limitTolerance(const TopoDS_Shape& shape, double tmax) {
    try {
        ShapeFix_ShapeTolerance fix;
        fix.LimitTolerance(shape, 0, tmax, TopAbs_SHAPE);
        return true;
    } catch (...) {
        return false;
    }
}

size_t countSubShapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind) {
    size_t n = 0;
    for (TopExp_Explorer it(shape, kind, TopAbs_SHAPE); it.More(); it.Next()) ++n;
    return n;
}

double signedVolume(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props, false, false, false);
    return props.Mass();
}

TopoDS_Shape ensureSolid(const TopoDS_Shape& shape) {
    if (countSubShapes(shape, TopAbs_SOLID) > 0) return shape;
    try {
        TopTools_IndexedMapOfShape shells;
        TopExp::MapShapes(shape, TopAbs_SHELL, shells);
        if (shells.Extent() == 0) return TopoDS_Shape();
        const double enclosed = std::fabs(signedVolume(shape));
        if (!(enclosed > 0)) return TopoDS_Shape();
        BRepBuilderAPI_MakeSolid maker;
        for (int i = 1; i <= shells.Extent(); ++i) maker.Add(TopoDS::Shell(shells.FindKey(i)));
        TopoDS_Shape solid = maker.Solid();
        if (signedVolume(solid) < 0) solid = solid.Reversed();
        const double volume = signedVolume(solid);
        const bool agrees = std::fabs(volume - enclosed) <= SOLID_VOLUME_REL_TOL * enclosed;
        if (!agrees || !isValid(solid)) return TopoDS_Shape();
        return solid;
    } catch (...) {
        return TopoDS_Shape();
    }
}

bool changedSolidVolume(const TopoDS_Shape& result, double inputVolume) {
    if (countSubShapes(result, TopAbs_SOLID) == 0) return false;
    const double v = signedVolume(result);
    if (!(v > 0)) return false;
    return std::fabs(v - std::fabs(inputVolume)) > SOLID_VOLUME_REL_TOL * std::fabs(inputVolume);
}

bool hasDegenerateFace(const TopoDS_Shape& shape) {
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    GProp_GProps props;
    for (int i = 1; i <= faces.Extent(); ++i) {
        BRepGProp::SurfaceProperties(TopoDS::Face(faces.FindKey(i)), props, false, false);
        if (props.Mass() < DEGENERATE_FACE_AREA) return true;
    }
    return false;
}

}  // namespace kapy_capi
