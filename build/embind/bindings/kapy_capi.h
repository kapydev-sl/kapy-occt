// engine/kernels/occt/build/embind/bindings/kapy_capi.h
//
// The C API of the kernel, ABI version 1: the functions `kpy-core.wasm` (Rust)
// reaches OCCT with. It is the only way into the kernel for a shape build or a
// question about a shape. Every
// function is `extern "C"`, `noexcept`, takes and returns plain integers and
// doubles, and reports a failure as a negative `KAPY_E_*` code; the message
// and any bulk answer sit in the result arena (`kapy_result_ptr/len`), which
// the next call overwrites.
//
// The declarations are also the source of the stub table the host generates
// (utils/scripts/build/capi-stubs.mjs reads the `KAPY_API` and `KAPY_HOST_API`
// lines: one per line, C types from a short list), so a signature is changed here and the
// generator re-run, never the other way round.
//
// Who includes it: the capi*.cpp files of the link.
// What does NOT belong here: OCCT types (the boundary is numbers and bytes),
// the test-only embind registrations (capiEmbind.cpp).

#pragma once

#include <cstdint>

#include <emscripten/emscripten.h>

#define KAPY_API extern "C" EMSCRIPTEN_KEEPALIVE
// A function only the host calls (the store's plumbing, capiPlumb.cpp): same
// linkage, and the generator marks it so Rust declares no extern for it.
#define KAPY_HOST_API extern "C" EMSCRIPTEN_KEEPALIVE

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
// No attempt the caller asked for built anything usable: `kapy_error_arg()`
// is the index of the solid that ran out (`ERR_KERNEL_NO_RESULT`).
#define KAPY_E_NO_RESULT -6
// A body face handed over as a single-wire section has holes in it
// (`ERR_KERNEL_FACE_HAS_HOLES`).
#define KAPY_E_FACE_HAS_HOLES -7

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
// The number a failure carries beside its code: the solid index of a
// `KAPY_E_NO_RESULT`, zero for every other failure.
KAPY_API int32_t kapy_error_arg() noexcept;

// Handles. `kapy_release` takes `count` u32 handles at `ptr`; an unknown one
// is skipped (a second release is not an error, as a second release is harmless), and
// the handles it dropped are queued for the host (`kapy_take_dropped`).
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
// appends to the log are what the core's naming table is made from) and stores it, so
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

// Blend, shell and offset: one fillet or chamfer build, a thick solid through a
// ladder handed over as data, an offset of a whole solid and an offset of the
// picked faces. Each names its result and answers its handle; a refusal
// answers `KAPY_E_FAILED` with the words the product reads (or
// `KAPY_E_NO_RESULT`) and stores nothing.
KAPY_API int32_t kapy_blend_attempt(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_blend_probe(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_thick_solid(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_offset_solid(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_offset_faces(uint32_t ptr, uint32_t length) noexcept;

// The swept volume of a solid along a ray, and the clearance cut built on it
// (the analytic channel; the mesh channel is the host's, in manifold-3d):
// `kapy_sweep_ray`                                   (capiSweepRay.cpp)
// `kapy_clearance_cut`                               (capiClearanceCut.cpp)
KAPY_API int32_t kapy_sweep_ray(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_clearance_cut(uint32_t ptr, uint32_t length) noexcept;

// The solid of a channel the host swept in the mesh domain, rebuilt from the
// welded mesh and its regions (capiChannel.cpp); the cut is Rust's.
KAPY_API int32_t kapy_channel_solid(uint32_t ptr, uint32_t length) noexcept;

// The questions: what the kernel measures, classifies and walks, each over one
// blob `(ptr, length)` that starts with the handle(s) it asks about and each
// answering its bytes in the arena. None mints or stores a shape, none is
// memoised. An unknown handle is `KAPY_E_UNKNOWN_HANDLE`; anything the kernel
// raises while it works is `KAPY_E_FAILED`, worded as OCCT words it
// (`<exception type>,<message>`). The layouts are in the
// capi*.cpp file of each function.
//
// `kapy_area` / `kapy_mass_props` / `kapy_is_valid`  (capiMeasure.cpp)
// `kapy_shapes_intersect`                            (capiContact.cpp)
// `kapy_classify_point`                              (capiClassify.cpp)
// `kapy_face_edges` / `kapy_seam_edges`              (capiTopo.cpp)
// `kapy_face_surface` / `kapy_face_normal`           (capiFace.cpp)
// `kapy_face_polylines` / `kapy_face_wires`          (capiContours.cpp)
// `kapy_min_distance` / `kapy_face_boundary`         (capiQuery.cpp)
KAPY_API int32_t kapy_area(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_mass_props(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_is_valid(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_shapes_intersect(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_classify_point(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_face_edges(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_seam_edges(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_face_surface(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_face_normal(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_face_polylines(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_face_wires(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_min_distance(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_face_boundary(uint32_t ptr, uint32_t length) noexcept;

// What the blend and shell recipes read off a body around their attempts:
// `kapy_topology`                                    (capiTopoGraph.cpp)
// `kapy_edge_probe`                                  (capiEdgeProbe.cpp)
// `kapy_face_triangle`                               (capiFaceTriangle.cpp)
// `kapy_shell_facts` / `kapy_thick_probe`            (capiShellFacts.cpp)
KAPY_API int32_t kapy_topology(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_edge_probe(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_face_triangle(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_shell_facts(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_thick_probe(uint32_t ptr, uint32_t length) noexcept;

// `kapy_mesh` / `kapy_hlr_input`                     (capiMesh.cpp)
// `kapy_brep_write` / `kapy_brep_read`               (capiBrepIo.cpp)
// `kapy_export_stl` / `kapy_export_step`             (capiExport.cpp)
// `kapy_import_step`                                 (capiImportStep.cpp)
// `kapy_import_mesh`                                 (capiImportMesh.cpp)
KAPY_API int32_t kapy_mesh(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_hlr_input(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_brep_write(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_brep_read(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_export_stl(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_export_step(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_import_step(uint32_t ptr, uint32_t length) noexcept;
KAPY_API int32_t kapy_import_mesh(uint32_t ptr, uint32_t length) noexcept;

// What only the host calls, around the operations (capiPlumb.cpp): start the
// store over, count its shapes, drain the handles the C operations minted or a
// release dropped (each a pointer to `[count, handle...]` u32s), and keep the
// naming tables a cache hands back.
KAPY_HOST_API void kapy_reset(uint32_t bumpEpoch) noexcept;
KAPY_HOST_API uint32_t kapy_live() noexcept;
KAPY_HOST_API uint32_t kapy_epoch() noexcept;
KAPY_HOST_API uint32_t kapy_take_minted() noexcept;
KAPY_HOST_API uint32_t kapy_take_dropped() noexcept;
KAPY_HOST_API int32_t kapy_table_adopt(uint32_t ptr, uint32_t length) noexcept;
KAPY_HOST_API int32_t kapy_table_bind(uint32_t id, uint32_t handle) noexcept;
KAPY_HOST_API void kapy_table_release(uint32_t id) noexcept;
KAPY_HOST_API uint32_t kapy_table_of(uint32_t handle) noexcept;
