// services/occt/build/embind/bindings/capiOps.cpp
//
// The first operations of the C API: letting go of a handle, taking another
// hold of it, and measuring it. Each one numbers the kernel's serials from the
// seed of the method it stands for (`serialSeedOf(name)` on the TypeScript
// side: FNV-1a of the name), so a measurement made here sees the same
// algorithm state the binding's own call does.
//
// The measurements are the binding's, call for call:
//   volume  BRepGProp::VolumeProperties(shape, props, onlyClosed = true,
//           skipShared = false, useTriangulation = false), |Mass()|
//           (binding/runShapeChecks.ts)
//   bounds  Bnd_Box + BRepBndLib::Add(shape, box, useTriangulation = false);
//           void is an empty answer (binding/runShapeBounds.ts)
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the store, the arena, the embind registrations.

#include <cmath>
#include <cstring>
#include <vector>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Transient.hxx>
#include <gp_Pnt.hxx>

#include "capiState.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {
// serialSeedOf('getShapeVolume'), ('getShapeBounds'), ('releaseShapes').
constexpr size_t SEED_VOLUME = 1473126409u;
constexpr size_t SEED_BOUNDS = 1744534848u;
constexpr size_t SEED_RELEASE = 305289564u;

// What the red control makes the kernel lie about (see `Kapy_CapiPerturbForTest`):
// mode 1 reports every shape as having no volume, which a cut reads as "the
// tool did nothing"; mode 2 pushes the far corner of every box out by a metre,
// which any distance measured to the far side reads as a different length. A
// lie small enough to round away would prove nothing about the judge.
constexpr double PERTURB_SHIFT = 1000.0;
}  // namespace

KAPY_API int32_t kapy_release(uint32_t ptr, uint32_t count) noexcept {
    begin();
    if (count != 0 && ptr == 0) return fail(KAPY_E_BAD_ARG, "release: no handles at ptr");
    Standard_Transient::SetSerialCounter(SEED_RELEASE);
    const uint32_t* handles = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(ptr));
    for (uint32_t i = 0; i < count; i++) {
        uint32_t handle = 0;
        std::memcpy(&handle, handles + i, sizeof(handle));
        release(handle, true);
    }
    return answerNothing();
}

KAPY_API int32_t kapy_retain(uint32_t handle) noexcept {
    begin();
    const uint32_t count = retain(handle);
    if (count == 0) return fail(KAPY_E_UNKNOWN_HANDLE, "retain: OCCT shape handle not found");
    return static_cast<int32_t>(count);
}

KAPY_API int32_t kapy_volume(uint32_t handle) noexcept {
    begin();
    Entry* entry = find(handle);
    if (!entry) return fail(KAPY_E_UNKNOWN_HANDLE, "OCCT shape handle not found");
    try {
        Standard_Transient::SetSerialCounter(SEED_VOLUME);
        GProp_GProps props;
        BRepGProp::VolumeProperties(entry->shape, props, true, false, false);
        double volume = std::fabs(props.Mass());
        if (perturbation() == 1) volume = 0.0;
        return answer(&volume, sizeof(volume));
    } catch (const Standard_Failure& failure) {
        return fail(KAPY_E_FAILED, failure.GetMessageString());
    } catch (...) {
        return fail(KAPY_E_FAILED, "getShapeVolume: unknown failure");
    }
}

KAPY_API int32_t kapy_bounds(uint32_t handle) noexcept {
    begin();
    Entry* entry = find(handle);
    if (!entry) return fail(KAPY_E_UNKNOWN_HANDLE, "OCCT shape handle not found");
    try {
        Standard_Transient::SetSerialCounter(SEED_BOUNDS);
        Bnd_Box box;
        BRepBndLib::Add(entry->shape, box, false);
        if (box.IsVoid()) return answerNothing();
        const gp_Pnt lo = box.CornerMin();
        const gp_Pnt hi = box.CornerMax();
        double six[6] = {lo.X(), lo.Y(), lo.Z(), hi.X(), hi.Y(), hi.Z()};
        if (perturbation() == 2) {
            for (int i = 3; i < 6; ++i) six[i] += PERTURB_SHIFT;
        }
        return answer(six, sizeof(six));
    } catch (const Standard_Failure& failure) {
        return fail(KAPY_E_FAILED, failure.GetMessageString());
    } catch (...) {
        return fail(KAPY_E_FAILED, "getShapeBounds: unknown failure");
    }
}
