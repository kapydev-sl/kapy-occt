// services/occt/build/embind/bindings/capiEmbind.cpp
//
// The store as the TypeScript side sees it: the shape cache mints a handle
// through `Kapy_StorePut`, lets go through `Kapy_StoreRelease`, and asks which
// handles the C API dropped behind its back (`Kapy_StoreTakeDropped`) so it
// can dispose what it holds for them. `Kapy_CapiPerturbForTest` is the red
// control's lever: it makes the C measurements lie by a known amount, and is
// never called outside tests.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the store itself and the C entry points.

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <TopoDS_Shape.hxx>

#include "capiState.hxx"
#include "capiStore.hxx"

using namespace emscripten;

namespace {
unsigned int storePut(const TopoDS_Shape& shape) { return kapy_capi::put(shape); }
int storeRelease(unsigned int handle) { return kapy_capi::release(handle, false); }
int storeRetain(unsigned int handle) { return static_cast<int>(kapy_capi::retain(handle)); }
void storeReset(bool bumpEpoch) { kapy_capi::reset(bumpEpoch); }
unsigned int storeLive() { return kapy_capi::live(); }
unsigned int storeEpoch() { return kapy_capi::epoch(); }

val storeTakeDropped() {
    val out = val::array();
    for (uint32_t handle : kapy_capi::takeDropped()) out.call<void>("push", handle);
    return out;
}

void perturb(int mode) { kapy_capi::setPerturbation(mode); }
}  // namespace

EMSCRIPTEN_BINDINGS(kapy_capi) {
    function("Kapy_StorePut", &storePut);
    function("Kapy_StoreRelease", &storeRelease);
    function("Kapy_StoreRetain", &storeRetain);
    function("Kapy_StoreReset", &storeReset);
    function("Kapy_StoreLive", &storeLive);
    function("Kapy_StoreEpoch", &storeEpoch);
    function("Kapy_StoreTakeDropped", &storeTakeDropped);
    function("Kapy_CapiPerturbForTest", &perturb);
}
