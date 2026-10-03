// engine/kernels/occt/build/embind/bindings/capiQuery.cpp
//
// Two questions the editor asks about named elements:
//   min distance   the smallest distance between two parts of stored bodies,
//                  each a whole body or one of its faces, edges or vertices
//                  (BRepExtrema_DistShapeShape)
//   face boundary  the boundary of one face as the sketch materialiser reads
//                  it: the ordered walk of every wire, then a sweep for the
//                  edges the walk never reached
// Both answer INDICES (of the store's maps); what a table says about those
// elements (roles, born-in, endpoints) is the core's to add.
//
// Min distance blob: two targets of `u32 handle, u8 kind, u32 index` (kind 0
// whole body, 1 face, 2 edge, 3 vertex; the index is ignored for 0). Answers
// one f64, or nothing when an index is out of range or the solve does not
// converge.
//
// Face boundary blob: `u32 handle, u32 face`. Answers `u32 events`, then per
// event `u8 phase` (0: the ordered walk, 1: the sweep), `u32 edge` (the edge's
// index, or 0xffffffff for one that is not an edge: a seam, a degenerate one),
// `u32 n` and `n` u32 vertex indices of that edge. An edge outside the face's
// range answers nothing. Seams and degenerate edges are not edges, but their
// vertices are real corners, so they are still reported (without an edge).
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: naming (the core's), 2D projection.

#include <algorithm>
#include <vector>

#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <GProp_GProps.hxx>
#include <Message_ProgressRange.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>

#include "capiAsk.hxx"

using namespace kapy_capi;

namespace {

// serialSeedOf('runMinDistance') and of 'getFaceBoundaryEdgesWithTopo'.
constexpr size_t SEED_MIN_DISTANCE = 1194878450u;
constexpr size_t SEED_FACE_BOUNDARY = 1493542579u;
constexpr uint32_t NO_EDGE = 0xffffffffu;
// An edge shorter than this is one OCCT kept to close a boundary.
constexpr double DEGENERATE_LENGTH = 1e-7;

// The part of a stored body one target names; a null shape when the index is
// outside the map.
TopoDS_Shape target(Blob& in) {
    Entry& entry = need(in.u32());
    const uint8_t kind = in.u8();
    const uint32_t index = in.u32();
    if (kind == 0) return entry.shape;
    ensureMaps(entry);
    const TopTools_IndexedMapOfShape& map =
        kind == 1 ? entry.faces : kind == 2 ? entry.edges : entry.vertices;
    if (kind > 3 || index >= static_cast<uint32_t>(map.Extent())) return TopoDS_Shape();
    return map.FindKey(static_cast<int>(index) + 1);
}

// The indices of the vertices of `edge`, each once, in explorer order.
std::vector<uint32_t> vertexIndices(const Entry& entry, const TopoDS_Shape& edge) {
    std::vector<uint32_t> out;
    for (TopExp_Explorer ex(edge, TopAbs_VERTEX); ex.More(); ex.Next()) {
        const int index = entry.vertices.FindIndex(ex.Current());
        if (index > 0) out.push_back(static_cast<uint32_t>(index - 1));
    }
    return out;
}

void event(Out& out, uint8_t phase, uint32_t edge, const std::vector<uint32_t>& vertices) {
    out.u8(phase);
    out.u32(edge);
    out.u32(static_cast<uint32_t>(vertices.size()));
    for (uint32_t v : vertices) out.u32(v);
}

// The edges of `face` that are artefacts rather than edges: the ones it walks
// twice (its parametrisation seams) and anything with no length.
std::vector<bool> artefacts(Entry& entry, const TopoDS_Shape& face) {
    std::vector<bool> skip(static_cast<size_t>(entry.edges.Extent()) + 1, false);
    std::vector<bool> seen(skip.size(), false);
    for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
        const int index = entry.edges.FindIndex(ex.Current());
        if (index <= 0) continue;
        if (seen[index]) skip[index] = true;
        seen[index] = true;
    }
    for (int i = 1; i <= entry.edges.Extent(); i++) {
        if (skip[i]) continue;
        GProp_GProps props;
        BRepGProp::LinearProperties(entry.edges.FindKey(i), props, false, false);
        if (props.Mass() <= DEGENERATE_LENGTH) skip[i] = true;
    }
    return skip;
}

void faceBoundary(Entry& entry, uint32_t faceIndex, Out& out) {
    ensureMaps(entry);
    if (faceIndex >= static_cast<uint32_t>(entry.faces.Extent())) return;
    const TopoDS_Shape face = entry.faces.FindKey(static_cast<int>(faceIndex) + 1);
    const std::vector<bool> skip = artefacts(entry, face);
    uint32_t count = 0;
    Out body;
    for (TopExp_Explorer wires(face, TopAbs_WIRE); wires.More(); wires.Next()) {
        for (BRepTools_WireExplorer walk(TopoDS::Wire(wires.Current())); walk.More();
             walk.Next()) {
            const int index = entry.edges.FindIndex(walk.Current());
            const bool real = index > 0 && !skip[index];
            event(body, 0, real ? static_cast<uint32_t>(index - 1) : NO_EDGE,
                  vertexIndices(entry, walk.Current()));
            ++count;
        }
    }
    for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
        const int index = entry.edges.FindIndex(ex.Current());
        if (index <= 0 || skip[index]) continue;
        event(body, 1, static_cast<uint32_t>(index - 1), vertexIndices(entry, ex.Current()));
        ++count;
    }
    out.u32(count);
    out.bytes(body.bytes().data(), body.bytes().size());
}

}  // namespace

KAPY_API int32_t kapy_min_distance(uint32_t ptr, uint32_t length) noexcept {
    return ask("runMinDistance", SEED_MIN_DISTANCE, ptr, length, [](Blob& in, Out& out) {
        const TopoDS_Shape a = target(in);
        const TopoDS_Shape b = target(in);
        if (a.IsNull() || b.IsNull()) return;
        BRepExtrema_DistShapeShape distance;
        distance.LoadS1(a);
        distance.LoadS2(b);
        distance.Perform(Message_ProgressRange());
        if (distance.IsDone()) out.f64(distance.Value());
    });
}

KAPY_API int32_t kapy_face_boundary(uint32_t ptr, uint32_t length) noexcept {
    return ask("getFaceBoundaryEdgesWithTopo", SEED_FACE_BOUNDARY, ptr, length,
               [](Blob& in, Out& out) {
                   Entry& entry = need(in.u32());
                   faceBoundary(entry, in.u32(), out);
               });
}
