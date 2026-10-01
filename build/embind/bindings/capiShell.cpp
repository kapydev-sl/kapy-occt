// services/occt/build/embind/bindings/capiShell.cpp
//
// Thick solid (the hollow of the Shell tool) through the C API: `runThickSolid`
// of the binding with its ladder handed over as data. The openings are the
// faces the blob names on the previous body; the rungs (a tolerance and a
// join each) are tried in order, then the cleanup rungs on a body rebuilt
// without its slivers (capiShellCleanup.cpp); the winner names the result
// through its history.
//
// A body of several solids is declined: the binding shells it solid by solid
// and names the pieces through several makers at once, which the JSON path
// still does. So is every failure (no attempt built, an index out of range):
// nothing has been stored, and the binding words the fault.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/encode_k7.rs):
//   thickSolid  bornIn, u32 previous, f64 thickness,
//               u32 nFaces, nFaces x u32 face index,
//               u32 nRungs, nRungs x (f64 tol, u8 join), then the same for the
//               cleanup rungs (join: 0 arc, 1 intersection)
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the ladder's contents (Rust), the cleanup
// rebuild (capiShellCleanup.cpp), the facts probe (it stays on JSON).

#include "capiShell.hxx"

#include <BRepOffset_Mode.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Message_ProgressRange.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include "capiFinish.hxx"
#include "capiGuards.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('thickSolidShape').
constexpr size_t SEED_THICK = 1502242177u;

// A result judged: the shape to keep (null when the attempt is unusable) and
// whether BRepCheck is happy with it (`acceptShellResult`).
struct Accepted {
    TopoDS_Shape shape;
    bool valid;
};

Accepted acceptShellResult(const TopoDS_Shape& result, double inputVolume) {
    if (result.IsNull()) return {TopoDS_Shape(), false};
    if (hasDegenerateFace(result)) return {TopoDS_Shape(), false};
    const TopoDS_Shape solid = ensureSolid(result);
    if (solid.IsNull() || !changedSolidVolume(solid, inputVolume)) {
        return {TopoDS_Shape(), false};
    }
    const TopoDS_Shape fixed = validOrHealed(solid);
    if (fixed.IsNull()) return {solid, false};
    limitTolerance(fixed, TOLERANCE_CAP);
    return {fixed, true};
}

std::vector<Rung> readRungs(Blob& in) {
    std::vector<Rung> rungs;
    const uint32_t count = in.u32();
    for (uint32_t i = 0; i < count; ++i) {
        const double tol = in.f64();
        rungs.push_back({tol, in.u8() != 0});
    }
    return rungs;
}

uint32_t thickOf(Blob& in) {
    const std::string bornIn = in.str();
    const uint32_t previous = in.u32();
    const double thickness = in.f64();
    const uint32_t nFaces = in.u32();
    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < nFaces; ++i) indices.push_back(in.u32());
    const std::vector<Rung> rungs = readRungs(in);
    const std::vector<Rung> cleanup = readRungs(in);
    if (!in.done()) throw BlobError();

    Entry& prev = need(previous);
    return declining("shell: the kernel raised", [&]() -> uint32_t {
        ensureMaps(prev);
        if (indices.empty()) throw DeclinedError("shell: no face to remove");
        for (const uint32_t idx : indices) {
            if (static_cast<int>(idx) >= prev.faces.Extent()) {
                throw DeclinedError("shell: face index out of range");
            }
        }
        TopTools_IndexedMapOfShape pieces;
        TopExp::MapShapes(prev.shape, TopAbs_SOLID, pieces);
        if (pieces.Extent() > 1) throw DeclinedError("shell: a body of several solids");

        TopTools_ListOfShape closing;
        std::vector<TopoDS_Shape> removed;
        for (const uint32_t idx : indices) {
            const TopoDS_Face face = TopoDS::Face(prev.faces.FindKey(static_cast<int>(idx) + 1));
            removed.push_back(face);
            closing.Append(face);
        }
        ThickResult made =
            makeThickSolid(prev.shape, closing, thickness, rungs, signedVolume(prev.shape));
        if (!made.op) made = shellCleaned(prev.shape, removed, thickness, cleanup);
        if (!made.op) throw DeclinedError("shell: no attempt built");
        return finishHistory(made.result, *made.op, prev, nullptr, bornIn, /*unify=*/true);
    });
}

}  // namespace

namespace kapy_capi {

ThickResult makeThickSolid(const TopoDS_Shape& shape, const TopTools_ListOfShape& closing,
                           double thickness, const std::vector<Rung>& rungs, double inputVolume) {
    ThickResult fallback;
    for (const Rung& rung : rungs) {
        const Message_ProgressRange range;
        std::unique_ptr<BRepOffsetAPI_MakeThickSolid> op(new BRepOffsetAPI_MakeThickSolid());
        try {
            // The offset is negative for an inward shell: the walls grow
            // toward the interior, up to `thickness` from the outer boundary.
            op->MakeThickSolidByJoin(shape, closing, -thickness, rung.tol, BRepOffset_Skin, false,
                                     false, rung.intersection ? GeomAbs_Intersection : GeomAbs_Arc,
                                     false, range);
            op->Build(range);
            if (op->IsDone()) {
                Accepted kept = acceptShellResult(op->Shape(), inputVolume);
                if (!kept.shape.IsNull()) {
                    if (kept.valid) return {std::move(op), kept.shape};
                    if (!fallback.op) fallback = ThickResult{std::move(op), kept.shape};
                }
            }
        } catch (...) {
            // This configuration is too tight for the kernel: try the next.
        }
    }
    return fallback;
}

}  // namespace kapy_capi

KAPY_API int32_t kapy_thick_solid(uint32_t ptr, uint32_t length) noexcept {
    return runOp("thickSolidShape", SEED_THICK, ptr, length, thickOf);
}
