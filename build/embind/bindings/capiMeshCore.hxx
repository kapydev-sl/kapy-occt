// engine/kernels/occt/build/embind/bindings/capiMeshCore.hxx
//
// What the C mesh questions share: one face's triangulation read out of OCCT
// and checked, the polyline of an edge, the area-weighted vertex normals, and
// a whole body meshed the way the TypeScript binding's `tessellate` meshes it
// (binding/tessellate.ts, tessellateFace.ts). Every number is the one the
// embind route computes: the same OCCT calls in the same order, and the f32
// arithmetic of the typed arrays it writes into, so the two transports give
// the same bytes.
//
// Who includes it: capiMesh*.cpp, capiTopo.cpp (the seam mask).
// What does NOT belong here: the C entry points, the answer layouts.

#pragma once

#include <cstdint>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include "capiStore.hxx"

namespace kapy_capi {

// A face's triangles: world-space xyz per node, 0-based node indices.
struct FaceTriangles {
    std::vector<double> positions;
    std::vector<int> triangles;
};

// What a body's tessellation holds, in the f32 / u32 the host reads.
struct BodyMesh {
    std::vector<float> positions;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> triFace;
    // One polyline (xyz as f32) per edge of the body's map, in map order.
    std::vector<std::vector<float>> edges;
};

// Math.hypot of three values with V8's own arithmetic (scaled by the largest,
// Kahan-summed), so a length is the one the JavaScript route computes.
double jsHypot3(double x, double y, double z);

// The triangulation BRepMesh left on `face`; false when it has none.
bool readFaceTriangles(const TopoDS_Shape& face, FaceTriangles& out);

// `first` when its area matches the planar face's, else the first re-mesh that
// does; `first` again when none does.
FaceTriangles checkedPlanarTriangles(const TopoDS_Shape& face, const FaceTriangles& first);

// The polyline an edge is drawn and picked with.
std::vector<float> edgePolyline(const TopoDS_Edge& edge);

// Area-weighted vertex normals of an indexed triangle list, accumulated in f32
// and normalised the way the typed-array route does it.
std::vector<float> vertexNormals(const std::vector<float>& positions,
                                 const std::vector<uint32_t>& indices);

// Mesh `entry`'s shape at the given deflections and read every face (in the
// store's face-map order) and every edge. `planar[i]` says face i is planar and
// is checked against its exact area; a face past the flags is not.
void meshBody(Entry& entry, double linear, double angular, const std::vector<uint8_t>& planar,
              BodyMesh& out);

// One byte per edge of `entry`'s map: 1 for an edge that is only how OCCT writes
// a periodic face down or one kept to close a boundary; empty when none is.
std::vector<uint8_t> seamMask(Entry& entry);

}  // namespace kapy_capi
