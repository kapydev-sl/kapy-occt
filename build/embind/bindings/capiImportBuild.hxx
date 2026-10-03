// engine/kernels/occt/build/embind/bindings/capiImportBuild.hxx
//
// Building a B-Rep out of a mesh whose regions Rust grew: the blob reader for
// the regions and the build itself.
//
// Who includes it: capiImportBuild.cpp, capiImportMesh.cpp, capiChannel.cpp.
// What does NOT belong here: naming or storing the result.

#pragma once

#include <cstdint>
#include <vector>

#include <TopoDS_Shape.hxx>

#include "capiBlob.hxx"
#include "capiImportCache.hxx"

namespace kapy_capi {

// How a mesh is sewn and merged.
struct Params {
    double sewingTol, angleTol, distTol;
    bool detectPlanar;
};

// The vertex positions (u32 float count and the f32 values) and the regions
// that follow them in a blob: u32 region count and per region f64[3] normal,
// f64[3] origin, u32 outline count and its indices, u32 hole count and per hole
// u32 count and its indices.
std::vector<MeshRegion> readRegions(Blob& in, std::vector<float>& positions);

// The faces of `shape`.
int countFacesOf(const TopoDS_Shape& shape);

// The B-Rep of the mesh (see capiImportBuild.cpp). Throws OpError when no
// region builds a face or the sewing is null.
BuiltMesh buildMeshShape(const std::vector<float>& positions, const std::vector<MeshRegion>& regions,
                         const Params& p, uint32_t inputTris);

}  // namespace kapy_capi
