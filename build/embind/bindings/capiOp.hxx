// services/occt/build/embind/bindings/capiOp.hxx
//
// The shell every C build operation runs in: check the call, look the memo up,
// number the kernel's serials from the operation's seed, run the body, and
// turn whatever it throws into a code. One template, so the operations differ
// only in their body and the error contract is written once.
//
// The memo key is the operation's name and the exact bytes of its arguments
// (the TypeScript binding keys on the same arguments): a repeat while the first
// handle lives answers it without running anything, seeding nothing, as the
// binding's op memo does. The seed is taken only on a miss for the same reason.
//
// The blob is read at a kernel address the host wrote into (`kapy_alloc`).
// Only `makeBox` is not memoised, as in the binding.
//
// Who includes it: capiPrism.cpp, capiLoft.cpp, capiPush.cpp.
// What does NOT belong here: what an operation builds.

#pragma once

#include <cstdint>
#include <string>

#include <Standard_Failure.hxx>
#include <Standard_Transient.hxx>
#include <gp_Vec.hxx>

#include "capiBlob.hxx"
#include "capiMemo.hxx"
#include "capiProfile.hxx"
#include "capiState.hxx"
#include "kapy_capi.h"

namespace kapy_capi {

// The sweep vector of a prism. The red control (perturbation 3) lengthens it by
// a metre along z, so every height measured off the result reads differently.
inline gp_Vec prismVector(const double* d) {
    constexpr double PERTURB_SHIFT = 1000.0;
    return gp_Vec(d[0], d[1], d[2] + (perturbation() == 3 ? PERTURB_SHIFT : 0.0));
}

// Run `body(Blob&)` (it answers the new handle)
// as the operation `name`, seeded with `seed`; answers the handle as one u32
// in the arena, or a failure code.
template <typename Body>
int32_t runOp(const char* name, size_t seed, uint32_t ptr, uint32_t length, Body&& body,
              bool memoised = true) {
    begin();
    if (length != 0 && ptr == 0) return fail(KAPY_E_BAD_ARG, "build: no arguments at ptr");
    const void* bytes = reinterpret_cast<const void*>(static_cast<uintptr_t>(ptr));
    try {
        const std::string key =
            std::string(name) + "|" + std::string(static_cast<const char*>(bytes), length);
        uint32_t handle = memoised ? memoFind(key) : 0;
        if (handle == 0) {
            Standard_Transient::SetSerialCounter(seed);
            Blob in(bytes, length);
            handle = body(in);
            if (memoised) memoStore(key, handle);
        }
        return answer(&handle, sizeof(handle));
    } catch (const BlobError& e) {
        return fail(KAPY_E_BAD_ARG, e.what());
    } catch (const OpError& e) {
        return fail(KAPY_E_FAILED, e.what());
    } catch (const Standard_Failure& f) {
        return fail(KAPY_E_FAILED, f.GetMessageString());
    } catch (const std::exception& e) {
        return fail(KAPY_E_FAILED, e.what());
    } catch (...) {
        return fail(KAPY_E_FAILED, (std::string(name) + ": unknown failure").c_str());
    }
}

}  // namespace kapy_capi
