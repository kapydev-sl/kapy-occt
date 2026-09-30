// services/occt/build/embind/bindings/capiStore.hxx
//
// The kernel's table of shapes, in C++: the one place a handle is minted,
// counted and dropped, for the embind side (the TypeScript shape cache mints
// and releases through it) and for the C API (Rust releases and measures by
// handle). A handle is `epoch << 24 | serial`: the serial counts up and is
// never reused inside an epoch, so with epoch 0 the ids are `h_1, h_2, ...`
// exactly as before, and an epoch bump (a recovery that rebuilt the kernel)
// makes every older id read "unknown" instead of aliasing a new shape.
//
// Who includes it: capiStore.cpp, capiOps.cpp, capiEmbind.cpp, capiCore.cpp.
// What does NOT belong here: the C entry points, the embind registrations,
// anything about a particular operation.

#pragma once

#include <cstdint>
#include <vector>

#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Shape.hxx>

namespace kapy_capi {

// One shape the store holds: the shape, how many owners it has, and its
// sub-shape maps, built the first time something asks for them.
struct Entry {
    TopoDS_Shape shape;
    uint32_t refs = 1;
    bool mapped = false;
    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedMapOfShape edges;
    TopTools_IndexedMapOfShape vertices;
};

// Put a shape in the table with one owner; answers its new handle.
uint32_t put(const TopoDS_Shape& shape);

// The entry of `handle`, or null when it was never minted, was released, or
// belongs to an older epoch.
Entry* find(uint32_t handle);

// One owner more; the new count, or 0 for an unknown handle.
uint32_t retain(uint32_t handle);

// One owner less. The remaining count, or -1 for an unknown handle. The last
// owner leaving erases the entry and queues the handle for `takeDropped`.
int32_t release(uint32_t handle, bool queueDropped);

// The faces, edges and vertices of an entry, indexed in OCCT's map order.
void ensureMaps(Entry& entry);

// Every entry gone. `bumpEpoch` makes the ids minted before read unknown from
// now on; without it the serial restarts at 1, the legacy recovery (a test
// that opens the next document in the same process).
void reset(bool bumpEpoch);

// The handles that reached zero through the C API since the last call, in
// order, and forgotten.
std::vector<uint32_t> takeDropped();

// How many entries are alive, and the current epoch.
uint32_t live();
uint32_t epoch();

}  // namespace kapy_capi
