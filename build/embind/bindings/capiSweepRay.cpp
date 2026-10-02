// services/occt/build/embind/bindings/capiSweepRay.cpp
//
// The swept volume of a solid along a ray through the C API: `runSweepRay` of
// the binding (see capiSweepRay.hxx for the construction). The core is shared
// with the clearance cut; this file is the core and the operation that names
// its result through the fuse's history.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/encode_k10.rs):
//   sweep_ray   bornIn, u32 previous, f64[3] unit direction, f64 length
//
// Anything the binding raises (no triangulation, no prism, a fuse that does
// not converge, a length that is not positive) is a failure (`KAPY_E_FAILED`), worded
// for the user.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the face reading and prisms
// (capiSweepRay.faces.cpp), the cylinder split (capiSweepRay.split.cpp), the
// clearance grow (capiClearanceCut.cpp).

#include "capiSweepRay.hxx"

#include <string>

#include <BRepAlgoAPI_Fuse.hxx>
#include <Message_ProgressRange.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('sweepRayShape').
constexpr size_t SEED_SWEEP_RAY = 1684425623u;

// Red control (perturbation 10): every sweep reaches a tenth as far, so a
// channel stops short of where it should exit.
constexpr int PERTURB_SHORT_SWEEP = 10;
constexpr double SHORT_SWEEP_FACTOR = 0.1;

// The prisms (or wedges) of every face of `shape`; throws when a face cannot
// be read or none yields a prism.
std::vector<TopoDS_Shape> prismsOf(const TopoDS_Shape& shape, const double dir[3],
                                   const gp_Vec& vec) {
    std::vector<TopoDS_Shape> prisms;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE, TopAbs_SHAPE); explorer.More();
         explorer.Next()) {
        const TopoDS_Shape face = explorer.Current();
        std::vector<SweepTri> tris;
        if (!sweepFaceTriangles(face, false, tris)) {
            throw OpError("sweepRay: face has no triangulation");
        }
        const FaceClass cls = sweepClassify(tris, dir);
        if (cls == FaceClass::kParallel) continue;
        if (cls == FaceClass::kMonotone) {
            const TopoDS_Shape prism = sweepFacePrism(face, vec);
            if (!prism.IsNull()) {
                prisms.push_back(prism);
                continue;
            }
            // MakePrism rejected the face: wedges.
        } else {
            std::vector<TopoDS_Shape> pieces;
            if (sweepSplitCylinder(face, dir, tris, pieces)) {
                bool ok = true;
                std::vector<TopoDS_Shape> piecePrisms;
                for (const TopoDS_Shape& piece : pieces) {
                    const TopoDS_Shape prism = sweepFacePrism(piece, vec);
                    if (prism.IsNull()) {
                        ok = false;
                        break;
                    }
                    piecePrisms.push_back(prism);
                }
                if (ok) {
                    prisms.insert(prisms.end(), piecePrisms.begin(), piecePrisms.end());
                    continue;
                }
                // A half was rejected: wedges.
            }
        }
        // Wedges are the only consumer that needs the fine mesh.
        std::vector<SweepTri> fine;
        const std::vector<SweepTri>& wedgeTris = sweepFaceTriangles(face, true, fine) ? fine : tris;
        for (const SweepTri& t : wedgeTris) {
            const TopoDS_Shape wedge = sweepTriangleWedge(t, dir, vec);
            if (!wedge.IsNull()) prisms.push_back(wedge);
        }
    }
    return prisms;
}

}  // namespace

namespace kapy_capi {

SweepResult sweepRayCore(const TopoDS_Shape& shape, const double dir[3], double length) {
    if (!(length > 0)) throw OpError("sweepRay: length must be positive");
    const double reach = perturbation() == PERTURB_SHORT_SWEEP ? length * SHORT_SWEEP_FACTOR : length;
    const gp_Vec vec(dir[0] * reach, dir[1] * reach, dir[2] * reach);
    const std::vector<TopoDS_Shape> prisms = prismsOf(shape, dir, vec);
    if (prisms.empty()) throw OpError("sweepRay: no face produced a prism");

    // Multi-argument fuse: the part as the argument, every prism a tool.
    TopTools_ListOfShape args;
    TopTools_ListOfShape tools;
    args.Append(shape);
    for (const TopoDS_Shape& p : prisms) tools.Append(p);
    SweepResult out;
    out.fuse.reset(new BRepAlgoAPI_Fuse());
    const Message_ProgressRange range;
    out.fuse->SetArguments(args);
    out.fuse->SetTools(tools);
    // `shape` is the caller's live part and the prisms are reused; neither may
    // be edited in place.
    out.fuse->SetNonDestructive(true);
    out.fuse->SetRunParallel(true);
    out.fuse->SetUseOBB(true);
    out.fuse->SetCheckInverted(false);
    out.fuse->Build(range);
    if (!out.fuse->IsDone()) throw OpError("sweepRay: fuse did not converge");
    out.result = out.fuse->Shape();
    if (out.result.IsNull()) throw OpError("sweepRay: fuse returned null");
    return out;
}

}  // namespace kapy_capi

KAPY_API int32_t kapy_sweep_ray(uint32_t ptr, uint32_t length) noexcept {
    return runOp("sweepRayShape", SEED_SWEEP_RAY, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        Entry& previous = need(in.u32());
        double dir[3];
        for (double& v : dir) v = in.f64();
        const double reach = in.f64();
        SweepResult sweep = sweepRayCore(previous.shape, dir, reach);
        return finishHistory(sweep.result, *sweep.fuse, previous, nullptr, bornIn, false);
    });
}
