// services/occt/build/embind/bindings/capiMeasure.cpp
//
// The whole-shape measures of the C API: surface area, volume with centre of
// mass, and whether the B-Rep passes BRepCheck. Each is the binding's own
// call (binding/runShapeMeasure.ts, runShapeChecks.ts), so the numbers are the
// ones the JSON transport has always given:
//   area       BRepGProp::SurfaceProperties(shape, props, false, false), Mass()
//   mass       BRepGProp::VolumeProperties(shape, props, true, false, false),
//              |Mass()| and CentreOfMass()
//   is valid   BRepCheck_Analyzer(shape, true, false).IsValid()
//
// Blob: u32 handle. Answers: area one f64; mass four f64 (volume, x, y, z);
// is-valid one byte.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the store, the arena, contact and classification.

#include <cmath>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <gp_Pnt.hxx>

#include "capiAsk.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('getShapeArea'), ('getShapeMassProps'), ('getShapeIsValid').
constexpr size_t SEED_AREA = 668715212u;
constexpr size_t SEED_MASS = 368369219u;
constexpr size_t SEED_VALID = 927906397u;
}  // namespace

KAPY_API int32_t kapy_area(uint32_t ptr, uint32_t length) noexcept {
    return ask("getShapeArea", SEED_AREA, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        out.f64(declining("getShapeArea", [&] {
            GProp_GProps props;
            BRepGProp::SurfaceProperties(entry.shape, props, false, false);
            return props.Mass();
        }));
    });
}

KAPY_API int32_t kapy_mass_props(uint32_t ptr, uint32_t length) noexcept {
    return ask("getShapeMassProps", SEED_MASS, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        declining("getShapeMassProps", [&] {
            GProp_GProps props;
            BRepGProp::VolumeProperties(entry.shape, props, true, false, false);
            const gp_Pnt c = props.CentreOfMass();
            out.f64(std::fabs(props.Mass()));
            out.f64(c.X());
            out.f64(c.Y());
            out.f64(c.Z());
            return 0;
        });
    });
}

KAPY_API int32_t kapy_is_valid(uint32_t ptr, uint32_t length) noexcept {
    return ask("getShapeIsValid", SEED_VALID, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        out.u8(declining("getShapeIsValid", [&] {
            BRepCheck_Analyzer analyzer(entry.shape, true, false);
            return analyzer.IsValid() ? 1 : 0;
        }));
    });
}
