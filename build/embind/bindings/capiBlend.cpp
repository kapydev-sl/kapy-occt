// services/occt/build/embind/bindings/capiBlend.cpp
//
// One fillet or chamfer build through the C API: `attemptLocalOperation` of the
// original binding with its decisions already taken. One build at a fixed value,
// nothing retried (the retries, the diagnosis of a blend that fails every
// attempt and the product's words for it are the Rust blend recipe's): the
// edges are mapped through the previous body's naming, the operator is built,
// the result must have no collapsed face, and it is named through the
// operator's history.
//
// A refusal answers `KAPY_E_FAILED` and stores nothing; its message is what
// the recipe reads back as the kernel's words: `<kind>: <what happened>`, where
// what happened is one of this file's own sentences or an OCCT exception as
// `<type>,<message>`; the degenerate-geometry sentences are the product's
// own and travel without the prefix.
//
// `kapy_blend_probe` runs the same build and discards the result: whether a
// value builds, which is all the blend diagnosis asks of its probes.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/encode_k7.rs):
//   blend     u8 kind (0 fillet, 1 chamfer), bornIn, u32 previous, f64 param,
//             u8 fromCopy, u32 n, n x u32 edge index
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the retries and the diagnosis (Rust).

#include <memory>
#include <vector>

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <ChFi3d_FilletShape.hxx>
#include <Message_ProgressRange.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>

#include "capiFinish.hxx"
#include "capiGuards.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('blendAttempt').
constexpr size_t SEED_BLEND = 442522128u;

// Red control (perturbation 7): every blend runs at half its radius or
// distance. Whatever the full value could build the half can, so nothing is
// refused and every measure taken off the result reads differently.
constexpr int PERTURB_HALF_BLEND = 7;

enum BlendKind : uint8_t { kFillet = 0, kChamfer = 1 };

// What the copier made of `original`: the one shape its history lists, or the
// original itself when the copier tracked nothing.
TopoDS_Shape copiedEdge(BRepBuilderAPI_Copy& copier, const TopoDS_Shape& original) {
    const TopTools_ListOfShape& list = copier.Modified(original);
    return list.Extent() > 0 ? list.First() : original;
}

// The product's own sentence for a build whose result holds a collapsed face.
std::string degenerateWords(uint8_t kind) {
    return kind == kFillet
               ? "Fillet produced degenerate geometry \xe2\x80\x94 try smaller radius or fewer edges"
               : "Chamfer produced degenerate geometry \xe2\x80\x94 try smaller distance or fewer edges";
}

// One build at `param` on `indices` of `prev`. Stored and named when `probe`
// is false (the new handle), discarded when it is true (answers 1 for a build
// that worked). Every failure leaves as an `OpError` in the words above.
uint32_t buildBlend(Entry& prev, uint8_t kind, const std::string& bornIn, double param,
                    bool fromCopy, const std::vector<uint32_t>& indices, bool probe) {
    const std::string label = kind == kFillet ? "fillet" : "chamfer";
    try {
        ensureMaps(prev);

        const Message_ProgressRange buildRange;
        std::unique_ptr<BRepBuilderAPI_Copy> copier;
        if (fromCopy) copier.reset(new BRepBuilderAPI_Copy(prev.shape, true, false));
        const TopoDS_Shape target = copier ? copier->Shape() : prev.shape;
        std::unique_ptr<BRepFilletAPI_MakeFillet> fillet;
        std::unique_ptr<BRepFilletAPI_MakeChamfer> chamfer;
        if (kind == kFillet) fillet.reset(new BRepFilletAPI_MakeFillet(target, ChFi3d_Rational));
        else chamfer.reset(new BRepFilletAPI_MakeChamfer(target));

        for (const uint32_t idx : indices) {
            if (static_cast<int>(idx) >= prev.edges.Extent()) {
                throw OpError(label + ": " + label + " edge index " + std::to_string(idx) +
                              " out of range [0, " + std::to_string(prev.edges.Extent() - 1) + "]");
            }
            const TopoDS_Shape original = prev.edges.FindKey(static_cast<int>(idx) + 1);
            const TopoDS_Edge edge = TopoDS::Edge(copier ? copiedEdge(*copier, original) : original);
            if (fillet) fillet->Add(param, edge);
            else chamfer->Add(param, edge);
        }
        BRepBuilderAPI_MakeShape& op = fillet ? static_cast<BRepBuilderAPI_MakeShape&>(*fillet)
                                              : static_cast<BRepBuilderAPI_MakeShape&>(*chamfer);
        op.Build(buildRange);
        if (!op.IsDone()) throw OpError(label + ": " + label + ": the kernel did not complete the build");
        const TopoDS_Shape result = op.Shape();
        if (result.IsNull()) {
            throw OpError(label + ": " + (kind == kFillet ? "Fillet" : "Chamfer") +
                          " produced a null shape");
        }
        if (hasDegenerateFace(result)) throw OpError(degenerateWords(kind));
        if (probe) return 1;
        return finishHistory(result, op, prev, nullptr, bornIn, /*unify=*/false);
    } catch (const OpError&) {
        throw;
    } catch (const Standard_Failure& f) {
        throw OpError(label + ": " + occtWords(f));
    }
}

// The blob of a blend, read once for both entry points.
struct BlendCall {
    uint8_t kind = 0;
    std::string bornIn;
    uint32_t previous = 0;
    double param = 0;
    bool fromCopy = false;
    std::vector<uint32_t> indices;
};

BlendCall readBlend(Blob& in) {
    BlendCall call;
    call.kind = in.u8();
    if (call.kind > kChamfer) throw BlobError();
    call.bornIn = in.str();
    call.previous = in.u32();
    call.param = in.f64();
    call.fromCopy = in.u8() != 0;
    const uint32_t count = in.u32();
    for (uint32_t i = 0; i < count; ++i) call.indices.push_back(in.u32());
    if (!in.done()) throw BlobError();
    if (perturbation() == PERTURB_HALF_BLEND) call.param *= 0.5;
    return call;
}

uint32_t blendOf(Blob& in) {
    const BlendCall call = readBlend(in);
    Entry& prev = need(call.previous);
    return buildBlend(prev, call.kind, call.bornIn, call.param, call.fromCopy, call.indices, false);
}

// One byte: 1 when the build at the value succeeds.
int32_t blendProbeOf(Blob& in) {
    const BlendCall call = readBlend(in);
    Entry& prev = need(call.previous);
    uint8_t ok = 0;
    try {
        ok = buildBlend(prev, call.kind, "probe", call.param, false, call.indices, true) ? 1 : 0;
    } catch (const OpError&) {
        ok = 0;
    }
    return answer(&ok, 1);
}

}  // namespace

KAPY_API int32_t kapy_blend_attempt(uint32_t ptr, uint32_t length) noexcept {
    return runOp("blendAttempt", SEED_BLEND, ptr, length, blendOf);
}

KAPY_API int32_t kapy_blend_probe(uint32_t ptr, uint32_t length) noexcept {
    return runRaw("blendProbe", SEED_BLEND, ptr, length, blendProbeOf);
}
