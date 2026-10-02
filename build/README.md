# kapy-occt: the OCCT 8.0 kernel, built from source

This directory is everything needed to rebuild `services/occt/dist/kapy-occt.{js,wasm}`
from the OCCT 8.0 sources, byte for byte. It is published as the public
repository `kapydev-sl/kapy-occt` (LGPL-2.1, see `LICENSES.md` in the app
repository).

The module is the only OCCT the app runs: the Rust document core
(`kpy-core.wasm`) reaches it through a C API, and the few TypeScript
callers that remain use a small Embind surface (see below).

## Layout

| Path                                      | What                                                                                                                                                                                                                                         |
| ----------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `embind/Dockerfile`                       | emsdk 4.0.10 + OCCT `V8_0_0` built to static WASM toolkits in `/opt/occt`, with native (legacy) WebAssembly exceptions.                                                                                                                      |
| `embind/patches/`                         | The two patches applied to the OCCT sources (below).                                                                                                                                                                                         |
| `embind/CMakeLists.txt`                   | Links the bindings against those toolkits and sets the Emscripten flags.                                                                                                                                                                     |
| `embind/bindings/kapy_capi.h`             | The C API, ABI version 1: the `KAPY_API` functions Rust calls. The host stub table is generated from this file.                                                                                                                              |
| `embind/bindings/capi*.cpp`               | The C API: shape store and result arena (`capiStore`, `capiCore`), every build (`capiPrism`, `capiBoolean`, `capiShell`, `capiOffset*`, `capiSweep*`, ...) and every question (`capiMeasure`, `capiTopo*`, `capiMesh*`, `capiContact`, ...). |
| `embind/bindings/facts*.cpp`              | The kernel's own naming facts: what each operation modified, generated or deleted, collected inside the module and drained by Rust (`kapy_facts_take`).                                                                                      |
| `embind/bindings/capiSelfTest*.{hxx,cpp}` | The boot self-test, run by `kapy_self_test`.                                                                                                                                                                                                 |
| `embind/bindings/exact*.{hxx,cpp}`        | The exact B-Rep container (CAD-exact documents); `exactBrep.cpp` binds only a test seam.                                                                                                                                                     |
| `embind/bindings/*.cpp` (the rest)        | Embind registrations for the OCCT classes the TypeScript side still touches.                                                                                                                                                                 |
| `embind/kapy_bindings.cpp`                | Embind core: the `TopoDS_*` shape types and their enums.                                                                                                                                                                                     |
| `embind/rebuild.sh`                       | Links the module in Docker and prints the sha256; `--twice` proves determinism; `--publish` copies to `services/occt/dist/`.                                                                                                                 |

## Patches to OCCT

1. `0001-brepgprop-gauss-devirtualise.patch`: under Emscripten an indirect call
   is a wasm to JS to wasm trampoline, and `BRepGProp_Gauss` makes one per
   Gauss point. Devirtualising the integrand makes
   `BRepGProp::SurfaceProperties` about 3 times faster.
2. `0002-hash-transients-by-creation-serial.patch`: transients and shapes (through
   their `TShape`) hash by the serial they were created under, not by their
   address, so map iteration order, and therefore what an operation such as
   `MakeThickSolid` builds, does not depend on where the heap put things.

The Dockerfile also rewrites OCCT's `occt_defs_flags.cmake`: it replaces the
hard-coded `-fexceptions` with `-fwasm-exceptions` (one exception model in the
whole module) and removes `OCC_CONVERT_SIGNALS` (LLVM 20 emits an invalid
`br_table` for its `setjmp`/`longjmp` pair under native exceptions). Both
are explained in comments next to the lines.

## Build

Needs Docker. The image is built for `linux/amd64` on purpose: the bytes are
only reproducible on the same image.

```bash
# 1. The toolchain and the OCCT toolkits (long; cached in the image).
docker build --platform linux/amd64 -t kapy-occt-builder:wasm-eh services/occt/build/embind

# 2. Link twice and compare. The two sha256 must be equal.
services/occt/build/embind/rebuild.sh --twice

# 3. Publish locally (copies to services/occt/dist/).
services/occt/build/embind/rebuild.sh --publish
```

Then, in the app repository, `npm run build:workers` (twice; the second run
must change nothing) restamps `src/cad/workers/wasmDigests.ts` and
`workerVersion.ts`, and `tests/corpus/smartobjects/quickjs.golden.json`
`provenance.occtWasm` is repinned.

The link is deterministic: the same image and sources give the same sha256 for
`kapy-occt.wasm` and `kapy-occt.js`. The sha256 of the committed kernel is in
the closing section of `docs/history/v3/38-v3.3-occt-y-orquestador.md`.

## Exceptions

The module uses native WebAssembly exceptions, the legacy `try`/`catch`
proposal (`-fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=1`), because Safari
15.2+, Chrome 95+, Firefox 100+ and Node 22 run it (exnref needs Safari
18.4 and a flag on Node 22). A thrown exception reaches JS as a
`WebAssembly.Exception`; `getExceptionMessage` is exported to read it. The C
API never lets one cross the boundary: every `KAPY_API` function catches and
returns a negative `KAPY_E_*` code with the message in the result arena.

## The two doors into the module

1. **The C API** (`kapy_capi.h`). Plain integers, doubles and bytes. Shapes
   live in a handle table inside the module; a call that builds returns a new
   handle, a call that asks fills the result arena (`kapy_result_ptr/len`).
   Rust calls it directly; the TypeScript host only copies bytes between the
   two memories. This is where every shape build, every measurement and the
   naming facts go through.
2. **Embind** (`--bind`). What is left: the TypeScript orchestrator's own
   primitive shapes, the fresh (non-memoised) STL meshing, the HLR edge
   convexity, and a few test seams (`Kapy_*ForTest`). The per-file count of
   `oc.<name>` accesses in `src/` is frozen by
   `tests/unit/guards/embindUsage.test.ts` and can only shrink as the
   orchestrator moves to Rust.

## Boot self-test

`kapy_self_test()` (in `capiSelfTest*.cpp`) checks, inside the module, that
the history OCCT reports for a boolean, a fillet and a unify (`Modified()`,
`Generated()`, `IsDeleted()` on edges and vertices) is complete: the exact
binding the persistent naming relies on. It returns 0 when healthy and a
failure code otherwise, with the words of what failed in the result arena.
Rust runs it at worker boot and `console.warn`s on a failure.

## Verifying a kernel

From the app repository, with the kernel in `services/occt/dist/`:

```bash
npx vitest run tests/unit/workers tests/unit/topo tests/unit/e2e   # real OCCT, no mocks
npm run corpus:check && npm run corpus:ii:check                    # zero differences
```

`KAPY_OCCT_MODULE=/path/to/kapy-occt.js` points the Node harness at a kernel
that is not the committed one (it prints a warning about the digest).
