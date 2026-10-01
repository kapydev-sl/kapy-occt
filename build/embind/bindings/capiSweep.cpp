// services/occt/build/embind/bindings/capiSweep.cpp
//
// The pipe sweeps of the C API: a profile (or a face with holes) carried along
// a path wire of sketch segments with `BRepOffsetAPI_MakePipe`. It is
// `sweepProfile` / `sweepFaceWithHoles` of the TypeScript binding with its
// decisions already taken (the profile arrives oriented, each hole loop
// carries its reverse flag). A path that is an edge of another body is not
// encoded: the core declines it and the binding answers.
//
// The path wire is built BEFORE the profile, as the binding's glue does, so
// the kernel numbers what it creates in the same order.
//
// Blobs (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/):
//   sweep_profile            bornIn, roleSuffix, profilePlane, pathPlane,
//                            pathSegments, profile segments (already oriented)
//   sweep_face_with_holes    the same head, then u32 loops, per loop:
//                            u8 reverse, segments (outer first)
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: revolves (capiRevolve.cpp), lofts (capiLoft.cpp),
// the helical sweeps (capiHelix.cpp).

#include <string>
#include <vector>

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "factsInternal.hxx"

using namespace kapy_capi;

namespace {

// serialSeedOf('sweepProfileShape'), ('sweepFaceWithHolesShape').
constexpr size_t SEED_SWEEP_PROFILE = 2120292160u;
constexpr size_t SEED_SWEEP_WITH_HOLES = 1961704907u;

// The open path wire of `segments` lifted onto `plane`, its edges chained in
// the order given (not oriented, not explored).
TopoDS_Wire pathWire(const std::vector<Segment>& segments, const Plane& plane) {
    if (segments.empty()) throw OpError("buildPathWire needs at least one segment");
    const std::vector<TopoDS_Edge> edges = buildLoopEdges(segments, plane, plane.origin);
    BRepBuilderAPI_MakeWire maker;
    for (const TopoDS_Edge& e : edges) maker.Add(e);
    return maker.Wire();
}

// Pipe `face` along `path`, name the result. `loopsOf()` answers the profile
// loops' shapes for the role facts, called after the pipe as the binding does.
template <typename LoopsOf>
uint32_t pipeAndFinish(const TopoDS_Wire& path, const TopoDS_Face& face, const std::string& bornIn,
                       const std::string& suffix, LoopsOf&& loopsOf, bool withHoles) {
    BRepOffsetAPI_MakePipe pipe(path, face);
    const TopoDS_Shape shape = pipe.Shape();
    if (shape.IsNull()) {
        throw OpError("Sweep produced a null shape \xE2\x80\x94 check path validity");
    }
    Finish how;
    how.bornIn = bornIn;
    how.roleSuffix = suffix;
    how.rolesJson = kapy_facts::sweepRolesOf(pipe, shape, loopsOf(), withHoles);
    return finishSolid(shape, how);
}

}  // namespace

KAPY_API int32_t kapy_sweep_profile(uint32_t ptr, uint32_t length) noexcept {
    return runOp("sweepProfileShape", SEED_SWEEP_PROFILE, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const std::string suffix = in.str();
        const Plane profilePlane = readPlane(in);
        const Plane pathPlane = readPlane(in);
        const TopoDS_Wire path = pathWire(readSegments(in), pathPlane);
        const std::vector<Segment> segments = readSegments(in);
        if (segments.empty()) throw OpError("sweepProfile needs at least one profile segment");

        const std::vector<TopoDS_Edge> edges =
            buildLoopEdges(segments, profilePlane, profilePlane.origin);
        const Explored wire = exploredWire(edges);
        BRepBuilderAPI_MakeFace faceMaker(wire.wire, true);
        return pipeAndFinish(
            path, faceMaker.Face(), bornIn, suffix,
            [&] {
                return std::vector<kapy_facts::Loop>{
                    {asShapes(wire.edges), asShapes(wire.vertices)}};
            },
            false);
    });
}

KAPY_API int32_t kapy_sweep_face_with_holes(uint32_t ptr, uint32_t length) noexcept {
    return runOp("sweepFaceWithHolesShape", SEED_SWEEP_WITH_HOLES, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const std::string suffix = in.str();
        const Plane profilePlane = readPlane(in);
        const Plane pathPlane = readPlane(in);
        const TopoDS_Wire path = pathWire(readSegments(in), pathPlane);
        const uint32_t count = in.u32();
        if (count == 0) throw OpError("sweepFaceWithHoles needs an outer boundary");
        std::vector<HoleLoop> loops;
        for (uint32_t i = 0; i < count; ++i) loops.push_back(buildHoleLoop(in, profilePlane));
        if (loops[0].edges.empty()) throw OpError("sweepFaceWithHoles needs an outer boundary");

        BRepBuilderAPI_MakeFace faceMaker(loops[0].wire, true);
        for (size_t i = 1; i < loops.size(); ++i) faceMaker.Add(loops[i].wire);
        return pipeAndFinish(
            path, faceMaker.Face(), bornIn, suffix,
            [&] {
                std::vector<kapy_facts::Loop> roleLoops;
                for (const HoleLoop& loop : loops) {
                    roleLoops.push_back(
                        {asShapes(loop.edges), asShapes(wireVertices(loop.wire))});
                }
                return roleLoops;
            },
            true);
    });
}
