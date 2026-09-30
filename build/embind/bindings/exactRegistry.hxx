// services/occt/build/embind/bindings/exactRegistry.hxx
//
// Which geometry classes the exact B-Rep knows it writes exactly, and how.
// A class not listed here — a new OCCT class, a subclass of a listed one —
// may carry a frame the shape set's reader re-derives, and would come back a
// few ulps off without anyone noticing. So the writer refuses it instead
// (exactGeometry.cpp): the cache is not written and the document opens by
// regenerating.
//
// Who includes this: exactGeometry.cpp and exactBrep.cpp.
// What does NOT belong here: writing or restoring a frame (exactGeometry.cpp).

#pragma once

#include <Standard_Type.hxx>

#include <string>

namespace kapy_exact {

// How a registered class's frame travels. NONE: registered as frameless
// (its every value is a double the shape set reads back exactly).
enum Kind : int {
    NONE = 0,
    FRAME = 1,
    CONIC = 2,
    TRIMMED = 3,
    OFFSET = 4,
    EXTRUSION = 5,
    REVOLUTION = 6,
};

// The table a geometry sits in.
enum Table : int { SURFACE = 0, CURVE = 1, CURVE2D = 2 };

// The kind of `type` in `table`, matched by exact class, or -1 when the class
// is not registered.
int registeredKind(Table table, const occ::handle<Standard_Type>& type);

// Treat the class named `name` as unregistered ("" registers everything
// again). Exists for the test that holds the refusal: a registry with one
// class removed must refuse, not write.
void excludeForTest(const std::string& name);

// Thrown by the writer at the first unregistered class it meets.
struct Unregistered {
    std::string className;
};

} // namespace kapy_exact
