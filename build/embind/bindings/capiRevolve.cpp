// engine/kernels/occt/build/embind/bindings/capiRevolve.cpp
//
// The revolves of the C API: a profile (or a face with holes) turned around an
// axis. It is `revolveProfile` / `revolveFaceWithHoles` of the TypeScript
// binding with its decisions already taken: the segments arrive oriented, each
// hole loop carries its reverse flag, the axis direction arrives as the unit
// vector the core divided with its own `Math.hypot`, and whether the turn is a
// full one arrives as a flag, so nothing here depends on the kernel's libm
// agreeing with the core's.
//
// Blobs (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/):
//   revolve_profile            bornIn, roleSuffix, plane, axisOrigin[3],
//                              axisUnit[3], f64 angle, u8 symmetric, u8 isFull,
//                              segments (already oriented)
//   revolve_face_with_holes    the same head, then u32 loops, per loop:
//                              u8 reverse, segments (outer first)
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: straight prisms (capiPrism.cpp), sweeps along a
// path (capiSweep.cpp).

#include <string>
#include <vector>

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "factsInternal.hxx"

using namespace kapy_capi;

namespace {

// serialSeedOf('revolveProfileShape'), ('revolveFaceWithHolesShape').
constexpr size_t SEED_REVOLVE_PROFILE = 2058018707u;
constexpr size_t SEED_REVOLVE_WITH_HOLES = 1698384582u;

// What the head of a revolve blob holds after the plane.
struct RevolveHead {
    double origin[3];
    double unit[3];
    double angle;
    bool symmetric;
    bool isFull;
};

RevolveHead readHead(Blob& in) {
    RevolveHead h;
    for (double& v : h.origin) v = in.f64();
    for (double& v : h.unit) v = in.f64();
    h.angle = in.f64();
    h.symmetric = in.u8() != 0;
    h.isFull = in.u8() != 0;
    return h;
}

// The axis the face turns around. The red control (perturbation 6) moves its
// origin a metre along x, so the body lands somewhere else and every bound
// measured off the result reads differently.
gp_Ax1 axisOf(const RevolveHead& h) {
    constexpr double PERTURB_SHIFT = 1000.0;
    const double shift = perturbation() == 6 ? PERTURB_SHIFT : 0.0;
    return gp_Ax1(gp_Pnt(h.origin[0] + shift, h.origin[1], h.origin[2]),
                  gp_Dir(h.unit[0], h.unit[1], h.unit[2]));
}

// The face the turn starts from: a symmetric turn first rotates it back by half
// the angle, so the body straddles the sketch plane.
TopoDS_Shape startFace(const TopoDS_Shape& face, const gp_Ax1& axis, const RevolveHead& h) {
    if (!h.symmetric) return face;
    gp_Trsf trsf;
    trsf.SetRotation(axis, -h.angle / 2);
    BRepBuilderAPI_Transform xform(face, trsf, false);
    return xform.Shape();
}

}  // namespace

KAPY_API int32_t kapy_revolve_profile(uint32_t ptr, uint32_t length) noexcept {
    return runOp("revolveProfileShape", SEED_REVOLVE_PROFILE, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const std::string suffix = in.str();
        const Plane plane = readPlane(in);
        const RevolveHead head = readHead(in);
        const std::vector<Segment> segments = readSegments(in);
        if (segments.empty()) throw OpError("revolveProfile needs at least one wire segment");

        const std::vector<TopoDS_Edge> edges = buildLoopEdges(segments, plane, plane.origin);
        const Explored wire = exploredWire(edges);
        BRepBuilderAPI_MakeFace faceMaker(wire.wire, true);
        const TopoDS_Shape face = faceMaker.Face();

        const gp_Ax1 axis = axisOf(head);
        const TopoDS_Shape turned = startFace(face, axis, head);
        BRepPrimAPI_MakeRevol revol(turned, axis, head.angle, false);
        const TopoDS_Shape shape = revol.Shape();

        const std::vector<kapy_facts::Loop> loops = {
            {asShapes(wire.edges), asShapes(wire.vertices)}};
        Finish how;
        how.bornIn = bornIn;
        how.roleSuffix = suffix;
        how.rolesJson = kapy_facts::revolveRolesOf(revol, shape, loops, false, head.isFull);
        return finishSolid(shape, how);
    });
}

KAPY_API int32_t kapy_revolve_face_with_holes(uint32_t ptr, uint32_t length) noexcept {
    return runOp("revolveFaceWithHolesShape", SEED_REVOLVE_WITH_HOLES, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const std::string suffix = in.str();
        const Plane plane = readPlane(in);
        const RevolveHead head = readHead(in);
        const uint32_t count = in.u32();
        if (count == 0) throw OpError("revolveFaceWithHoles needs an outer boundary");
        std::vector<HoleLoop> loops;
        for (uint32_t i = 0; i < count; ++i) loops.push_back(buildHoleLoop(in, plane));
        if (loops[0].edges.empty()) throw OpError("revolveFaceWithHoles needs an outer boundary");

        BRepBuilderAPI_MakeFace faceMaker(loops[0].wire, true);
        for (size_t i = 1; i < loops.size(); ++i) faceMaker.Add(loops[i].wire);
        const TopoDS_Shape face = faceMaker.Face();

        const gp_Ax1 axis = axisOf(head);
        const TopoDS_Shape turned = startFace(face, axis, head);
        BRepPrimAPI_MakeRevol revol(turned, axis, head.angle, false);
        const TopoDS_Shape shape = revol.Shape();

        std::vector<kapy_facts::Loop> roleLoops;
        for (const HoleLoop& loop : loops) {
            roleLoops.push_back({asShapes(loop.edges), asShapes(wireVertices(loop.wire))});
        }
        Finish how;
        how.bornIn = bornIn;
        how.roleSuffix = suffix;
        how.rolesJson = kapy_facts::revolveRolesOf(revol, shape, roleLoops, true, head.isFull);
        return finishSolid(shape, how);
    });
}
