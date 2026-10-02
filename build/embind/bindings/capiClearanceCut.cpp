// services/occt/build/embind/bindings/capiClearanceCut.cpp
//
// Clearance Cut through the C API, the analytic channel of `runClearanceCut`:
// grow the part by the clearance (a cube Minkowski, per world axis a sweep of
// twice the clearance moved back by the clearance and unified), sweep the
// grown part along the extraction direction and cut the channel from the
// minuend. The channel is a handle only inside this call: named through the
// fuse's history of the tool, stored without announcing it to the host, and
// dropped once the cut is done.
//
// Which channel a tool takes (this analytic one, or the mesh Minkowski one for
// a faceted tool) is decided on the Rust side; the mesh channel is built by
// the host's own glue, so only the analytic one reaches this function.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/encode_k10.rs):
//   clearance_cut  bornIn, u32 minuend, u32 tool, f64[3] unit direction,
//                  f64 length, f64 clearance
//
// Anything the binding raises is a decline, so the host asks the JSON route.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the sweep itself (capiSweepRay*.cpp), the cut's
// own naming (capiBoolean.cpp).

#include <string>

#include <BRepBuilderAPI_Transform.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "capiBoolean.hxx"
#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "capiSweepRay.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('clearanceCutShape').
constexpr size_t SEED_CLEARANCE_CUT = 2078206070u;

// The fuzzy value of every boolean the binding runs without one of its own.
constexpr double CUT_FUZZY = 1e-3;

constexpr double AXES[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

// `shape` moved by `delta` (a copy, so the input survives).
TopoDS_Shape translated(const TopoDS_Shape& shape, const double delta[3]) {
    gp_Trsf trsf;
    trsf.SetTranslation(gp_Vec(delta[0], delta[1], delta[2]));
    BRepBuilderAPI_Transform mover(shape, trsf, true);
    return mover.Shape();
}

// Merge the coplanar faces and colinear edges a grow sweep leaves, so the face
// count does not compound across the axes. Null on any failure: the caller
// keeps the shape as it is (correct, with more faces).
TopoDS_Shape tryUnify(const TopoDS_Shape& shape) {
    try {
        ShapeUpgrade_UnifySameDomain u(shape, true, true, false);
        u.Build();
        const TopoDS_Shape s = u.Shape();
        return s.IsNull() ? TopoDS_Shape() : s;
    } catch (...) {
        return TopoDS_Shape();
    }
}

// The tool grown by `clearance` on every side.
TopoDS_Shape grown(const TopoDS_Shape& tool, double clearance) {
    TopoDS_Shape cur = tool;
    if (!(clearance > 0)) return cur;
    for (const double* u : AXES) {
        const SweepResult sw = sweepRayCore(cur, u, clearance * 2);
        const double back[3] = {-u[0] * clearance, -u[1] * clearance, -u[2] * clearance};
        const TopoDS_Shape moved = translated(sw.result, back);
        const TopoDS_Shape unified = tryUnify(moved);
        cur = unified.IsNull() ? moved : unified;
    }
    return cur;
}

}  // namespace

KAPY_API int32_t kapy_clearance_cut(uint32_t ptr, uint32_t length) noexcept {
    return runOp("clearanceCutShape", SEED_CLEARANCE_CUT, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const uint32_t minuend = in.u32();
        Entry& tool = need(in.u32());
        need(minuend);
        double dir[3];
        for (double& v : dir) v = in.f64();
        const double reach = in.f64();
        const double clearance = in.f64();
        return declining("clearanceCut", [&]() -> uint32_t {
            const TopoDS_Shape part = grown(tool.shape, clearance);
            SweepResult sweep = sweepRayCore(part, dir, reach);
            // The channel gets a real naming table (the tool's ancestry) so the
            // cut can carry the names; it lives only inside this call.
            const uint32_t channel =
                finishHistory(sweep.result, *sweep.fuse, tool, nullptr, bornIn, false, false);
            sweep.fuse.reset();
            try {
                const uint32_t out = booleanOf(minuend, channel, kCut, bornIn, CUT_FUZZY, false, true);
                dropNamed(channel);
                if (out == 0) throw OpError("clearanceCut: cut produced an empty result");
                return out;
            } catch (...) {
                dropNamed(channel);
                throw;
            }
        });
    });
}
