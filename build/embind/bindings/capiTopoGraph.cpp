// services/occt/build/embind/bindings/capiTopoGraph.cpp
//
// The vertices that bound a body's faces and edges, in the index order of the
// store's maps: the part of a body's boundary graph the edges-of-faces question
// (capiTopo.cpp) does not answer. With the edges of each face that question
// gives, the Rust core derives every adjacency the blend recipe and the query
// engine read (which faces an edge bounds, which edges meet at a vertex).
//
// Blob: u32 handle. Answers: `u32 faces`, per face `u32 n` and `n` u32 vertex
// indices (the order the explorer walks them, without repeats), then `u32 edges`
// and the same per edge.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the edges of a face (capiTopo.cpp), the maps'
// construction, anything the core's naming table already holds.

#include <algorithm>
#include <vector>

#include <TopExp_Explorer.hxx>

#include "capiAsk.hxx"

using namespace kapy_capi;

namespace {

// serialSeedOf('blendTopology').
constexpr size_t SEED_TOPOLOGY = 754815721u;

// The vertices of `shape` the explorer walks, once each, as indices of `map`.
void verticesOf(const TopoDS_Shape& shape, const TopTools_IndexedMapOfShape& map, Out& out) {
    std::vector<uint32_t> found;
    for (TopExp_Explorer ex(shape, TopAbs_VERTEX); ex.More(); ex.Next()) {
        const int index = map.FindIndex(ex.Current()) - 1;
        if (index < 0) continue;
        const uint32_t at = static_cast<uint32_t>(index);
        if (std::find(found.begin(), found.end(), at) == found.end()) found.push_back(at);
    }
    out.u32(static_cast<uint32_t>(found.size()));
    for (uint32_t v : found) out.u32(v);
}

}  // namespace

KAPY_API int32_t kapy_topology(uint32_t ptr, uint32_t length) noexcept {
    return ask("blendTopology", SEED_TOPOLOGY, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        ensureMaps(entry);
        out.u32(static_cast<uint32_t>(entry.faces.Extent()));
        for (int f = 1; f <= entry.faces.Extent(); ++f) {
            verticesOf(entry.faces.FindKey(f), entry.vertices, out);
        }
        out.u32(static_cast<uint32_t>(entry.edges.Extent()));
        for (int e = 1; e <= entry.edges.Extent(); ++e) {
            verticesOf(entry.edges.FindKey(e), entry.vertices, out);
        }
    });
}
