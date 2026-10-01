// services/occt/build/embind/bindings/capiOffsetSlab.hxx
//
// The last resort for Offset Faces (`runOffsetFaces.slab.ts`): for the small
// distances a fit clearance uses, thicken each picked face into a thin slab of
// the offset distance and fuse it on (growing) or cut it away (shrinking),
// instead of moving the faces and re-stitching their neighbours.
//
// Who includes it: capiOffsetFaces.cpp, capiOffsetSlab.cpp.
// What does NOT belong here: the true offset, naming.

#pragma once

#include <vector>

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

namespace kapy_capi {

// `shape` with every face of `faces` offset by `distance` through slabs, or a
// null shape when a slab or a boolean fails.
TopoDS_Shape offsetFacesBySlabs(const TopoDS_Shape& shape, const std::vector<TopoDS_Face>& faces,
                                double distance);

}  // namespace kapy_capi
