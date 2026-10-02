// services/occt/build/embind/bindings/exactBrep.hxx
//
// The exact B-Rep as bytes: a shape written so that reading it gives the shape
// back to the last bit (the format is described in exactBrepCore.cpp). The
// C API answers them in the result arena (capiBrepIo.cpp).
//
// Who includes it: exactBrepCore.cpp, capiBrepIo.cpp.
// What does NOT belong here: the format itself, or how the bytes travel.

#pragma once

#include <string>

#include <TopoDS_Shape.hxx>

namespace kapy_exact {

// Write `shape` into `bytes`. Answers "" when it is written, and why not
// otherwise: `unregisteredGeometry:<class>` for a geometry class the registry
// does not list (exactRegistry.cpp) — nothing is written, and the caller
// writes no cache — or `failed`.
std::string writeExact(const TopoDS_Shape& shape, std::string& bytes);

// Read what `writeExact` wrote; false when the bytes are not that.
bool readExact(const std::string& bytes, TopoDS_Shape& shape);

} // namespace kapy_exact
