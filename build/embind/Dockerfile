# services/occt/build/embind/Dockerfile
#
# Option C toolchain: compile OCCT 8.0 to static WASM libs with a modern
# Emscripten, so our own Embind bindings (kapy_bindings.cpp) can link
# against them. This is the planegcs pattern (services/planegcs/build)
# scaled up to OCCT: we own the kernel version and the exposed API.
#
# Build the image (produces /opt/occt with static .a toolkits):
#   docker build -t kapy-occt-builder services/occt/build/embind
#
# Then link our bindings against it (see README.md):
#   docker run --rm -v "$PWD/services/occt/build/embind:/work" -w /work \
#     kapy-occt-builder sh -c 'emcmake cmake -B build && cmake --build build -j'
#
# Pin both toolchains: Emscripten and OCCT move fast and the linker flags
# are version-sensitive. Bump deliberately, never floating `latest`.
#
# Native WebAssembly exceptions (-fwasm-exceptions, the LEGACY try/catch
# proposal): a C++ throw and its catch stay inside wasm instead of every
# potentially-throwing call leaving for a JS `invoke_*` trampoline. Legacy and
# not the exnref proposal because that is what Safari 15.2+, Chrome 95+, Firefox
# 100+ and Node 22 all run; emsdk 4.0.x defaults to it (WASM_LEGACY_EXCEPTIONS).
# Build the image for linux/amd64, as every kernel before it was:
#   docker build --platform linux/amd64 -t kapy-occt-builder:wasm-eh services/occt/build/embind

FROM emscripten/emsdk:4.0.10

ARG OCCT_TAG=V8_0_0

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake ninja-build git ca-certificates python3 && \
    rm -rf /var/lib/apt/lists/*

# OCCT 8.0 source (LGPL-2.1). Shallow clone of the release tag.
RUN git clone --depth 1 --branch "${OCCT_TAG}" \
        https://github.com/Open-Cascade-SAS/OCCT.git /src/occt

# Devirtualise the Gauss integrand: under emscripten an indirect call becomes a
# wasm->JS->wasm trampoline, and BRepGProp_Gauss makes one per Gauss point.
# Without this, BRepGProp::SurfaceProperties is ~3x slower than the 7.7 build.
# Hash a transient (and a shape, through its TShape) by the serial it was
# created under, not by its address: map orders — and so what MakeThickSolid
# builds — must not depend on where the heap put things.
COPY patches/ /src/patches/
RUN cd /src/occt && git apply /src/patches/0001-brepgprop-gauss-devirtualise.patch && \
    git apply /src/patches/0002-hash-transients-by-creation-serial.patch

# Build ONLY the toolkits our kernel usage needs, statically, with every
# optional third-party (FreeType, FreeImage, OpenGL, TK, RapidJSON) OFF —
# we never render or read fonts inside the worker. DataExchange stays ON
# for STEP/STL. Trim the module set aggressively: dead code the linker
# can drop is dead code we never compile.
# Exceptions are NOT optional: OCCT reports recoverable failures by throwing
# Standard_Failure, and without them emscripten turns every throw into abort().
# OCCT's own adm/cmake/occt_defs_flags.cmake appends a hard-coded -fexceptions
# (the emulated, JS-trampoline model) to every compile, which would leave half
# the kernel on the other model and make the link refuse the mix; rewrite it
# to -fwasm-exceptions so the WHOLE module uses one model.
#
# The same file also defines OCC_CONVERT_SIGNALS, which turns every
# OCC_CATCH_SIGNALS into a setjmp/longjmp pair so a POSIX signal can be thrown
# as a C++ exception. WebAssembly has no signals (a fault is a trap, not a
# signal), so the pair never fires; but with native EH it lowers to
# wasm-sjlj, and LLVM 20 (emsdk 4.0.x) emits an invalid `br_table` inside
# `try` for the big functions that use it (ShapeUpgrade_ShapeDivide::Perform,
# ShapeCustom_BSplineRestriction::ConvertSurface, ...), which V8 rejects.
# Dropping the definition makes OCC_CATCH_SIGNALS empty: same behaviour,
# no setjmp registration on every guarded call, and a module that validates.
RUN sed -i -e 's/-fexceptions/-fwasm-exceptions/g' \
           -e '/add_definitions(-DOCC_CONVERT_SIGNALS)/d' \
        /src/occt/adm/cmake/occt_defs_flags.cmake && \
    grep -c fwasm-exceptions /src/occt/adm/cmake/occt_defs_flags.cmake && \
    ! grep -n 'fexceptions\|OCC_CONVERT_SIGNALS' /src/occt/adm/cmake/occt_defs_flags.cmake
RUN cd /src/occt && emcmake cmake -G Ninja -B /build \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_LIBRARY_TYPE=Static \
        -DCMAKE_CXX_FLAGS="-fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=1" \
        -DCMAKE_C_FLAGS="-fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=1" \
        -DBUILD_MODULE_Draw=OFF \
        -DBUILD_MODULE_Visualization=OFF \
        -DBUILD_MODULE_ApplicationFramework=OFF \
        -DBUILD_MODULE_DataExchange=ON \
        -DBUILD_MODULE_ModelingData=ON \
        -DBUILD_MODULE_ModelingAlgorithms=ON \
        -DBUILD_MODULE_FoundationClasses=ON \
        -DUSE_FREETYPE=OFF \
        -DUSE_FREEIMAGE=OFF \
        -DUSE_OPENGL=OFF \
        -DUSE_GLES2=OFF \
        -DUSE_RAPIDJSON=OFF \
        -DUSE_DRACO=OFF \
        -DUSE_TK=OFF \
        -DUSE_TBB=OFF \
        -DINSTALL_DIR=/opt/occt && \
    cmake --build /build -j "$(nproc)" && \
    cmake --install /build

ENV OCCT_ROOT=/opt/occt
