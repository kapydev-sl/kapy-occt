# Vendored OCCT custom build (scaffolding)

Our own trimmed WebAssembly build of the OCCT B-Rep kernel, so we can
**ship only the ~80 classes our worker actually uses** instead of the
full `opencascade.full.wasm`, and **own the kernel version + any Embind
patches** — the same reasons we already vendor + build planegcs
ourselves (see [`../planegcs/README.md`](../planegcs/README.md)).

This directory is **scaffolding**: the build config and the tooling to
keep it honest. The compiled artifact (`dist/kapy-occt.{js,wasm}`) **is now
committed** — it is the only OCCT kernel the worker boots. Re-produce it on a
machine with Docker-registry egress (see [Status](#status)) whenever the
kernel or bindings change.

## Why

Two separate goals, which need to be kept apart because the tooling
serves them very differently:

1. **Smaller wasm** — a custom build binds only our classes and lets
   Emscripten `-O3` dead-code-eliminate everything else. This is fully
   solved by the config here.
2. **Newer / maintained OCCT kernel** — this is the real motivation and
   the harder half. See the ceiling below.

## The OCCT-version ceiling (read this first)

The moving parts are three different things, only one of which is
unmaintained:

- **OCCT (the C++ kernel)** — very much alive; **8.0 shipped May 2026**.
- **opencascade.js (the Embind binding generator + Docker toolchain)** —
  the thing that turns OCCT into a low-level `.wasm` with the exact API
  our code depends on (`BRepBuilderAPI_*`, `TopExp_Explorer`,
  `Modified()`/`Generated()`, …). **Last published 2023; its newest
  Docker build image is `2.0.0-beta.b5ff984` (Mar 2023) = OCCT 7.7.0.**
- **occt-wasm (npm, 2026)** — modern OCCT 8.0, but a high-level *façade*
  that does **not** expose the low-level classes our TopoNamer needs, so
  it is not a drop-in for us.

We are **already on that 7.7.0 beta** (`opencascade.js@2.0.0-beta.b5ff984`
in `package.json`). So building with the stock opencascade.js image gives
us **the same kernel version, just smaller** — it delivers goal #1 and
build ownership, but **not** goal #2.

### Getting to OCCT 8.0 (the three options)

| Option | What it takes | Effort / risk |
| --- | --- | --- |
| **A. Stock opencascade.js custom build** (this config as-is) | Run the 2023 image against `kapy-occt.yml`. | Low. **But caps at OCCT 7.7.0.** |
| **B. Rebuild the opencascade.js toolchain against OCCT 8.0** | Bump the OCCT submodule + Emscripten in opencascade.js's own build repo, regenerate bindings, fix whatever 7.7→8.0 API churn breaks the generator. | High. Uncertain — the generator may choke on 8.0 API changes. |
| **C. Hand-write Embind for our ~80 classes against OCCT 8.0** | Exactly the planegcs pattern: template the Embind `.cpp` for the methods we call, compile OCCT 8.0 + our bindings with a modern `emsdk`. | High but bounded; we already own this pattern for planegcs. Only path that is both **8.0 and low-level**. |

Recommended sequencing: land **A** first (proves the pipeline + gives
the size win + de-risks the class list), then **C** for the kernel bump
to 8.0. **C is scaffolded** under [`build/embind/`](build/embind/README.md)
— OCCT 8.0 Dockerfile, CMake link, a per-pattern Embind starter, and the
generated binding spec. Its crux (matching opencascade.js's overload
numbering) is documented there.

## Files

| File | What |
| --- | --- |
| `build/kapy-occt.yml` | **Option A** — opencascade.js custom-build config: the exact `bindings:` list (~80 OCCT symbols) + `emccFlags`. Caps at OCCT 7.7.0. |
| `build/extract-symbols.mjs` | Scans `src/cad/**` for `oc.<Symbol>` usage and diffs it against the yml, so the binding list can't silently drift from the code. Run in CI. |
| `build/extract-methods.mjs` | Deeper scan for **Option C**: per class, the exact ctor overloads + static members the code calls. Emits `embind/BINDINGS_SPEC.md`. |
| `build/embind/` | **Option C** — own OCCT 8.0 build with hand-written Embind (Dockerfile + CMake + bindings + spec). The path to a modern kernel *and* the low-level API. See [`build/embind/README.md`](build/embind/README.md). |
| `dist/` | Build output (`kapy-occt.{js,wasm,d.ts}`) — produced by either build, not committed until verified. |

## Keeping the binding list honest

```bash
node services/occt/build/extract-symbols.mjs        # fails if the yml misses a used symbol
node services/occt/build/extract-symbols.mjs --list # print the symbols the code uses
```

As of writing: **78 OCCT symbols referenced in `src/cad`, all present in
`kapy-occt.yml`** (the yml binds a few extra: the `HLRBRep_*` drawings
classes and `TopoDS_*` sub-shape types reached indirectly through the
`TopoDS` namespace).

## Building (Option A — requires Docker + registry egress)

```bash
cd services/occt/build

# Produces kapy-occt.{js,wasm,d.ts} from kapy-occt.yml.
docker run --rm \
  -v "$(pwd):/src" -u "$(id -u):$(id -g)" \
  donalffons/opencascade.js:2.0.0-beta.b5ff984 \
  kapy-occt.yml

mkdir -p ../dist && mv kapy-occt.js kapy-occt.wasm kapy-occt.d.ts ../dist/
ls -lh ../dist/kapy-occt.wasm   # compare against the ~11 MB full wasm
```

The build is its own validator: an unknown or renamed symbol fails
loudly, which is how we confirm `kapy-occt.yml` is complete against the
pinned OCCT.

## Integrating (apply only after a verified build)

Unlike planegcs, a custom OCCT build is **not** a wasm-only swap: the
generated JS glue (`kapy-occt.js`) is paired with its `.wasm`, so the
worker must load the custom glue, not the npm wrapper's bundled full
glue. `opencascade.js`'s `initOpenCascade` supports overriding both:

1. `src/cad/workers/occt/occt.worker.ts` — pass the custom module:

   ```ts
   import initOpenCascade from 'opencascade.js';
   import mainJS from '../../../../services/occt/dist/kapy-occt.js';
   // ...
   const instance = await initOpenCascade({
       mainJS,
       mainWasm: resolveWasmUrl('kapy-occt.wasm'),
   });
   ```

2. `utils/scripts/build/workers.mjs` — copy `services/occt/dist/kapy-occt.wasm`
   into `public/workers/kapy-occt.wasm` instead of `opencascade.full.wasm`.

3. **Verify worker boot** per the hard rule in
   [`CLAUDE.md`](../../CLAUDE.md) / [`docs/workers.md`](../../docs/workers.md):
   open `/edit/[id]`, confirm the viewport renders and that
   `runPropagationSelfTest` does **not** `console.warn` (it exercises
   `Modified()`/`Generated()` on edges and vertices — the single most
   important thing to re-check against any rebuilt kernel), then run the
   OCCT suite:

   ```bash
   npx vitest run tests/unit/**/*.occt.test.ts
   ```

This integration is intentionally **left unapplied** here: the hard rule
says not to touch the worker boot / WASM resolution without verifying
both workers boot end-to-end, which needs the built artifact.

## Status

- ✅ Binding config (`kapy-occt.yml`) written and **validated against the
  code** (`extract-symbols.mjs` passes: every used symbol is bound).
- ✅ Build + integration procedure documented.
- ✅ **Shipped.** `dist/kapy-occt.{js,wasm}` is committed and is the sole OCCT
  kernel the worker boots (`OCCT_KERNEL` A/B flag removed). The old stock
  `opencascade.full.wasm` no longer ships; the `opencascade.js` npm package is
  kept only for its TS types + Node test kernel.
- ✅ **Native WebAssembly exceptions (v3.3 K1).** The committed kernel is built
  with emsdk 4.0.10 and `-fwasm-exceptions` (legacy EH: Safari 15.2+, Chrome
  95+, Firefox 100+, Node 22), sha256 `dd6b5997...`; a cold Bancal regeneration
  costs 56 % less CPU than on the emulated-exception kernel. The recipe and
  the reasons behind each flag are in
  [`build/embind/README.md`](build/embind/README.md#exceptions-one-model-for-the-whole-module).
- ⛔ **Build not executed in the original scaffolding environment.** The opencascade.js image
  layers are served from `production.cloudfront.docker.com`, which the
  session's egress policy blocks (`403 Forbidden`). `docker pull` gets the
  manifest but not the blobs. Run the build on a machine with normal
  Docker Hub access (local dev or CI).

Upstream OCCT is LGPL-2.1; opencascade.js tooling is Apache-2.0/MIT.
