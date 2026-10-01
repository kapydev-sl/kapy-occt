// services/occt/build/embind/bindings/capiPrism.cpp
//
// The straight-prism family of the C API: a profile swept along a vector,
// a profile with holes swept along a vector, several shapes gathered in a
// compound, and a box. Each is the TypeScript builder of the same name with
// its decisions already taken (which way each loop runs arrives as a flag and
// the segments arrive oriented), so what is left here is the OCCT call
// sequence, the role facts and the naming, in the order the binding makes them.
//
// Blobs (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/):
//   extrude_profile          bornIn, roleSuffix, plane, direction[3], segments
//   extrude_face_with_holes  bornIn, roleSuffix, plane, direction[3], u32 loops,
//                            per loop: u8 reverse, segments (outer first)
//   compound                 bornIn, u32 count, u32 handles
//   make_box                 bornIn, sizeX, sizeY, sizeZ
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: lofts (capiLoft.cpp), pushed faces (capiPush.cpp).

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Pnt.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "factsInternal.hxx"
#include "factsJson.hxx"

using namespace kapy_capi;

namespace {

// Serial seeds of the binding's methods (`serialSeedOf(name)`: FNV-1a of the
// name), so the kernel numbers what it creates from the same state.
constexpr size_t SEED_EXTRUDE_PROFILE = 845068484u;     // extrudeProfileShape
constexpr size_t SEED_EXTRUDE_WITH_HOLES = 845305103u;  // extrudeFaceWithHolesShape
constexpr size_t SEED_COMPOUND = 940985459u;            // compoundShapes
constexpr size_t SEED_MAKE_BOX = 1522672354u;           // makeBoxShape

void readDirection(Blob& in, double* d) {
    for (int i = 0; i < 3; ++i) d[i] = in.f64();
}

bool isZeroVector(const double* d) { return !(std::hypot(std::hypot(d[0], d[1]), d[2]) > 0); }

// The JSON array of each segment's source id (null for none), the `sources`
// row of one loop.
std::string sourcesOf(const std::vector<Segment>& segments) {
    std::string out = "[";
    for (size_t i = 0; i < segments.size(); ++i) {
        out += i ? "," : "";
        out += segments[i].hasSource ? kapy_facts::jstr(segments[i].source) : "null";
    }
    return out + "]";
}

std::string formatSize(double v) { return std::isnan(v) ? "NaN" : kapy_facts::jnum(v); }

}  // namespace

KAPY_API int32_t kapy_extrude_profile(uint32_t ptr, uint32_t length) noexcept {
    return runOp("extrudeProfileShape", SEED_EXTRUDE_PROFILE, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const std::string suffix = in.str();
        const Plane plane = readPlane(in);
        double dir[3];
        readDirection(in, dir);
        const std::vector<Segment> segments = readSegments(in);
        if (segments.empty()) throw OpError("extrudeProfile needs at least one wire segment");
        if (isZeroVector(dir)) throw OpError("extrudeProfile direction vector must be non-zero");

        const std::vector<TopoDS_Edge> edges = buildLoopEdges(segments, plane, plane.origin);
        const Explored wire = exploredWire(edges);
        BRepBuilderAPI_MakeFace faceMaker(wire.wire, true);
        const TopoDS_Face face = faceMaker.Face();
        const gp_Vec vec = prismVector(dir);
        BRepPrimAPI_MakePrism prism(face, vec, false, true);
        const TopoDS_Shape shape = prism.Shape();

        const std::vector<kapy_facts::Loop> loops = {
            {asShapes(wire.edges), asShapes(wire.vertices)}};
        Finish how;
        how.bornIn = bornIn;
        how.roleSuffix = suffix;
        how.rolesJson = kapy_facts::prismRolesOf(prism, shape, loops, false,
                                                 "[" + sourcesOf(segments) + "]", dir);
        return finishSolid(shape, how);
    });
}

KAPY_API int32_t kapy_extrude_face_with_holes(uint32_t ptr, uint32_t length) noexcept {
    return runOp("extrudeFaceWithHolesShape", SEED_EXTRUDE_WITH_HOLES, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const std::string suffix = in.str();
        const Plane plane = readPlane(in);
        double dir[3];
        readDirection(in, dir);
        const uint32_t count = in.u32();
        if (count == 0) throw OpError("extrudeFaceWithHoles needs an outer boundary");
        if (isZeroVector(dir)) {
            throw OpError("extrudeFaceWithHoles direction vector must be non-zero");
        }
        std::vector<HoleLoop> loops;
        for (uint32_t i = 0; i < count; ++i) loops.push_back(buildHoleLoop(in, plane));
        if (loops[0].edges.empty()) throw OpError("extrudeFaceWithHoles needs an outer boundary");

        BRepBuilderAPI_MakeFace faceMaker(loops[0].wire, true);
        for (size_t i = 1; i < loops.size(); ++i) faceMaker.Add(loops[i].wire);
        const TopoDS_Face face = faceMaker.Face();
        const gp_Vec vec = prismVector(dir);
        BRepPrimAPI_MakePrism prism(face, vec, false, true);
        const TopoDS_Shape shape = prism.Shape();

        std::vector<kapy_facts::Loop> roleLoops;
        std::string sources = "[";
        for (size_t i = 0; i < loops.size(); ++i) {
            roleLoops.push_back({asShapes(loops[i].edges), asShapes(wireVertices(loops[i].wire))});
            sources += (i ? "," : "") + sourcesOf(loops[i].ordered);
        }
        sources += "]";
        Finish how;
        how.bornIn = bornIn;
        how.roleSuffix = suffix;
        how.rolesJson = kapy_facts::prismRolesOf(prism, shape, roleLoops, true, sources, dir);
        return finishSolid(shape, how);
    });
}

KAPY_API int32_t kapy_compound(uint32_t ptr, uint32_t length) noexcept {
    return runOp("compoundShapes", SEED_COMPOUND, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const uint32_t count = in.u32();
        std::vector<TopoDS_Shape> parts;
        for (uint32_t i = 0; i < count; ++i) {
            Entry* entry = find(in.u32());
            if (!entry) throw OpError("OCCT shape handle not found");
            parts.push_back(entry->shape);
        }
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const TopoDS_Shape& part : parts) builder.Add(compound, part);
        Finish how;
        how.bornIn = bornIn;
        how.rolesJson = "{\"kind\":\"none\"}";
        how.unify = false;
        return finishSolid(compound, how);
    });
}

KAPY_API int32_t kapy_make_box(uint32_t ptr, uint32_t length) noexcept {
    return runOp(
        "makeBoxShape", SEED_MAKE_BOX, ptr, length,
        [](Blob& in) {
            const std::string bornIn = in.str();
            const double sx = in.f64(), sy = in.f64(), sz = in.f64();
            if (!(sx > 0) || !(sy > 0) || !(sz > 0)) {
                throw OpError("makeBox: sizes must be positive (got " + formatSize(sx) +
                              "\xC3\x97" + formatSize(sy) + "\xC3\x97" + formatSize(sz) + ")");
            }
            const double hx = sx / 2, hy = sy / 2, hz = sz / 2;
            BRepPrimAPI_MakeBox maker(gp_Pnt(-hx, -hy, -hz), gp_Pnt(hx, hy, hz));
            const TopoDS_Shape shape = maker.Shape();
            return finishBox(shape, bornIn, hx, hy, hz);
        },
        false);
}
