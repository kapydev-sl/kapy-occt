// engine/kernels/occt/build/embind/bindings/capiImportCache.hxx
//
// What the two C imports keep between calls: the parsed STEP base shape and
// the built mesh shape, each under the content hash the feature carries. A
// call with a key already here parses or builds nothing and hands out an
// independent copy, so the cached base survives the release of what was handed
// out (the host keeps no copy of either map). A store reset forgets them: every shape of the old kernel is gone.
//
// It also holds the one thing the mesh import's two files share: a planar
// region as Rust grew it, and the face built out of one.
//
// Who includes it: capiImportStep.cpp, capiImportMesh.cpp,
// capiImportRegion.cpp, capiImportCache.cpp, capiStore.cpp.
// What does NOT belong here: how a file is parsed or a mesh is sewn.

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace kapy_capi {

// A mesh turned into B-Rep, and what the import dialog reports about it.
struct BuiltMesh {
    TopoDS_Shape shape;
    uint32_t inputTris = 0;
    uint32_t regions = 0;
    uint32_t outputFaces = 0;
    bool isSolid = false;
};

// contentHash -> parsed base shape.
std::unordered_map<std::string, TopoDS_Shape>& stepCache();

// contentHash -> built mesh shape and stats.
std::unordered_map<std::string, BuiltMesh>& meshCache();

// A planar region of a mesh: its plane, its outline and its holes as indices
// into the vertex positions (`mesh/region_grow.rs` grew it).
struct MeshRegion {
    double normal[3];
    double origin[3];
    std::vector<uint32_t> outer;
    std::vector<std::vector<uint32_t>> inner;
};

// The planar face of `region` over `positions` (xyz floats), or a null shape
// for a region that cannot make one: an outline of fewer than three vertices,
// a normal with no length, a wire or a face OCCT does not finish
// (`importMesh.faceBuild.ts`).
TopoDS_Shape buildRegionFace(const std::vector<float>& positions, const MeshRegion& region);

// Forget both caches.
void clearImportCaches();

}  // namespace kapy_capi
