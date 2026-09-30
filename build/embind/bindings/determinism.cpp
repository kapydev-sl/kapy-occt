// services/occt/build/embind/bindings/determinism.cpp
//
// The counter the kernel numbers its objects with (patch 0002): a transient,
// and a shape through its TShape, hashes by the serial it was created under
// instead of by its address, so a map walks in the order things were made.
// The host sets the counter before each call into the kernel
// (src/cad/workers/occt/binding/serialSeed.ts), which makes the serials — and
// every map order an algorithm depends on — the same each time the same call
// runs on the same input, however many documents the process opened before.
//
// Who includes this: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: any other binding; when to reset (the host's).

#include <emscripten/bind.h>

#include <Standard_Transient.hxx>

using namespace emscripten;

// wasm32: size_t is 32 bits, and embind carries it as an unsigned int.
static void Kapy_SetSerialCounter(unsigned int next) {
    Standard_Transient::SetSerialCounter(static_cast<size_t>(next));
}
static unsigned int Kapy_SerialCounter() {
    return static_cast<unsigned int>(Standard_Transient::SerialCounter());
}

EMSCRIPTEN_BINDINGS(kapy_determinism) {
    function("Kapy_SetSerialCounter", &Kapy_SetSerialCounter);
    function("Kapy_SerialCounter", &Kapy_SerialCounter);
}
