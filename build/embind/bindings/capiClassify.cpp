// services/occt/build/embind/bindings/capiClassify.cpp
//
// `kapy_classify_point`: where a world-space point sits against a stored body.
// The walk itself is `probePoint` (capiClassify.hxx), the binding's.
//
// Blob: u32 handle, f64 x, f64 y, f64 z. Answers: one byte (the state) and a
// u32 (how many solids were tested; a cylinder's probe reports it).
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the store, the arena, the walk.

#include "capiAsk.hxx"
#include "capiClassify.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('classifyPoint').
constexpr size_t SEED_CLASSIFY = 1694952259u;
}  // namespace

KAPY_API int32_t kapy_classify_point(uint32_t ptr, uint32_t length) noexcept {
    return ask("classifyPoint", SEED_CLASSIFY, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        const double x = in.f64();
        const double y = in.f64();
        const double z = in.f64();
        const PointProbe probe = probePoint(entry.shape, x, y, z);
        // The red control: a body that contains nothing, so a point that is
        // inside reads as outside.
        out.u8(perturbation() == 8 && probe.state == STATE_IN ? STATE_OUT : probe.state);
        out.u32(probe.solids);
    });
}
