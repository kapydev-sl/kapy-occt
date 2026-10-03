// engine/kernels/occt/build/embind/bindings/capiTopo.cpp
//
// Two questions of a body's topology, in the index order of the store's maps
// (the order every TopoId resolves through):
//   face edges   per face, the edges that bound it in the order the explorer
//                walks them, without repeats (binding/topoAdjacency.ts,
//                `computeFaceEdges`)
//   seam edges   one byte per edge: 1 for an edge that is only the way OCCT
//                writes a periodic face down (a face walking the same edge
//                twice) or one it kept to close a boundary (no length), 0 for a
//                real edge; an empty answer when none is masked
//                (binding/seamEdges.ts, `classifySeamEdges`)
//
// Blob: u32 handle. Answers: face edges `u32 faces`, then per face `u32 n` and
// `n` u32 edge indices; seam edges one byte per edge, or nothing.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// `seamMask` is shared with the mesh question (capiMesh.cpp), which answers the
// mask beside the triangles.
// What does NOT belong here: the store, the arena, the maps' construction.

#include <algorithm>
#include <vector>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include "capiAsk.hxx"
#include "capiMeshCore.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('topoAdjacency') and of the seam question (which has no
// binding method of its own, so the name is the question's).
constexpr size_t SEED_ADJACENCY = 1432097120u;
constexpr size_t SEED_SEAM = 979458395u;
// An edge shorter than this is one OCCT kept to close a boundary.
constexpr double DEGENERATE_LENGTH = 1e-7;

void faceEdges(Entry& entry, Out& out) {
    ensureMaps(entry);
    const int faceCount = entry.faces.Extent();
    out.u32(static_cast<uint32_t>(faceCount));
    for (int fi = 1; fi <= faceCount; fi++) {
        std::vector<uint32_t> edges;
        for (TopExp_Explorer ex(entry.faces.FindKey(fi), TopAbs_EDGE); ex.More(); ex.Next()) {
            const int index = entry.edges.FindIndex(ex.Current()) - 1;
            if (index < 0) continue;
            const uint32_t at = static_cast<uint32_t>(index);
            if (std::find(edges.begin(), edges.end(), at) == edges.end()) edges.push_back(at);
        }
        out.u32(static_cast<uint32_t>(edges.size()));
        for (uint32_t e : edges) out.u32(e);
    }
}

}  // namespace

namespace kapy_capi {

std::vector<uint8_t> seamMask(Entry& entry) {
    ensureMaps(entry);
    const int edgeCount = entry.edges.Extent();
    std::vector<uint8_t> mask(static_cast<size_t>(edgeCount), 0);
    bool any = false;
    for (int fi = 1; fi <= entry.faces.Extent(); fi++) {
        std::vector<uint8_t> seen(static_cast<size_t>(edgeCount), 0);
        for (TopExp_Explorer ex(entry.faces.FindKey(fi), TopAbs_EDGE); ex.More(); ex.Next()) {
            const int index = entry.edges.FindIndex(ex.Current());
            if (index <= 0) continue;
            if (seen[index - 1]) {
                mask[index - 1] = 1;
                any = true;
            } else {
                seen[index - 1] = 1;
            }
        }
    }
    for (int ei = 1; ei <= edgeCount; ei++) {
        if (mask[ei - 1]) continue;
        GProp_GProps props;
        BRepGProp::LinearProperties(entry.edges.FindKey(ei), props, false, false);
        if (props.Mass() <= DEGENERATE_LENGTH) {
            mask[ei - 1] = 1;
            any = true;
        }
    }
    if (!any) mask.clear();
    return mask;
}

}  // namespace kapy_capi

KAPY_API int32_t kapy_face_edges(uint32_t ptr, uint32_t length) noexcept {
    return ask("topoAdjacency", SEED_ADJACENCY, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        faceEdges(entry, out);
    });
}

KAPY_API int32_t kapy_seam_edges(uint32_t ptr, uint32_t length) noexcept {
    return ask("seamEdges", SEED_SEAM, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        for (uint8_t m : seamMask(entry)) out.u8(m);
    });
}
