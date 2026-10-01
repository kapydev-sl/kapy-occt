// services/occt/build/embind/bindings/kapy_capi.h
//
// The C API of the kernel, ABI version 1: the functions `kpy-core.wasm` (Rust)
// reaches OCCT with when it does not go through the JSON transport. Every
// function is `extern "C"`, `noexcept`, takes and returns plain integers and
// doubles, and reports a failure as a negative `KAPY_E_*` code; the message
// and any bulk answer sit in the result arena (`kapy_result_ptr/len`), which
// the next call overwrites.
//
// The declarations are also the source of the stub table the host generates
// (utils/scripts/build/capi-stubs.mjs reads the `KAPY_API` lines: one per
// line, C types from a short list), so a signature is changed here and the
// generator re-run, never the other way round.
//
// Who includes it: the capi*.cpp files of the link.
// What does NOT belong here: OCCT types (the boundary is numbers and bytes),
// the embind registrations (capiEmbind.cpp).

#pragma once

#include <cstdint>

#include <emscripten/emscripten.h>

#define KAPY_API extern "C" EMSCRIPTEN_KEEPALIVE

// The version of this header's contract. A host that binds stubs built for
// another version refuses the kernel instead of calling into a moved table.
#define KAPY_ABI_VERSION 1

// Codes. Zero is success; the host adds -100 (no kernel bound) and -101 (the
// call trapped) on its own side and they never come from here.
#define KAPY_OK 0
#define KAPY_E_UNKNOWN_HANDLE -1
#define KAPY_E_BAD_ARG -2
#define KAPY_E_FAILED -3
#define KAPY_E_NOMEM -4
// An operand of a boolean is not a solid (the host raises `ERR_KERNEL_NOT_SOLID`).
#define KAPY_E_NOT_SOLID -5
// The kernel will not answer this input and the core must: the JSON path
// builds it, or raises the fault it owns for it (a loft section that is a face
// with holes). Nothing was stored. Additive: the ABI version does not move.
#define KAPY_E_DECLINED -6

// Identity and self-check.
KAPY_API int32_t kapy_abi_version() noexcept;
KAPY_API int32_t kapy_self_test() noexcept;

// Memory the host writes into and reads out of (the two modules do not share
// one). `kapy_alloc` answers 0 when it cannot.
KAPY_API uint32_t kapy_alloc(uint32_t bytes) noexcept;
KAPY_API void kapy_free(uint32_t ptr) noexcept;

// The result arena: where the last call left its bulk answer.
KAPY_API uint32_t kapy_result_ptr() noexcept;
KAPY_API uint32_t kapy_result_len() noexcept;

// The last failure: its code (0 when the last call succeeded) and its message
// (written to the arena; the answer is its length).
KAPY_API int32_t kapy_error() noexcept;
KAPY_API int32_t kapy_error_message() noexcept;

// Handles. `kapy_release` takes `count` u32 handles at `ptr`; an unknown one
// is skipped (a second release is not an error, as in the JSON transport), and
// the handles it dropped are queued for the host (`Kapy_StoreTakeDropped`).
// `kapy_retain` answers the new count.
KAPY_API int32_t kapy_release(uint32_t ptr, uint32_t count) noexcept;
KAPY_API int32_t kapy_retain(uint32_t handle) noexcept;

// Measurements. The volume is one f64 in the arena; the bounds are six f64
// (min xyz, max xyz), or an empty arena for a void shape.
KAPY_API int32_t kapy_volume(uint32_t handle) noexcept;
KAPY_API int32_t kapy_bounds(uint32_t handle) noexcept;

// The naming facts the kernel collected since the last call, in the facts'
// binary layout (see topo/store/wire.rs), in the arena; the log is emptied.
KAPY_API int32_t kapy_facts_take() noexcept;

// Shape builders. Each takes a blob at `ptr` (`length` bytes, written by the
// host into the kernel's memory) holding the arguments, and answers the new
// handle as one u32 in the arena. The kernel names the shape (the facts it
// appends to the log are the ones the JSON transport records) and stores it, so
// the handle comes back ready to use (an empty arena means the operation has
// no result, as an empty intersection). The blob layouts are in the capi*.cpp
// file of each function.
KAPY_API int32_t kapy_extrude_profile(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_extrude_face_with_holes(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_extrude_face(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_drafted_profile(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_drafted_sections(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_twisted_profile(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_compound(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_make_box(uint32_t ptr, uint32_t length) noexcept;

// Revolves, pipe sweeps, lofts through sections (profiles or faces of bodies),
// and the helical sweeps (the helix feature and the thread).
KAPY_API int32_t kapy_revolve_profile(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_revolve_face_with_holes(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_sweep_profile(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_sweep_face_with_holes(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_loft(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_helical_sweep(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_thread_sweep(uint32_t ptr, uint32_t length) noexcept;

// Booleans, the plane trim, the multi-solid fuse, rigid transforms and pattern
// instancing. `kapy_split_solids` answers `u32 count` and then, per piece, its
// handle, its volume and its centroid (`u32, f64, f64 x3`), largest first.
KAPY_API int32_t kapy_boolean(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_fuse_many(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_trim_by_plane(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_split_solids(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_transform(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_instance_body(uint32_t ptr, uint32_t length) noexcept;
