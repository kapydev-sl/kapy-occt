// services/occt/build/embind/bindings/capiOffset.cpp
//
// Offset of a whole solid by a signed distance through the C API:
// `runOffsetSolid` of the binding (`runOffsetSolid.ts`). The arc join goes
// straight to the join builder (the simple variant cannot round a convex
// edge); the intersection join tries the simple variant first and then the
// join builder through the same three tolerances the shell uses. Every
// attempt is gated on a usable result (no collapsed face, valid or healed, a
// solid), the winner is oriented forward and has its tolerance capped, and
// it is named through the operator's history.
//
// A refusal (no attempt built, a zero distance) answers `KAPY_E_DECLINED` and
// stores nothing, so the host redoes it on the JSON path and the binding words
// the failure.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/encode_k7.rs):
//   offsetSolid  bornIn, u32 previous, f64 offset, u8 join (0 arc, 1 intersection)
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the offset of faces (capiOffsetFaces.cpp), the
// shell (capiShell.cpp).

#include <cmath>
#include <memory>

#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffset_Mode.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Message_ProgressRange.hxx>
#include <TopTools_ListOfShape.hxx>

#include "capiFinish.hxx"
#include "capiGuards.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('offsetSolidShape').
constexpr size_t SEED_OFFSET = 795966039u;

// Tolerances tried in order (mm): tight first, then looser values that let the
// join algorithm close a solid it rejects at 1e-3.
constexpr double OFFSET_TOLS[] = {1e-3, 1e-2, 1e-1};

// The operator that built and the shape it built.
struct Made {
    std::unique_ptr<BRepOffsetAPI_MakeThickSolid> op;
    TopoDS_Shape result;
};

// A usable solid out of an attempt's shape, or null: no collapsed face, valid
// B-Rep (repaired once if it is not), and a solid (a compound comes back as a
// bare shell, which is sewn into the solid it encloses).
TopoDS_Shape acceptOffsetResult(const TopoDS_Shape& r) {
    if (r.IsNull()) return TopoDS_Shape();
    if (hasDegenerateFace(r)) return TopoDS_Shape();
    const TopoDS_Shape fixed = validOrHealed(r);
    if (fixed.IsNull()) return TopoDS_Shape();
    return ensureSolid(fixed);
}

// The whole-solid offset with no faces removed; nothing is built twice
// (MakeThickSolidBySimple builds on its own, so no Build call follows).
Made attemptSimple(const TopoDS_Shape& shape, double offset) {
    std::unique_ptr<BRepOffsetAPI_MakeThickSolid> op(new BRepOffsetAPI_MakeThickSolid());
    try {
        op->MakeThickSolidBySimple(shape, offset);
        const TopoDS_Shape kept = acceptOffsetResult(op->Shape());
        if (!kept.IsNull()) return {std::move(op), kept};
    } catch (...) {
        // Present but failed on this shape: fall back.
    }
    return Made();
}

// The join variant with an empty opening list, through the tolerances. The
// intersection join also asks for face-face intersections (a box grows to a
// bigger box, not a rounded one) and drops the seam edges it leaves.
Made attemptByJoin(const TopoDS_Shape& shape, double offset, bool arc) {
    for (const double tol : OFFSET_TOLS) {
        const Message_ProgressRange range;
        std::unique_ptr<BRepOffsetAPI_MakeThickSolid> op(new BRepOffsetAPI_MakeThickSolid());
        const TopTools_ListOfShape empty;
        try {
            op->MakeThickSolidByJoin(shape, empty, offset, tol, BRepOffset_Skin, !arc, false,
                                     arc ? GeomAbs_Arc : GeomAbs_Intersection, !arc, range);
            op->Build(range);
            if (op->IsDone()) {
                const TopoDS_Shape kept = acceptOffsetResult(op->Shape());
                if (!kept.IsNull()) return {std::move(op), kept};
            }
        } catch (...) {
            // Tolerance too tight for the kernel here: try the next one.
        }
    }
    return Made();
}

// A solid with a positive signed volume. The join builder can hand back one
// whose orientation is reversed: valid, with a positive |volume|, and every
// boolean treats it as empty.
TopoDS_Shape forward(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props, true, false, false);
    return props.Mass() < 0 ? shape.Reversed() : shape;
}

uint32_t offsetOf(Blob& in) {
    const std::string bornIn = in.str();
    const uint32_t previous = in.u32();
    const double offset = in.f64();
    const bool arc = in.u8() == 0;
    if (!in.done()) throw BlobError();

    Entry& prev = need(previous);
    return declining("offset: the kernel raised", [&]() -> uint32_t {
        if (!(std::fabs(offset) > 0)) throw DeclinedError("offset: distance must be non-zero");
        Made made = arc ? attemptByJoin(prev.shape, offset, true) : attemptSimple(prev.shape, offset);
        if (!arc && !made.op) made = attemptByJoin(prev.shape, offset, false);
        if (!made.op) throw DeclinedError("offset: no attempt built");
        const TopoDS_Shape result = forward(made.result);
        limitTolerance(result, TOLERANCE_CAP);
        return finishHistory(result, *made.op, prev, nullptr, bornIn, /*unify=*/true);
    });
}

}  // namespace

KAPY_API int32_t kapy_offset_solid(uint32_t ptr, uint32_t length) noexcept {
    return runOp("offsetSolidShape", SEED_OFFSET, ptr, length, offsetOf);
}
