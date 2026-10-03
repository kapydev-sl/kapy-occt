// engine/kernels/occt/build/embind/bindings/capiTrim.cpp
//
// Cut a solid at a plane through the C API: `runTrimByPlane` of the binding.
// The discarded side is a box big enough to swallow the shape, turned so one
// face lies on the plane and moved onto it, then cut away by the same boolean
// every cut runs. The box and its two moves are intermediates: each is named
// (the boolean reads its naming table) and stored, but kept from the host and
// released before the call ends, in the order the binding releases them.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/):
//   trim_by_plane   bornIn, u32 handle, f64[3] origin, f64[3] normal (already
//                   unit length), u8 turn (0 none, 1 about the axis below,
//                   2 half a turn about X), f64 axisX, f64 axisY, f64 angle
// The Rust side does the JavaScript arithmetic that has no C++ twin (the
// normalisation, the arccosine); what arrives is what the box is turned by.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the boolean itself (capiBoolean.cpp).

#include <algorithm>
#include <cmath>
#include <string>

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Bnd_Box.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "capiBoolean.hxx"
#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('trimShapeByPlane').
constexpr size_t SEED_TRIM = 142083949u;

// How much bigger than the shape the discard box is made.
constexpr double BOX_SCALE = 4;
// The defaults of a boolean (runBoolean's fuzzy band, no glue, no unify).
constexpr double FUZZY_DEFAULT = 1e-3;

enum Turn : uint8_t { kNoTurn = 0, kAboutAxis = 1, kHalfTurn = 2 };

// One intermediate move: `trsf` applied to the stored shape `handle` (copying
// it), named from it and stored without telling the host; `handle` is released.
uint32_t moved(uint32_t handle, const gp_Trsf& trsf, const std::string& bornIn) {
    Entry& entry = need(handle);
    BRepBuilderAPI_Transform mover(entry.shape, trsf, true);
    const TopoDS_Shape shape = mover.Shape();
    const uint32_t next = finishHistory(shape, mover, entry, nullptr, bornIn, false, false);
    dropNamed(handle);
    return next;
}

// The box of side `size` with one face on the plane and the rest on the
// normal's side.
uint32_t halfSpaceBox(double size, const double* origin, const double* n, double axisX,
                      double axisY, double angle, uint8_t turn, const std::string& bornIn) {
    const double h = size / 2;
    BRepPrimAPI_MakeBox maker(gp_Pnt(-h, -h, -h), gp_Pnt(h, h, h));
    const TopoDS_Shape box = maker.Shape();
    uint32_t shape = finishBox(box, bornIn, h, h, h, false);
    if (turn != kNoTurn) {
        gp_Trsf rotation;
        if (turn == kAboutAxis) {
            rotation.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(axisX, axisY, 0)), angle);
        } else {
            rotation.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0)), M_PI);
        }
        shape = moved(shape, rotation, bornIn);
    }
    gp_Trsf shift;
    shift.SetTranslation(
        gp_Vec(origin[0] + n[0] * (size / 2), origin[1] + n[1] * (size / 2),
               origin[2] + n[2] * (size / 2)));
    return moved(shape, shift, bornIn);
}

}  // namespace

KAPY_API int32_t kapy_trim_by_plane(uint32_t ptr, uint32_t length) noexcept {
    return runOp("trimShapeByPlane", SEED_TRIM, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const uint32_t handle = in.u32();
        double origin[3], n[3];
        for (double& v : origin) v = in.f64();
        for (double& v : n) v = in.f64();
        const uint8_t turn = in.u8();
        if (turn > kHalfTurn) throw BlobError();
        const double axisX = in.f64(), axisY = in.f64(), angle = in.f64();

        Bnd_Box bounds;
        BRepBndLib::Add(need(handle).shape, bounds, false);
        if (bounds.IsVoid()) return uint32_t(0);
        const gp_Pnt lo = bounds.CornerMin();
        const gp_Pnt hi = bounds.CornerMax();
        const double mins[3] = {lo.X(), lo.Y(), lo.Z()};
        const double maxs[3] = {hi.X(), hi.Y(), hi.Z()};
        double reach = 1;
        for (int i = 0; i < 3; ++i) {
            reach = std::max(reach, std::fabs(mins[i] - origin[i]));
            reach = std::max(reach, std::fabs(maxs[i] - origin[i]));
        }
        const double size = reach * BOX_SCALE;
        const uint32_t box =
            halfSpaceBox(size, origin, n, axisX, axisY, angle, turn, bornIn + "__trim");
        try {
            const uint32_t result =
                booleanOf(handle, box, kCut, bornIn, FUZZY_DEFAULT, false, false);
            dropNamed(box);
            return result;
        } catch (...) {
            dropNamed(box);
            throw;
        }
    });
}
