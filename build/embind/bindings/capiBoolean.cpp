// services/occt/build/embind/bindings/capiBoolean.cpp
//
// Cut, fuse and common through the C API: `runBoolean` of the binding with
// its decisions already taken. The sequence is the binding's call for call,
// because every OCCT object it makes is numbered and the judges compare the
// numbering: an operator built empty and staged with SetArguments / SetTools
// (not the constructor that builds), the same options in the same order, one
// Build, the validity check, ShapeFix_Shape when the result fails it, then the
// naming (and the unify pass when asked).
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/):
//   boolean   u8 kind (0 cut, 1 fuse, 2 common), bornIn, u32 previous,
//             u32 tool, f64 fuzzy, u8 glue, u8 unify
// Fuzzy arrives resolved (the default is the Rust side's), glue and unify as
// the JavaScript truthiness of what the step passed.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the trim (capiTrim.cpp), the many-way fuse
// (capiFuseMany.cpp).

#include "capiBoolean.hxx"

#include <memory>

#include <BOPAlgo_GlueEnum.hxx>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <Message_ProgressRange.hxx>
#include <ShapeFix_Shape.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('cutShape'), ('fuseShape'), ('intersectShape').
constexpr size_t SEED_CUT = 556899046u;
constexpr size_t SEED_FUSE = 1286983026u;
constexpr size_t SEED_COMMON = 1303882545u;

// Red control (perturbation 4): every cut runs as a common, so whatever the
// cut was meant to remove is all that is left.
constexpr int PERTURB_CUT_AS_COMMON = 4;

size_t seedOf(uint8_t kind) {
    return kind == kCut ? SEED_CUT : kind == kFuse ? SEED_FUSE : SEED_COMMON;
}

size_t countOf(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind) {
    size_t n = 0;
    for (TopExp_Explorer it(shape, kind, TopAbs_SHAPE); it.More(); it.Next()) ++n;
    return n;
}

// ShapeFix_Shape on `shape`: the repaired shape, or null when it throws or
// does not come out valid (`healShape.ts`).
TopoDS_Shape heal(const TopoDS_Shape& shape) {
    try {
        opencascade::handle<ShapeFix_Shape> fix = new ShapeFix_Shape(shape);
        fix->Perform(Message_ProgressRange());
        const TopoDS_Shape fixed = fix->Shape();
        if (fixed.IsNull() || !isValid(fixed)) return TopoDS_Shape();
        return fixed;
    } catch (...) {
        return TopoDS_Shape();
    }
}

std::unique_ptr<BRepAlgoAPI_BooleanOperation> makeOperator(uint8_t kind) {
    if (kind == kCut) return std::unique_ptr<BRepAlgoAPI_BooleanOperation>(new BRepAlgoAPI_Cut());
    if (kind == kFuse) return std::unique_ptr<BRepAlgoAPI_BooleanOperation>(new BRepAlgoAPI_Fuse());
    return std::unique_ptr<BRepAlgoAPI_BooleanOperation>(new BRepAlgoAPI_Common());
}

const char* nameOf(uint8_t kind) { return kind == kCut ? "cut" : kind == kFuse ? "fuse" : "common"; }

const char* classOf(uint8_t kind) {
    return kind == kCut ? "Cut" : kind == kFuse ? "Fuse" : "Common";
}

}  // namespace

namespace kapy_capi {

uint32_t booleanOf(uint32_t previous, uint32_t tool, uint8_t kind, const std::string& bornIn,
                   double fuzzy, bool glue, bool unify) {
    Entry& prev = need(previous);
    Entry& tl = need(tool);
    if (kind == kCut && perturbation() == PERTURB_CUT_AS_COMMON) kind = kCommon;

    TopTools_ListOfShape args;
    args.Append(prev.shape);
    TopTools_ListOfShape tools;
    tools.Append(tl.shape);
    std::unique_ptr<BRepAlgoAPI_BooleanOperation> op = makeOperator(kind);
    op->SetArguments(args);
    op->SetTools(tools);
    op->SetNonDestructive(true);
    op->SetFuzzyValue(fuzzy);
    op->SetRunParallel(true);
    op->SetUseOBB(true);
    op->SetCheckInverted(false);
    if (glue) op->SetGlue(BOPAlgo_GlueShift);

    op->Build(Message_ProgressRange());
    if (!op->IsDone()) {
        if (countOf(prev.shape, TopAbs_SOLID) == 0 || countOf(tl.shape, TopAbs_SOLID) == 0) {
            throw NotSolidError(std::string("a ") + nameOf(kind) + " operand is not a solid");
        }
        throw OpError(std::string("OCCT ") + nameOf(kind) + " failed: BRepAlgoAPI_" +
                      classOf(kind) + " did not converge");
    }
    const TopoDS_Shape result = op->Shape();
    if (result.IsNull()) {
        if (kind == kCommon) return 0;
        throw OpError(std::string("OCCT ") + nameOf(kind) + " produced a null shape (empty result)");
    }
    if (kind == kCommon) {
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(result, TopAbs_FACE, faces);
        if (faces.Extent() == 0) return 0;
    }
    TopoDS_Shape named = result;
    if (kind != kCommon && !isValid(result)) {
        const TopoDS_Shape healed = heal(result);
        if (!healed.IsNull()) named = healed;
    }
    return finishHistory(named, *op, prev, &tl, bornIn, unify);
}

}  // namespace kapy_capi

KAPY_API int32_t kapy_boolean(uint32_t ptr, uint32_t length) noexcept {
    const uint8_t kind = firstByte(ptr, length);
    const char* name = kind == kCut ? "cutShape" : kind == kFuse ? "fuseShape" : "intersectShape";
    return runOp(name, seedOf(kind), ptr, length, [](Blob& in) {
        const uint8_t kind = in.u8();
        if (kind > kCommon) throw BlobError();
        const std::string bornIn = in.str();
        const uint32_t previous = in.u32();
        const uint32_t tool = in.u32();
        const double fuzzy = in.f64();
        const bool glue = in.u8() != 0;
        const bool unify = in.u8() != 0;
        return booleanOf(previous, tool, kind, bornIn, fuzzy, glue, unify);
    });
}
