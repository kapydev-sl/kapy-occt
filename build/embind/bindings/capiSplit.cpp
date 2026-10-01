// services/occt/build/embind/bindings/capiSplit.cpp
//
// Take the top-level solids of a stored shape apart through the C API:
// `runSplitSolids` of the binding. Each solid becomes a stored shape of its own
// whose naming table is filtered from the parent's, and the pieces come back
// biggest first (ties by centroid), so "the biggest keeps the body" is stable.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/):
//   split_solids   bornIn, u32 handle
// Answer: u32 count (0 when there is nothing to split: fewer than two solids),
// then per piece u32 handle, f64 volume, f64[3] centroid.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: which piece keeps the body (the regen layer).

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "factsInternal.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('splitSolids').
constexpr size_t SEED_SPLIT = 1545489195u;
// Relative volume difference below which two pieces count as equally big.
constexpr double TIE_REL_TOL = 1e-9;

struct Piece {
    uint32_t handle;
    double volume;
    double centroid[3];
};

// Biggest first; equal volumes fall back to the centroid.
double byVolumeThenCentroid(const Piece& a, const Piece& b) {
    const double scale = std::max({std::fabs(a.volume), std::fabs(b.volume), 1.0});
    if (std::fabs(a.volume - b.volume) > TIE_REL_TOL * scale) return b.volume - a.volume;
    for (int k = 0; k < 3; ++k) {
        if (a.centroid[k] != b.centroid[k]) return a.centroid[k] - b.centroid[k];
    }
    return 0;
}

void put32(std::string& out, uint32_t v) { out.append(reinterpret_cast<const char*>(&v), 4); }
void put64(std::string& out, double v) { out.append(reinterpret_cast<const char*>(&v), 8); }

}  // namespace

KAPY_API int32_t kapy_split_solids(uint32_t ptr, uint32_t length) noexcept {
    return runRaw("splitSolids", SEED_SPLIT, ptr, length, [](Blob& in) -> int32_t {
        const std::string bornIn = in.str();
        Entry& parent = need(in.u32());
        TopTools_IndexedMapOfShape solids;
        TopExp::MapShapes(parent.shape, TopAbs_SOLID, solids);
        const int count = solids.Extent();
        std::string out;
        if (count < 2) {
            put32(out, 0);
            return answer(out.data(), out.size());
        }
        if (parent.tableId == 0) throw OpError("an operand namer carries no naming table");
        ensureMaps(parent);
        const kapy_facts::Maps parentMaps{&parent.faces, &parent.edges, &parent.vertices};

        std::vector<Piece> pieces;
        for (int i = 1; i <= count; ++i) {
            const TopoDS_Shape solid = TopoDS::Solid(solids.FindKey(i));
            GProp_GProps props;
            BRepGProp::VolumeProperties(solid, props, true, false, false);
            const gp_Pnt c = props.CentreOfMass();
            Piece piece{0, std::fabs(props.Mass()), {c.X(), c.Y(), c.Z()}};

            const Mapped maps(solid);
            const uint32_t id = allocTable();
            kapy_facts::buildSubShape(id, parent.tableId, maps.view(), parentMaps, bornIn);
            piece.handle = putNative(solid, id);
            kapy_facts::bind(id, "h_" + std::to_string(piece.handle));
            pieces.push_back(piece);
        }
        std::stable_sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) {
            return byVolumeThenCentroid(a, b) < 0;
        });
        put32(out, static_cast<uint32_t>(pieces.size()));
        for (const Piece& p : pieces) {
            put32(out, p.handle);
            put64(out, p.volume);
            for (double v : p.centroid) put64(out, v);
        }
        return answer(out.data(), out.size());
    });
}
