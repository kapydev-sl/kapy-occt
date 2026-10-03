// engine/kernels/occt/build/embind/bindings/exactBrep.cpp
//
// The embind lever of the exact B-Rep's registry: `Kapy_ExactBrepExcludeForTest`
// takes one geometry class out of it, so a test can hold the writer to its
// refusal of a class it does not know it writes exactly. The bytes themselves
// move through the C API (`kapy_brep_write` / `kapy_brep_read`, capiBrepIo.cpp);
// the former embind pair that moved them through a file of emscripten's FS had
// no caller left and is gone.
//
// Who includes this: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the format (exactBrepCore.cpp), STEP / STL
// exchange (io.cpp), or what a cache holds.

#include <emscripten/bind.h>

#include "exactRegistry.hxx"

using namespace emscripten;

EMSCRIPTEN_BINDINGS(kapy_exact_brep) {
    function("Kapy_ExactBrepExcludeForTest", &kapy_exact::excludeForTest);
}
