// services/occt/build/embind/bindings/capiState.hxx
//
// What the C entry points share: the result arena, the last error and the
// test-only perturbation. One definition (capiCore.cpp), so an operation file
// never keeps state of its own.
//
// Who includes it: capiCore.cpp, capiOps.cpp, capiOp.hxx, capiEmbind.cpp.
// What does NOT belong here: the public signatures (kapy_capi.h).

#pragma once

#include <cstddef>
#include <cstdint>

namespace kapy_capi {

// Begin a call: clears the last error so `kapy_error()` reads this call's.
void begin();

// Record a failure and answer its code, for `return fail(...)`.
int32_t fail(int32_t code, const char* message);

// Replace the arena's bytes with `bytes` (copied) and answer KAPY_OK.
int32_t answer(const void* bytes, size_t length);

// The arena empty, answer KAPY_OK: a call that has no bulk answer.
int32_t answerNothing();

// The perturbation a red control asks for (see `Kapy_CapiPerturbForTest`):
// 0 none, 1 the volume answer is zeroed, 2 the bounds answer is shifted, 3 every
// prism sweeps a metre further along z, 4 every cut runs as a common, 5 every
// translation goes a metre further along x, 6 every revolve turns around an
// axis a metre further along x, 7 every blend runs at half its radius or
// distance.
int perturbation();
void setPerturbation(int mode);

}  // namespace kapy_capi
