// engine/kernels/occt/build/embind/bindings/capiMemo.hxx
//
// The memo of the C build operations: a pure build asked twice with the same
// arguments answers the handle of the first while that handle is alive, which
// is what the regen has always relied on. The key is the operation and the exact bytes of its
// arguments; a hit mints nothing and runs nothing (no serial seeding either),
// so a repeated regen leaves the kernel exactly as the first one did.
//
// Who includes it: capiStore.cpp (clears it on reset), capiFinish.cpp.
// What does NOT belong here: the operations, the store.

#pragma once

#include <cstdint>
#include <string>

namespace kapy_capi {

// The handle remembered for `key` when it is still alive in the store (the
// entry moves to the newest position); 0 otherwise. A dead entry is dropped.
uint32_t memoFind(const std::string& key);

// Remember `handle` for `key`; the oldest entry goes past 512.
void memoStore(const std::string& key, uint32_t handle);

// Forget everything (a store reset: every handle is gone).
void memoClear();

}  // namespace kapy_capi
