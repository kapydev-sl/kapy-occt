# Option C — own OCCT 8.0 build with hand-written Embind

Compile **OCCT 8.0** to WebAssembly ourselves and expose exactly the
classes our worker uses through **our own Embind bindings** — the only
path that gives us both a *modern, maintained kernel* and the *low-level
API* the TopoNamer depends on. Same pattern as
[`services/planegcs/build`](../../../planegcs/build), scaled to OCCT.

## Layout

| File | What |
| --- | --- |
| `Dockerfile` | emsdk 4.0.10 + OCCT 8.0 (`V8_0_0`) built to static WASM toolkits in `/opt/occt`, with native (legacy) WebAssembly exceptions. |
| `CMakeLists.txt` | Links `kapy_bindings.cpp` against those toolkits → `kapy-occt.{js,wasm}`. |
| `kapy_bindings.cpp` | Our Embind registrations. STARTER: one worked example per binding pattern; the rest is spec-driven TODO. |
| `BINDINGS_SPEC.md` | **Generated** by `../extract-methods.mjs --spec` — per class, the exact ctor overloads + static members the code calls. This is the checklist. |

## The one hard problem: overload numbering

Our TS calls opencascade.js's suffixed overload names — `gp_Pnt_3`,
`BRepBuilderAPI_MakeEdge_10`, `TopoDS.Face_1`. Embind does **not** number
overloads; we name each binding by hand. So the whole job hinges on one
decision:

- **C1 — reproduce opencascade.js's numbering (drop-in).** Register each
  overload under the same `_N` opencascade.js used, so the ~333 existing
  call sites are unchanged. Risk: OCCT 8.0 may have added/reordered
  overloads vs 7.7, shifting numbers; each must be checked against both
  `opencascade.full.d.ts` (7.7) and the 8.0 header. Tedious but no app
  churn. **Recommended to start.**
- **C2 — semantic names + update call sites.** Bind with readable names
  (`gp_Pnt_xyz`) and codemod the call sites. More upfront churn, cleaner
  and more stable long-term (no dependence on a generator's numbering).

Either way, keep the mapping in this one file and cover it with the
propagation self-test (below). This is the primary correctness risk of
Option C — not the compile.

> Accelerator worth evaluating before hand-writing everything: point
> **opencascade.js's own binding generator** (its `WasmGenerator`) at the
> OCCT 8.0 headers restricted to our class list. It emits the `_N`
> numbering for free (solving C1 mechanically) and only the classes we
> list. That is the pragmatic realization of Option C; hand-writing
> `kapy_bindings.cpp` is the fallback when the generator chokes on 8.0.

## Build

```bash
# 1. Build OCCT 8.0 -> static WASM toolkits (long; caches in the image).
#    --platform linux/amd64 is part of the recipe: every kernel so far was
#    built on amd64 and the bytes are only reproducible on the same image.
docker build --platform linux/amd64 -t kapy-occt-builder:wasm-eh services/occt/build/embind

# 2. Regenerate the binding spec from the current code.
node services/occt/build/extract-methods.mjs --spec

# 3. Compile + link our bindings against OCCT.
docker run --rm --platform linux/amd64 -v "$PWD/services/occt/build/embind:/work" -w /work \
  kapy-occt-builder:wasm-eh sh -c 'emcmake cmake -B build -G Ninja && cmake --build build -j'

# 4. Publish the artifact.
mkdir -p services/occt/dist
cp build/kapy-occt.js build/kapy-occt.wasm services/occt/dist/
# then `npm run build:workers` (twice: the second run must change nothing) to
# restamp src/cad/workers/wasmDigests.ts and workerVersion.ts.
```

The link is deterministic: linking twice from the same image gives the same
sha256 for both files (`kapy-occt.wasm` dd6b5997..., 18 598 351 bytes).

## Exceptions: one model for the whole module

The kernel uses NATIVE WebAssembly exceptions, the legacy `try`/`catch`
proposal (`-fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=1`, the emsdk 4.0.x
default). A C++ `throw` and its `catch` stay inside wasm; the old model
(`-fexceptions`) sent every potentially-throwing call out to a JS `invoke_*`
trampoline and took 41 % of a Bancal regeneration (16 million trampolines).
Legacy and not the exnref proposal because that is what Safari 15.2+, Chrome
95+, Firefox 100+ and Node 22 run (exnref needs Safari 18.4+ and a flag on
Node 22).

Three things in the recipe exist only to make that true:

- OCCT's own `adm/cmake/occt_defs_flags.cmake` appends a hard-coded
  `-fexceptions` to every compile. The Dockerfile rewrites it to
  `-fwasm-exceptions`; left alone, half the kernel would be on the other model.
- The same file defines `OCC_CONVERT_SIGNALS`, which makes every
  `OCC_CATCH_SIGNALS` a `setjmp`/`longjmp` pair. WebAssembly has no signals, so
  the pair never fires, but with native exceptions it lowers to wasm-sjlj and
  LLVM 20 (emsdk 4.0.x) emits an invalid `br_table` inside `try` in the big
  functions that use it (`ShapeUpgrade_ShapeDivide::Perform`,
  `ShapeCustom_BSplineRestriction::ConvertSurface`, ...): V8 refuses the
  module. The Dockerfile removes the definition; `OCC_CATCH_SIGNALS` becomes
  empty, as it is on Windows.
- A thrown exception reaches JS as a `WebAssembly.Exception`, not a number:
  `decodeOcctException` (`src/cad/workers/occt/occtException.ts`) reads it
  through `getExceptionMessage` (`-sEXPORT_EXCEPTION_HANDLING_HELPERS`).

Check that no `invoke_*` import is left in the link: the boundary profiler
(`npm run bench:boundary`) prints `invoke_* trampolines per regen: 0`.

Expect step 3 to fail-iterate at first: unresolved symbols tell you which
OCCT toolkit to add to `CMakeLists.txt`'s `target_link_libraries`, and
missing bindings surface when the worker runs. The linker + the self-test
are the source of truth for "complete".

## Integrating

Same as Option A (see [`../../README.md`](../../README.md#integrating)):
`occt.worker.ts` loads the custom module via `initOpenCascade({ mainJS,
mainWasm })`, `workers.mjs` copies `kapy-occt.wasm`. Left unapplied until
a verified build exists (hard rule: don't touch worker boot unverified).

## Verify (non-negotiable for a kernel swap)

1. `runPropagationSelfTest` must not `console.warn` — it checks that
   `Modified()`/`Generated()` propagate on edges AND vertices, the exact
   binding the persistent naming relies on. Bind those methods on the
   boolean / fillet / unify algos precisely.
2. `npx vitest run tests/unit/**/*.occt.test.ts` — the geometry-
   correctness suite against real OCCT. Green here = the 8.0 kernel + our
   bindings behave like the 7.7 build the tests were written against.

## Status

- ✅ `BINDINGS_SPEC.md` generated + validated from the code (78 classes).
- ✅ Build harness (Dockerfile, CMake) + a per-pattern bindings starter.
- 🚧 `kapy_bindings.cpp` covers the patterns, not yet all 78 classes —
  complete it from the spec (or via the generator accelerator).
- ⛔ Not compiled in this environment: no emsdk here, and OCCT/Docker
  base layers need registry egress the session blocks. Runs on local/CI.

OCCT 8.0 is LGPL-2.1.
