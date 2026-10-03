// engine/kernels/occt/build/embind/bindings/capiPlumb.cpp
//
// What the HOST asks of the kernel's store around the operations: start the
// store over, count what it holds, learn which handles the C operations minted
// and which ones a release dropped behind the host's back, and keep the
// naming tables a cache hands back (adopt one, bind it to a handle, release
// it). None of these is a crossing of the Rust frontier: the core never calls
// them, so they are `KAPY_HOST_API` and the generator leaves them out of the
// externs Rust is linked with.
//
// The two `take` functions answer a pointer to a static `u32` buffer
// (`[count, handle...]`) instead of using the result arena: the host reads
// them from inside the hook that runs right after a call, while the arena
// still holds the answer the caller of that call is about to read.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the store itself (capiStore.cpp) and the facts
// log (factsLog.cpp).

#include <string>
#include <vector>

#include "capiState.hxx"
#include "capiStore.hxx"
#include "factsLog.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

std::vector<uint32_t> g_taken;

// The handles in `handles`, as `[count, handle...]`, and where they are.
uint32_t lend(const std::vector<uint32_t>& handles) {
    g_taken.assign(1, static_cast<uint32_t>(handles.size()));
    g_taken.insert(g_taken.end(), handles.begin(), handles.end());
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_taken.data()));
}

}  // namespace

KAPY_HOST_API void kapy_reset(uint32_t bumpEpoch) noexcept { reset(bumpEpoch != 0); }

KAPY_HOST_API uint32_t kapy_live() noexcept { return live(); }

KAPY_HOST_API uint32_t kapy_epoch() noexcept { return epoch(); }

KAPY_HOST_API uint32_t kapy_take_minted() noexcept { return lend(takeMinted()); }

KAPY_HOST_API uint32_t kapy_take_dropped() noexcept { return lend(takeDropped()); }

KAPY_HOST_API int32_t kapy_table_adopt(uint32_t ptr, uint32_t length) noexcept {
    begin();
    const std::string text(reinterpret_cast<const char*>(static_cast<uintptr_t>(ptr)), length);
    const uint32_t id = allocTable();
    kapy_facts::adopt(id, text);
    return static_cast<int32_t>(id);
}

KAPY_HOST_API int32_t kapy_table_bind(uint32_t id, uint32_t handle) noexcept {
    Entry* entry = find(handle);
    if (!entry) return KAPY_E_UNKNOWN_HANDLE;
    entry->tableId = id;
    kapy_facts::bind(id, "h_" + std::to_string(handle));
    return KAPY_OK;
}

KAPY_HOST_API void kapy_table_release(uint32_t id) noexcept { kapy_facts::release(id); }

// The naming table bound to `handle`, 0 when it has none or is unknown.
KAPY_HOST_API uint32_t kapy_table_of(uint32_t handle) noexcept {
    const Entry* entry = find(handle);
    return entry ? entry->tableId : 0;
}
