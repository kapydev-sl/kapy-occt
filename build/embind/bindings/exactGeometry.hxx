// services/occt/build/embind/bindings/exactGeometry.hxx
//
// The frames of a shape set's geometry, written exactly and put back after it
// is read (see exactGeometry.cpp).
//
// Who includes this: exactBrep.cpp.
// What does NOT belong here: locations (exactBrep.cpp), or anything else.

#pragma once

#include <BinTools_ShapeSet.hxx>

#include <istream>
#include <ostream>

namespace kapy_exact {

// Every frame of every surface, curve and 2D curve of `set`, in its tables'
// order. `set` must have had its shapes added.
void writeFrames(std::ostream& os, BinTools_ShapeSet& set);

// Put the frames `writeFrames` wrote back into `set`, whose geometry tables
// were just read. Throws `Standard_Failure` when the two do not line up.
void restoreFrames(std::istream& is, BinTools_ShapeSet& set);

} // namespace kapy_exact
