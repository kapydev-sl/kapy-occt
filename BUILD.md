# engine/kernels/occt

The OCCT 8.0 B-Rep kernel the app runs, as a WebAssembly module.

| Path                       | What                                                                                                                                                                         |
| -------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `dist/kapy-occt.{js,wasm}` | The committed kernel: the only OCCT the worker, the Node test harness and the job worker boot. It changes only at the checkpoints the plan names.                            |
| `kernelPath.ts`            | `shippedKernelPath()` and `initShippedKernel()`: the one way Node-side code boots the kernel.                                                                                |
| `build/`                   | Everything needed to rebuild the module from the OCCT sources, deterministically (Docker, patches, bindings, `rebuild.sh`). Start at [`build/README.md`](./build/README.md). |

The `opencascade.js` npm package (OCCT 7.7) is a types-only dependency; its
runtime import is banned by eslint. Verifying against 7.7 would certify
geometry the app cannot build.

## How the app talks to it

- The Rust document core (`kpy-core.wasm`) calls the module's C API
  (`build/embind/bindings/kapy_capi.h`): every shape build and every question
  about a shape. The TypeScript host only moves bytes between the two memories
  (`src/cad/workers/rust/capi/`, `src/cad/workers/occt/capiBinding.ts`).
- No TypeScript touches an embind class: `tests/unit/guards/embindUsage.test.ts`
  holds `src/` at zero `oc.` accesses. The only embind left in the module is
  the test levers (`Kapy_*ForTest`).

## Rebuilding

```bash
docker build --platform linux/amd64 -t kapy-occt-builder:wasm-eh engine/kernels/occt/build/embind
engine/kernels/occt/build/embind/rebuild.sh --twice     # the two sha256 must match
engine/kernels/occt/build/embind/rebuild.sh --publish   # copies to dist/
npm run build:workers                              # twice; restamps the digests
```

A kernel binary is committed only at a plan checkpoint. In between, point the
Node harness at a local link with `KAPY_OCCT_MODULE=/path/to/kapy-occt.js`.

## Checks that guard the kernel

- Boot self-test (C++, `kapy_self_test`): the boolean, fillet and unify history
  the persistent naming relies on is complete. A failure is a `console.warn`
  at boot; `tests/unit/workers/workerBundlesBoot.occt.test.ts` fails on it.
- `tests/unit/**/*.occt.test.ts`: geometry against the real module, no mocks.
- `npm run corpus:check` and `npm run corpus:ii:check`: the document corpora
  must regenerate to the committed fingerprints.

OCCT is LGPL-2.1 with the Open CASCADE exception; see `LICENSES.md`.
