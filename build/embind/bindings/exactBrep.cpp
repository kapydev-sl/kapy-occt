// services/occt/build/embind/bindings/exactBrep.cpp
//
// The embind door of the exact B-Rep: `Kapy_WriteShapeExact` and
// `Kapy_ReadShapeExact`, which move the bytes of exactBrepCore.cpp through a
// file of emscripten's FS. This is the route the TypeScript binding takes when
// the C API (capiBrepIo.cpp) is not the one asked, and the one the judges
// compare it against.
//
// Who includes this: the embind link (see ../CMakeLists.txt). Its caller is
// src/cad/workers/occt/binding/brepIo.ts.
// What does NOT belong here: the format (exactBrepCore.cpp), STEP / STL
// exchange (io.cpp), or what a cache holds.

#include <emscripten/bind.h>

#include <fstream>
#include <iterator>
#include <string>

#include <TopoDS_Shape.hxx>

#include "exactBrep.hxx"
#include "exactRegistry.hxx"

using namespace emscripten;

namespace {

// Write `shape` to `file`. Answers "" when it is written, and why not
// otherwise (see exactBrep.hxx).
std::string Kapy_WriteShapeExact(const TopoDS_Shape& shape, std::string file) {
    std::string bytes;
    const std::string refusal = kapy_exact::writeExact(shape, bytes);
    if (!refusal.empty()) return refusal;
    std::ofstream os(file, std::ios::out | std::ios::binary | std::ios::trunc);
    os.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    os.flush();
    return os.good() ? "" : "failed";
}

bool Kapy_ReadShapeExact(TopoDS_Shape& shape, std::string file) {
    std::ifstream is(file, std::ios::in | std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    return kapy_exact::readExact(bytes, shape);
}

} // namespace

EMSCRIPTEN_BINDINGS(kapy_exact_brep) {
    function("Kapy_WriteShapeExact", &Kapy_WriteShapeExact);
    function("Kapy_ReadShapeExact", &Kapy_ReadShapeExact);
    function("Kapy_ExactBrepExcludeForTest", &kapy_exact::excludeForTest);
}
