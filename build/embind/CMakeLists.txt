# services/occt/build/embind/CMakeLists.txt
#
# Links kapy_bindings.cpp against the static OCCT 8.0 toolkits built into
# /opt/occt by the Dockerfile, emitting kapy-occt.js + kapy-occt.wasm.
# Run under emcmake (the Dockerfile's emsdk provides the toolchain file).
#
#   emcmake cmake -B build && cmake --build build -j
#
# Output lands in build/kapy-occt.{js,wasm}; move to ../../dist/.

cmake_minimum_required(VERSION 3.20)
project(kapy_occt CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OCCT_ROOT "$ENV{OCCT_ROOT}" CACHE PATH "OCCT install prefix")
if(NOT OCCT_ROOT)
    set(OCCT_ROOT /opt/occt)
endif()

# OCCT ships a CMake package config under a lower-cased directory that
# find_package's default suffixes miss. NO_CMAKE_FIND_ROOT_PATH is required
# because the Emscripten toolchain sets CMAKE_FIND_ROOT_PATH_MODE_PACKAGE to
# ONLY, which would otherwise make find_package ignore any path outside the
# emsdk sysroot — including our OCCT install.
find_package(OpenCASCADE REQUIRED
    PATHS "${OCCT_ROOT}/lib/cmake/opencascade" "${OCCT_ROOT}"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

# The bindings are split by OCCT domain under bindings/ so no single file
# carries all 78 classes; each contributes its own EMSCRIPTEN_BINDINGS block.
file(GLOB KAPY_BINDING_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bindings/*.cpp")
add_executable(kapy-occt kapy_bindings.cpp ${KAPY_BINDING_SOURCES})

# OCCT signals every recoverable failure by throwing Standard_Failure, and the
# app depends on catching it (runLocalOperation's fillet retry, runShell's
# tolerance escalation, healShape). Emscripten disables exception CATCHING by
# default, which turns those throws into abort() — the module dies and the
# heap is corrupt.
# NATIVE WebAssembly exceptions, the legacy proposal (-fwasm-exceptions with
# WASM_LEGACY_EXCEPTIONS, emsdk's default): a throw and its catch stay inside
# wasm. The emulated model (-fexceptions) sent every call that might throw
# through a JS `invoke_*` trampoline, 16-364 million round trips per regen at
# ~19 ns, 39-49 % of its CPU. The whole module must use ONE model — OCCT's
# static toolkits are compiled with the same flag (see the Dockerfile), and a
# mix does not link. Legacy and not exnref because Safari 15.2+, Chrome 95+,
# Firefox 100+ and Node 22 run the legacy opcodes; exnref needs Safari 18.4 and
# a flag on Node 22.
target_compile_options(kapy-occt PRIVATE -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=1)
target_include_directories(kapy-occt PRIVATE ${OpenCASCADE_INCLUDE_DIR})

# The toolkits our bindings pull in. Order matters for a static link:
# higher-level algorithms first, foundation last. Trim/extend once the
# link errors tell you exactly what's unresolved — the linker is the
# source of truth for the minimal set.
target_link_libraries(kapy-occt PRIVATE
    # Data exchange (STEP / STL I/O)
    TKDESTEP TKDESTL TKXSBase TKDE
    # Hidden-line removal (drawings)
    TKHLR
    # Modeling algorithms
    TKMesh TKOffset TKFillet TKBool TKBO TKShHealing
    TKPrim TKTopAlgo TKGeomAlgo TKBRep
    # Modeling data + geometry
    TKGeomBase TKG3d TKG2d TKMath
    # Foundation
    TKernel
)

# Emscripten output tuned to how the OCCT worker consumes the module:
# ES6 module + Embind + growable memory. Mirrors the flags in the
# Option-A kapy-occt.yml so the two builds stay swap-compatible.
target_link_options(kapy-occt PRIVATE
    -O3
    --bind
    -fwasm-exceptions
    -sWASM_LEGACY_EXCEPTIONS=1
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    # The module loads in the OCCT WebWorker and in the Node test harness, so
    # `node` stays in ENVIRONMENT — and emscripten rejects EXPORT_ES6 with a
    # node environment unless import.meta is available.
    -sENVIRONMENT=web,worker,node
    -sUSE_ES6_IMPORT_META=1
    -sALLOW_MEMORY_GROWTH=1
    -sMAXIMUM_MEMORY=4GB
    # OCCT recurses deeply in fillet/boolean/blend evaluation; the
    # emscripten default 64KB stack overflows on non-trivial models and
    # (with no stack check in the shipping build) silently corrupts memory
    # -> 'memory access out of bounds'. opencascade.js links 8MB; match it.
    -sSTACK_SIZE=8MB
    # HEAPU8: the host copies bytes between this memory and Rust's through it
    # (the C API, bindings/kapy_capi.h; its KAPY_API functions are exported by
    # EMSCRIPTEN_KEEPALIVE).
    # FS: the STEP/STL readers and writers go through emscripten's virtual FS.
    # getExceptionMessage: occtApi decodes a thrown Standard_Failure through it;
    # without it every OCCT error reaches the UI as "OCCT error 0x1234". Under
    # native exceptions a throw reaches JS as a WebAssembly.Exception, and the
    # helper that reads it is only emitted with EXPORT_EXCEPTION_HANDLING_HELPERS.
    -sEXPORT_EXCEPTION_HANDLING_HELPERS
    -sEXPORTED_RUNTIME_METHODS=['FS','getExceptionMessage','HEAPU8']
    -sEXPORT_NAME=initKapyOcct
    -sASSERTIONS=0
)

set_target_properties(kapy-occt PROPERTIES OUTPUT_NAME "kapy-occt" SUFFIX ".js")
