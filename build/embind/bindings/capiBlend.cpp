// services/occt/build/embind/bindings/capiBlend.cpp
//
// One fillet or chamfer build through the C API: `attemptLocalOperation` of the
// binding (`runLocalOperation.ts`) with its decisions already taken. One build
// at a fixed value, nothing retried (the retries, the diagnosis of a blend that
// fails every attempt and the words for it are the Rust blend recipe's): the
// edges are mapped through the previous body's naming, the operator is built,
// the result must have no collapsed face, and it is named through the
// operator's history.
//
// Every refusal answers `KAPY_E_DECLINED` and stores nothing, so the host
// redoes the attempt on the JSON path and the binding words the failure (it
// decodes the kernel's own exception) exactly as before.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/encode_k7.rs):
//   blend     u8 kind (0 fillet, 1 chamfer), bornIn, u32 previous, f64 param,
//             u8 fromCopy, u32 n, n x u32 edge index
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the retries (Rust), the probe (it stays on JSON).

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
// declined and every measure taken off the result reads differently.
constexpr int PERTURB_HALF_BLEND = 7;

enum BlendKind : uint8_t { kFillet = 0, kChamfer = 1 };

// What the copier made of `original`: the one shape its history lists, or the
// original itself when the copier tracked nothing.
TopoDS_Shape copiedEdge(BRepBuilderAPI_Copy& copier, const TopoDS_Shape& original) {
    const TopTools_ListOfShape& list = copier.Modified(original);
    return list.Extent() > 0 ? list.First() : original;
}

uint32_t blendOf(Blob& in) {
    const uint8_t kind = in.u8();
    if (kind > kChamfer) throw BlobError();
    const std::string bornIn = in.str();
    const uint32_t previous = in.u32();
    double param = in.f64();
    const bool fromCopy = in.u8() != 0;
    const uint32_t count = in.u32();
    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < count; ++i) indices.push_back(in.u32());
    if (!in.done()) throw BlobError();
    if (perturbation() == PERTURB_HALF_BLEND) param *= 0.5;

    Entry& prev = need(previous);
    return declining("blend: the kernel raised", [&]() -> uint32_t {
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
                throw DeclinedError("blend: edge index out of range");
            }
            const TopoDS_Shape original = prev.edges.FindKey(static_cast<int>(idx) + 1);
            const TopoDS_Edge edge = TopoDS::Edge(copier ? copiedEdge(*copier, original) : original);
            if (fillet) fillet->Add(param, edge);
            else chamfer->Add(param, edge);
        }
        BRepBuilderAPI_MakeShape& op = fillet ? static_cast<BRepBuilderAPI_MakeShape&>(*fillet)
                                              : static_cast<BRepBuilderAPI_MakeShape&>(*chamfer);
        op.Build(buildRange);
        if (!op.IsDone()) throw DeclinedError("blend: the kernel did not complete the build");
        const TopoDS_Shape result = op.Shape();
        if (result.IsNull()) throw DeclinedError("blend: null shape");
        if (hasDegenerateFace(result)) throw DeclinedError("blend: degenerate geometry");
        return finishHistory(result, op, prev, nullptr, bornIn, /*unify=*/false);
    });
}

}  // namespace

KAPY_API int32_t kapy_blend_attempt(uint32_t ptr, uint32_t length) noexcept {
    return runOp("blendAttempt", SEED_BLEND, ptr, length, blendOf);
}
