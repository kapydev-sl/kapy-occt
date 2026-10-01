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
// Only `makeBox` and `splitSolids` are not memoised, as in the binding, and an
// operation that has no result (an empty intersection) answers nothing and
// remembers nothing, because the binding never remembers a null.
//
// Who includes it: the capi*.cpp build operations.
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
#include "capiStore.hxx"
#include "kapy_capi.h"

namespace kapy_capi {

// The sweep vector of a prism. The red control (perturbation 3) lengthens it by
// a metre along z, so every height measured off the result reads differently.
inline gp_Vec prismVector(const double* d) {
    constexpr double PERTURB_SHIFT = 1000.0;
    return gp_Vec(d[0], d[1], d[2] + (perturbation() == 3 ? PERTURB_SHIFT : 0.0));
}

// A failure that is the step's fault and not the kernel's: an operand of a
// boolean that is not a solid. The host maps it to `ERR_KERNEL_NOT_SOLID`.
struct NotSolidError : OpError {
    explicit NotSolidError(const std::string& message) : OpError(message) {}
};

// An input the kernel can build only by raising a fault the core owns (a loft
// section that is a face with holes): the call answers `KAPY_E_DECLINED`, the
// host takes the JSON path, and the binding raises it there. Nothing has been
// stored when it is thrown.
struct DeclinedError : OpError {
    explicit DeclinedError(const std::string& message) : OpError(message) {}
};

// A handle the store does not hold, worded as the binding words it.
struct UnknownHandleError : OpError {
    explicit UnknownHandleError(uint32_t handle)
        : OpError("OCCT shape handle not found: h_" + std::to_string(handle)) {}
};

// The entry of `handle`, or the unknown-handle failure.
inline Entry& need(uint32_t handle) {
    Entry* entry = find(handle);
    if (!entry) throw UnknownHandleError(handle);
    return *entry;
}

// The first byte of a blob (the kind of an operation that has several), or 255
// when there is none; the blob reader refuses such a blob afterwards.
inline uint8_t firstByte(uint32_t ptr, uint32_t length) {
    if (length == 0 || ptr == 0) return 255;
    return *reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(ptr));
}

// Run `body()` (it answers a status) and turn whatever it throws into a code.
template <typename Body>
int32_t guarded(const char* name, Body&& body) {
    try {
        return body();
    } catch (const BlobError& e) {
        return fail(KAPY_E_BAD_ARG, e.what());
    } catch (const NotSolidError& e) {
        return fail(KAPY_E_NOT_SOLID, e.what());
    } catch (const UnknownHandleError& e) {
        return fail(KAPY_E_UNKNOWN_HANDLE, e.what());
    } catch (const DeclinedError& e) {
        return fail(KAPY_E_DECLINED, e.what());
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

// Run `body(Blob&)` (it answers the new handle, or 0 for no result) as the
// operation `name`, seeded with `seed`; answers the handle as one u32 in the
// arena, nothing for no result, or a failure code.
template <typename Body>
int32_t runOp(const char* name, size_t seed, uint32_t ptr, uint32_t length, Body&& body,
              bool memoised = true) {
    begin();
    if (length != 0 && ptr == 0) return fail(KAPY_E_BAD_ARG, "build: no arguments at ptr");
    const void* bytes = reinterpret_cast<const void*>(static_cast<uintptr_t>(ptr));
    return guarded(name, [&]() -> int32_t {
        const std::string key =
            std::string(name) + "|" + std::string(static_cast<const char*>(bytes), length);
        uint32_t handle = memoised ? memoFind(key) : 0;
        if (handle == 0) {
            Standard_Transient::SetSerialCounter(seed);
            Blob in(bytes, length);
            handle = body(in);
            if (handle == 0) return answerNothing();
            if (memoised) memoStore(key, handle);
        }
        return answer(&handle, sizeof(handle));
    });
}

// Run `body(Blob&)` as an operation that answers its own bytes (`splitSolids`):
// seeded every time, never memoised.
template <typename Body>
int32_t runRaw(const char* name, size_t seed, uint32_t ptr, uint32_t length, Body&& body) {
    begin();
    if (length != 0 && ptr == 0) return fail(KAPY_E_BAD_ARG, "build: no arguments at ptr");
    const void* bytes = reinterpret_cast<const void*>(static_cast<uintptr_t>(ptr));
    return guarded(name, [&]() -> int32_t {
        Standard_Transient::SetSerialCounter(seed);
        Blob in(bytes, length);
        return body(in);
    });
}

}  // namespace kapy_capi
