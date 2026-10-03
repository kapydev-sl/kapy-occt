# engine/kernels/occt/build/embind

The build recipe of the kernel: Dockerfile, patches, CMake and every C++
source of the module. The overview, the build commands and the verification
live in [`../README.md`](../README.md); this file only says where things are
inside this directory.

| Path                        | What                                                                                                                           |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| `Dockerfile`                | emsdk 4.0.10 + OCCT `V8_0_0` to static toolkits in `/opt/occt`, native legacy WebAssembly exceptions, the two patches applied. |
| `patches/`                  | `0001` devirtualises the Gauss integrand; `0002` hashes transients by creation serial.                                         |
| `CMakeLists.txt`            | The toolkit list and the Emscripten flags. `bindings/*.cpp` is globbed in.                                                     |
| `rebuild.sh`                | Link in Docker, print sha256, `--twice` for determinism, `--publish` to copy into `engine/kernels/occt/dist/`.                 |
| `bindings/kapy_capi.h`      | The C API (ABI 1). One `KAPY_API` function per line: the host stub generator reads this file.                                  |
| `bindings/capi*.cpp`        | The C API implementation.                                                                                                      |
| `bindings/facts*.cpp`       | Naming facts collected inside the module.                                                                                      |
| `bindings/capiSelfTest*`    | The boot self-test.                                                                                                            |
| `bindings/exact*`           | Exact B-Rep container.                                                                                                         |
| `bindings/capiEmbind.cpp`   | The test levers registered with embind (`Kapy_*ForTest`); there is no embind class.                                            |

## Rules for the C++

- At most 300 lines per file; split at natural seams.
- A new C API function goes in `kapy_capi.h` first; the stub table is
  regenerated (`utils/scripts/build/capi-stubs.mjs`) and Rust gets a matching
  extern, which a test enforces.
- The boundary is numbers and bytes: no OCCT type crosses it, and no C++
  exception escapes a `KAPY_API` function.
- No Embind class registration: every call the host makes is a `KAPY_API`
  function. Embind is for test levers only.
- Exceptions are one model for the whole module (`-fwasm-exceptions`); do not
  add a dependency compiled with `-fexceptions`.

## Determinism

Linking twice from the same image gives the same bytes. Anything that puts an
address, a timestamp or a path into the output breaks that, and `rebuild.sh
--twice` is the gate.
