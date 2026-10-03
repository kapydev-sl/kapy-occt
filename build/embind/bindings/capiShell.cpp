// engine/kernels/occt/build/embind/bindings/capiShell.cpp
//
// Thick solid (the hollow of the Shell tool) through the C API: `runThickSolid`
// of the binding with its ladder handed over as data. The openings are the
// faces the blob names on the previous body; the rungs (a tolerance and a
// join each) are tried in order, then the cleanup rungs on a body rebuilt
// without its slivers (capiShellCleanup.cpp); the winner names the result
// through its history.
//
// A body of several solids is hollowed solid by solid (the ones without an
// opening stay solid) and named through the history of all the makers at once.
// A failure stores nothing: an index out of range fails in the words the shell
// always gave, and a solid no attempt could hollow answers `KAPY_E_NO_RESULT`
// with its index.
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
// rebuild (capiShellCleanup.cpp), the facts probe (capiShellFacts.cpp).

#include "capiShell.hxx"

#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepOffset_Mode.hxx>
#include <BRep_Builder.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Message_ProgressRange.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>

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

// The ladder, then the cleanup attempts, on one solid: the fault when none builds.
ThickResult attempt(const TopoDS_Shape& shape, const TopTools_ListOfShape& closing,
                    const std::vector<TopoDS_Shape>& removed, double thickness,
                    const std::vector<Rung>& rungs, const std::vector<Rung>& cleanup,
                    uint32_t index) {
    ThickResult made = makeThickSolid(shape, closing, thickness, rungs, signedVolume(shape));
    if (!made.op) made = shellCleaned(shape, removed, thickness, cleanup);
    if (!made.op) {
        throw NoResultError("no shell attempt built solid " + std::to_string(index),
                            static_cast<int32_t>(index));
    }
    return made;
}

// A body of several solids is hollowed solid by solid: the ones without a
// picked face stay solid, the results meet in a compound and every element is
// named through the history of all the makers.
uint32_t shellEachSolid(Entry& prev, const ShellPieces& split, double thickness,
                        const std::vector<Rung>& rungs, const std::vector<Rung>& cleanup,
                        const std::string& bornIn) {
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    std::vector<ThickResult> kept;
    for (size_t p = 0; p < split.pieces.size(); ++p) {
        const ShellPiece& piece = split.pieces[p];
        if (piece.removed.empty()) {
            builder.Add(compound, piece.shape);
            continue;
        }
        kept.push_back(attempt(piece.shape, piece.closing, piece.removed, thickness, rungs,
                               cleanup, static_cast<uint32_t>(p)));
        builder.Add(compound, kept.back().result);
    }
    if (kept.empty()) throw NoResultError("no solid of the body has an opening", 0);
    std::vector<BRepBuilderAPI_MakeShape*> makers;
    for (ThickResult& made : kept) makers.push_back(made.op.get());
    return finishHistoryOfMakers(compound, makers, prev, bornIn, /*unify=*/true);
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
    ensureMaps(prev);
    checkShellFaces(prev, indices);
    const ShellPieces split = shellPieces(prev, indices);
    if (!split.whole) return shellEachSolid(prev, split, thickness, rungs, cleanup, bornIn);

    const ShellPiece& body = split.pieces[0];
    const ThickResult made =
        attempt(body.shape, body.closing, body.removed, thickness, rungs, cleanup, 0);
    return finishHistory(made.result, *made.op, prev, nullptr, bornIn, /*unify=*/true);
}

}  // namespace

namespace kapy_capi {

void checkShellFaces(const Entry& prev, const std::vector<uint32_t>& indices) {
    if (indices.empty()) throw OpError("Shell needs at least one face to remove");
    const int count = prev.faces.Extent();
    for (const uint32_t idx : indices) {
        if (static_cast<int>(idx) >= count) {
            throw OpError("Shell face index " + std::to_string(idx) + " out of range [0, " +
                          std::to_string(count - 1) + "]");
        }
    }
}

ShellPieces shellPieces(const Entry& prev, const std::vector<uint32_t>& indices) {
    ShellPieces split{true, {}};
    TopTools_IndexedMapOfShape solids;
    TopExp::MapShapes(prev.shape, TopAbs_SOLID, solids);
    if (solids.Extent() <= 1) {
        split.pieces.emplace_back();
        ShellPiece& body = split.pieces.back();
        body.shape = prev.shape;
        for (const uint32_t idx : indices) {
            const TopoDS_Face face = TopoDS::Face(prev.faces.FindKey(static_cast<int>(idx) + 1));
            body.removed.push_back(face);
            body.closing.Append(face);
        }
        return split;
    }
    split.whole = false;
    for (int p = 1; p <= solids.Extent(); ++p) {
        split.pieces.emplace_back();
        ShellPiece& piece = split.pieces.back();
        piece.shape = solids.FindKey(p);
        TopTools_IndexedMapOfShape faceMap;
        TopExp::MapShapes(piece.shape, TopAbs_FACE, faceMap);
        for (const uint32_t idx : indices) {
            const int at = faceMap.FindIndex(prev.faces.FindKey(static_cast<int>(idx) + 1));
            if (at > 0) {
                piece.closing.Append(faceMap.FindKey(at));
                piece.removed.push_back(faceMap.FindKey(at));
            }
        }
    }
    return split;
}

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
