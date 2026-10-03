// engine/kernels/occt/build/embind/bindings/capiHelix.cpp
//
// The helical sweeps of the C API: a profile carried along a helix on a
// cylinder around an axis, with a pipe shell whose binormal is locked to that
// axis. `helicalSweepProfile` (the helix feature) and `threadSweepShape` (the
// thread's V-groove tool) are the same construction: they differ in one pipe
// shell knob (the thread allows C0 joins) and in the words of their errors.
//
// The helix is anchored on the profile face's centroid, and the centroid comes
// out of the kernel, so the arithmetic that follows it (the projection onto
// the axis, the radius, the radial direction) cannot be settled by the core
// beforehand. It is done here in the order the TypeScript does it, in plain
// double arithmetic (this file is compiled without fused multiply-add), with
// `Math.hypot` as V8 computes it. What the core settles is everything that
// does not depend on the centroid: the orientation of the profile, the unit
// axis, the sign, the slope, the length of the helix line and the number of
// edges it is cut into.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/):
//   bornIn, roleSuffix, plane, axisOrigin[3], axisDirection[3] (as given),
//   axisUnit[3], f64 sign, f64 slope, f64 lineLength, u32 edges,
//   segments (already oriented)
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: straight sweeps (capiSweep.cpp).

#pragma STDC FP_CONTRACT OFF

#include <cmath>
#include <string>
#include <vector>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <GProp_GProps.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Message_ProgressRange.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Lin2d.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "factsInternal.hxx"

using namespace kapy_capi;

namespace {

// serialSeedOf('helicalSweepProfileShape'), ('threadSweepProfileShape').
constexpr size_t SEED_HELICAL = 2097190867u;
constexpr size_t SEED_THREAD = 1304203714u;

// The radius under which a centroid counts as lying on the axis.
constexpr double RADIUS_TOL = 1e-6;

// What the head of the blob holds after the plane.
struct HelixHead {
    double origin[3];
    double direction[3];
    double unit[3];
    double sign;
    double slope;
    double lineLength;
    uint32_t edges;
};

HelixHead readHead(Blob& in) {
    HelixHead h;
    for (double& v : h.origin) v = in.f64();
    for (double& v : h.direction) v = in.f64();
    for (double& v : h.unit) v = in.f64();
    h.sign = in.f64();
    h.slope = in.f64();
    h.lineLength = in.f64();
    h.edges = in.u32();
    return h;
}

// `Math.hypot(x, y, z)` as V8 computes it: scale by the largest magnitude,
// Kahan-sum the squares, take the root and scale back (the same algorithm as
// the core's `jsmath::hypot`).
double hypot3(double x, double y, double z) {
    const double values[3] = {x, y, z};
    double largest = 0.0;
    bool nan = false;
    for (double v : values) {
        if (std::isnan(v)) {
            nan = true;
        } else if (std::fabs(v) > largest) {
            largest = std::fabs(v);
        }
    }
    if (largest == INFINITY) return INFINITY;
    if (nan) return NAN;
    if (largest == 0.0) return 0.0;
    double sum = 0.0, compensation = 0.0;
    for (double v : values) {
        const double n = std::fabs(v) / largest;
        const double summand = n * n - compensation;
        const double preliminary = sum + summand;
        compensation = (preliminary - sum) - summand;
        sum = preliminary;
    }
    return std::sqrt(sum) * largest;
}

// The helix wire through `centroid`: a straight line in the (angle, height)
// space of a cylinder whose first point is the centroid, cut into short edges
// and lifted onto the cylinder (`buildHelixWire.ts`). The order the kernel's
// objects are created in is the binding's.
TopoDS_Wire helixWire(const HelixHead& h, const gp_Pnt& centroid) {
    const double dx = centroid.X() - h.origin[0];
    const double dy = centroid.Y() - h.origin[1];
    const double dz = centroid.Z() - h.origin[2];
    const double t = dx * h.unit[0] + dy * h.unit[1] + dz * h.unit[2];
    const double ox = h.origin[0] + t * h.unit[0];
    const double oy = h.origin[1] + t * h.unit[1];
    const double oz = h.origin[2] + t * h.unit[2];
    const double rx = centroid.X() - ox;
    const double ry = centroid.Y() - oy;
    const double rz = centroid.Z() - oz;
    const double radius = hypot3(rx, ry, rz);
    if (radius < RADIUS_TOL) {
        throw OpError(
            "buildHelixWire: profile centroid lies on the helix axis (radius is zero)");
    }
    const gp_Ax3 ax3(gp_Pnt(ox, oy, oz), gp_Dir(h.unit[0], h.unit[1], h.unit[2]),
                     gp_Dir(rx / radius, ry / radius, rz / radius));
    const occ::handle<Geom_Surface> surface = new Geom_CylindricalSurface(ax3, radius);
    const gp_Lin2d line(gp_Pnt2d(0, 0), gp_Dir2d(h.sign * 1, h.slope));
    const occ::handle<Geom2d_Curve> curve = new Geom2d_Line(line);

    BRepBuilderAPI_MakeWire wire;
    for (uint32_t i = 0; i < h.edges; ++i) {
        const double a = (h.lineLength * i) / h.edges;
        const double b = (h.lineLength * (i + 1)) / h.edges;
        const occ::handle<Geom2d_Curve> trimmed = new Geom2d_TrimmedCurve(curve, a, b, true, true);
        BRepBuilderAPI_MakeEdge maker(trimmed, surface);
        const TopoDS_Edge edge = maker.Edge();
        BRepLib::BuildCurves3d(edge);
        wire.Add(edge);
    }
    return wire.Wire();
}

// The sweep both entry points share. `label` words the errors ("helicalSweepProfile"
// or "threadSweepShape"), `nullMessage` the one for a null shape, `c0` is the
// thread's knob, `prefix` the indexed role prefix.
uint32_t sweepHelix(Blob& in, const char* label, const char* nullMessage, bool c0,
                    const char* prefix) {
    const std::string bornIn = in.str();
    const std::string suffix = in.str();
    const Plane plane = readPlane(in);
    const HelixHead head = readHead(in);
    const std::vector<Segment> segments = readSegments(in);
    if (segments.empty()) throw OpError(std::string(label) + " needs at least one profile segment");

    const std::vector<TopoDS_Edge> edges = buildLoopEdges(segments, plane, plane.origin);
    const Explored profile = exploredWire(edges);
    BRepBuilderAPI_MakeFace faceMaker(profile.wire, true);
    const TopoDS_Face face = faceMaker.Face();

    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props, false, false);
    const TopoDS_Wire helix = helixWire(head, props.CentreOfMass());

    BRepOffsetAPI_MakePipeShell shell(helix);
    shell.SetMode(gp_Dir(head.direction[0], head.direction[1], head.direction[2]));
    if (c0) shell.SetForceApproxC1(false);
    shell.Add(profile.wire, false, false);
    if (!shell.IsReady()) {
        throw OpError(std::string(label) + ": pipe shell rejected the inputs");
    }
    const Message_ProgressRange range;
    shell.Build(range);
    if (!shell.MakeSolid()) {
        throw OpError(std::string(label) + ": failed to close the swept shell");
    }
    const TopoDS_Shape shape = shell.Shape();
    if (shape.IsNull()) throw OpError(nullMessage);

    Finish how;
    how.bornIn = bornIn;
    how.roleSuffix = suffix;
    how.rolesJson = kapy_facts::indexedRoles(prefix, shape);
    return finishSolid(shape, how);
}

}  // namespace

KAPY_API int32_t kapy_helical_sweep(uint32_t ptr, uint32_t length) noexcept {
    return runOp("helicalSweepProfileShape", SEED_HELICAL, ptr, length, [](Blob& in) {
        return sweepHelix(in, "helicalSweepProfile", "Helical sweep produced a null shape", false,
                          "helix");
    });
}

KAPY_API int32_t kapy_thread_sweep(uint32_t ptr, uint32_t length) noexcept {
    return runOp("threadSweepProfileShape", SEED_THREAD, ptr, length, [](Blob& in) {
        return sweepHelix(in, "threadSweepShape", "Thread sweep produced a null shape", true,
                          "thread");
    });
}
