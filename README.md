# kapy-occt

The modified Open CASCADE Technology (OCCT) 8.0 build that [Kapy CAD](https://kapycad.com)
ships as a WebAssembly module (`kapy-occt.wasm` / `kapy-occt.js`).

This repository publishes the complete set of modifications and the build recipe,
as required by OCCT's licence (LGPL-2.1 with the Open CASCADE exception):

- **Upstream:** OCCT `V8_0_0` (https://github.com/Open-Cascade-SAS/OCCT), cloned by the Dockerfile.
- **Patches** (`build/embind/patches/`): applied with `git apply` on top of upstream.
- **Bindings and C API** (`build/embind/bindings/`, `build/embind/kapy_bindings.cpp`, `kapy_capi.h`):
  our embind bindings and the `extern "C"` API compiled into the same module.
- **Build recipe:** `build/embind/Dockerfile`, `CMakeLists.txt`, `rebuild.sh`; the toolchain
  (emsdk version, flags such as native WebAssembly exceptions) is pinned there. See `BUILD.md`.

The Kapy CAD application loads this module as a separate, replaceable file; it can be
rebuilt from this repository and swapped in.

Licence: the OCCT sources and these modifications are under LGPL-2.1 with the Open CASCADE
exception (see `LICENSE-LGPL-2.1.txt` and https://dev.opencascade.org/resources/licensing).
