// engine/kernels/occt/build/embind/bindings/capiOffsetFaces.cpp
//
// Offset of only the picked faces of a solid through the C API:
// `runOffsetFaces` (+ its slab fallback) of the binding. The plan arrives as
// data: the joins to try in order (the true offset, with neighbours extended to
// meet the moved face or closed with rounds) and whether the slab fallback is
// allowed (a slab thickened from each face, fused on or cut away). The first
// attempt that gives a usable solid wins; it has its tolerance capped and is
// named by enumeration, every face pairing back to an input face recording it
// as its provenance (`buildOffsetFaces`).
//
// A refusal answers `KAPY_E_FAILED` (`KAPY_E_NO_RESULT` when no attempt built)
// and stores nothing.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/encode_k7.rs):
//   offsetFaces  bornIn, u32 previous, f64 offset, u32 nFaces, nFaces x u32
//                face index, u32 nJoins, nJoins x u8 join (0 arc, 1
//                intersection), u8 slab
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the slab construction (capiOffsetSlab.cpp), the
// offset of a whole solid (capiOffset.cpp).

#include <cmath>
#include <vector>

#include <BRepOffset_MakeOffset.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_JoinType.hxx>
#include <TopoDS.hxx>

#include "capiFinish.hxx"
#include "capiGuards.hxx"
#include "capiOffsetSlab.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('offsetFacesShape').
constexpr size_t SEED_OFFSET_FACES = 1487310002u;

// Offset tolerance of the true offset (mm).
constexpr double OFFSET_TOL = 1e-3;

// The true offset: the algorithm starts at a global offset of 0 (the faces not
// picked stay put), each picked face is asked for the signed offset, and the
// result must be a usable solid. Null on any failure, including a result that
// is invalid and cannot be repaired.
TopoDS_Shape buildOffset(const TopoDS_Shape& shape, const std::vector<TopoDS_Face>& faces,
                         double offset, bool intersection) {
    try {
        BRepOffset_MakeOffset op;
        op.Initialize(shape, 0, OFFSET_TOL, BRepOffset_Skin, intersection, false,
                      intersection ? GeomAbs_Intersection : GeomAbs_Arc, false, false);
        for (const TopoDS_Face& face : faces) op.SetOffsetOnFace(face, offset);
        op.MakeOffsetShape();
        if (!op.IsDone()) return TopoDS_Shape();
        const TopoDS_Shape r = op.Shape();
        if (r.IsNull() || hasDegenerateFace(r)) return TopoDS_Shape();
        const TopoDS_Shape fixed = validOrHealed(r);
        if (fixed.IsNull()) return TopoDS_Shape();
        // A compound input comes back as a bare shell: sew it into its solid.
        const TopoDS_Shape solid = ensureSolid(fixed);
        if (solid.IsNull()) return TopoDS_Shape();
        limitTolerance(solid, TOLERANCE_CAP);
        return solid;
    } catch (...) {
        return TopoDS_Shape();
    }
}

uint32_t offsetFacesOf(Blob& in) {
    const std::string bornIn = in.str();
    const uint32_t previous = in.u32();
    const double offset = in.f64();
    const uint32_t nFaces = in.u32();
    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < nFaces; ++i) indices.push_back(in.u32());
    const uint32_t nJoins = in.u32();
    std::vector<bool> intersections;
    for (uint32_t i = 0; i < nJoins; ++i) intersections.push_back(in.u8() != 0);
    const bool slab = in.u8() != 0;
    if (!in.done()) throw BlobError();

    Entry& prev = need(previous);
    ensureMaps(prev);
    if (!(std::fabs(offset) > 0)) throw OpError("offset: distance must be non-zero");
    if (indices.empty()) throw OpError("offset faces: no faces selected");
    for (const uint32_t idx : indices) {
        if (static_cast<int>(idx) >= prev.faces.Extent()) {
            throw OpError("offset faces: index " + std::to_string(idx) + " out of range [0, " +
                          std::to_string(prev.faces.Extent() - 1) + "]");
        }
    }
    std::vector<TopoDS_Face> faces;
    for (const uint32_t idx : indices) {
        faces.push_back(TopoDS::Face(prev.faces.FindKey(static_cast<int>(idx) + 1)));
    }
    TopoDS_Shape result;
    for (const bool intersection : intersections) {
        result = buildOffset(prev.shape, faces, offset, intersection);
        if (!result.IsNull()) break;
    }
    if (result.IsNull() && slab) result = offsetFacesBySlabs(prev.shape, faces, offset);
    if (result.IsNull()) throw NoResultError("no offset-faces attempt built", 0);

    const Mapped maps(result);
    const uint32_t id = allocTable();
    kapy_facts::buildOffsetFaces(id, prev.tableId, maps.view(), bornIn, offset);
    return finishNamed(result, maps, id, bornIn, /*unify=*/true);
}

}  // namespace

KAPY_API int32_t kapy_offset_faces(uint32_t ptr, uint32_t length) noexcept {
    return runOp("offsetFacesShape", SEED_OFFSET_FACES, ptr, length, offsetFacesOf);
}
