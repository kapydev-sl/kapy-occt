// services/occt/build/embind/bindings/capiLoft.cpp
//
// The lofted extrudes of the C API: a profile carried to a drafted top
// (`draftedProfileShape`), through a stack of offset sections
// (`draftedSectionsShape`), and through many rotated sections (the twisted
// prism, `twistedProfileShape`). All three are one `BRepOffsetAPI_ThruSections`
// over section wires; they differ in how many sections there are, whether the
// loft is ruled, and what the caller computed for each section (its origin and
// its offset or rotated segments, already oriented) — the core does that
// arithmetic, this file only builds and names.
//
// Blob (all three): bornIn, roleSuffix, plane, u32 sections, per section:
//   origin[3], segments (already oriented, so the wire chains)
//
// The general loft (`loftShape`) takes sections that are either a sketch
// profile or a face of a body of the store:
//   loft  bornIn, u8 ruled, u8 closed, u32 sections, per section u8 kind:
//         0 profile: plane, segments (already oriented); 1 face: u32 handle,
//         u32 faceIndex
// A face with holes is declined (`KAPY_E_DECLINED`): the binding raises the
// fault the core maps for it, and the host takes that path.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the 2D loop arithmetic, straight prisms.

#include <string>
#include <vector>

#include <BRepOffsetAPI_ThruSections.hxx>
#include <Message_ProgressRange.hxx>
#include <ShapeAnalysis.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "factsInternal.hxx"

using namespace kapy_capi;

namespace {

// serialSeedOf('draftedProfileShape'), ('draftedSectionsShape'),
// ('twistedProfileShape').
constexpr size_t SEED_DRAFTED_PROFILE = 473685089u;
constexpr size_t SEED_DRAFTED_SECTIONS = 259103947u;
constexpr size_t SEED_TWISTED_PROFILE = 388392039u;
constexpr size_t SEED_LOFT = 834395469u;  // loftShape

// The pre-3D tolerance of the loft (`loftThruSections.ts`).
constexpr double LOFT_PRES_3D = 1e-6;

// The face of `shape` a cap shape is, as `{first,last}` of the role facts: -1
// for a cap the loft did not answer.
std::string capsJson(BRepOffsetAPI_ThruSections& loft, const TopoDS_Shape& shape) {
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    int first = -1, last = -1;
    const TopoDS_Shape a = loft.FirstShape();
    if (!a.IsNull() && faces.FindIndex(a) > 0) first = faces.FindIndex(a) - 1;
    const TopoDS_Shape b = loft.LastShape();
    if (!b.IsNull() && faces.FindIndex(b) > 0) last = faces.FindIndex(b) - 1;
    return "{\"first\":" + std::to_string(first) + ",\"last\":" + std::to_string(last) + "}";
}

// Read the sections, build each one's wire, loft them and finish the solid.
uint32_t loftSections(Blob& in, bool ruled) {
    const std::string bornIn = in.str();
    const std::string suffix = in.str();
    const Plane plane = readPlane(in);
    const uint32_t count = in.u32();
    std::vector<TopoDS_Wire> wires;
    for (uint32_t i = 0; i < count; ++i) {
        double origin[3];
        for (double& v : origin) v = in.f64();
        const std::vector<Segment> segments = readSegments(in);
        if (segments.empty()) throw OpError("buildSectionWire needs at least one wire segment");
        wires.push_back(exploredWire(buildLoopEdges(segments, plane, origin)).wire);
    }
    if (wires.size() < 2) throw OpError("loftThruSections needs at least two section wires");

    BRepOffsetAPI_ThruSections loft(true, ruled, LOFT_PRES_3D);
    for (const TopoDS_Wire& w : wires) loft.AddWire(w);
    const Message_ProgressRange range;
    loft.Build(range);
    if (!loft.IsDone()) throw OpError("ThruSections did not converge");
    const TopoDS_Shape shape = loft.Shape();
    if (shape.IsNull()) throw OpError("ThruSections produced a null shape");

    Finish how;
    how.bornIn = bornIn;
    how.roleSuffix = suffix;
    how.rolesJson = "{\"kind\":\"loftCaps\",\"caps\":" + capsJson(loft, shape) + "}";
    return finishSolid(shape, how);
}

// The outer wire of face `faceIndex` of the body `handle` (the section a face
// is). A face with inner wires is declined.
TopoDS_Wire faceOuterWire(uint32_t handle, uint32_t faceIndex) {
    Entry& entry = need(handle);
    ensureMaps(entry);
    if (faceIndex >= static_cast<uint32_t>(entry.faces.Extent())) {
        throw OpError("loft face index " + std::to_string(faceIndex) + " out of range");
    }
    const TopoDS_Face face = TopoDS::Face(entry.faces.FindKey(static_cast<int>(faceIndex) + 1));
    int wireCount = 0;
    for (TopExp_Explorer ex(face, TopAbs_WIRE); ex.More(); ex.Next()) ++wireCount;
    if (wireCount > 1) {
        throw DeclinedError("face " + std::to_string(faceIndex) + " has " +
                            std::to_string(wireCount - 1) + " hole(s)");
    }
    const TopoDS_Wire outer = ShapeAnalysis::OuterWire(face);
    if (outer.IsNull()) throw OpError("loft face has no usable outer wire");
    return outer;
}

// The general loft: read the sections, wire each, run ThruSections, finish.
uint32_t loftGeneral(Blob& in) {
    const std::string bornIn = in.str();
    const bool ruled = in.u8() != 0;
    const bool closed = in.u8() != 0;
    const uint32_t count = in.u32();
    std::vector<TopoDS_Wire> wires;
    for (uint32_t i = 0; i < count; ++i) {
        if (in.u8() == 0) {
            const Plane plane = readPlane(in);
            const std::vector<Segment> segments = readSegments(in);
            if (segments.empty()) throw OpError("buildSectionWire needs at least one wire segment");
            wires.push_back(exploredWire(buildLoopEdges(segments, plane, plane.origin)).wire);
        } else {
            const uint32_t handle = in.u32();
            const uint32_t faceIndex = in.u32();
            wires.push_back(faceOuterWire(handle, faceIndex));
        }
    }
    if (wires.size() < 2) throw OpError("loftThruSections needs at least two section wires");

    BRepOffsetAPI_ThruSections loft(true, ruled, LOFT_PRES_3D);
    for (const TopoDS_Wire& w : wires) loft.AddWire(w);
    if (closed) loft.AddWire(wires[0]);
    const Message_ProgressRange range;
    loft.Build(range);
    if (!loft.IsDone()) throw OpError("ThruSections did not converge");
    const TopoDS_Shape shape = loft.Shape();
    if (shape.IsNull()) throw OpError("ThruSections produced a null shape");

    Finish how;
    how.bornIn = bornIn;
    how.rolesJson =
        "{\"kind\":\"loftCaps\",\"caps\":" + (closed ? std::string("null") : capsJson(loft, shape)) + "}";
    return finishSolid(shape, how);
}

}  // namespace

KAPY_API int32_t kapy_loft(uint32_t ptr, uint32_t length) noexcept {
    return runOp("loftShape", SEED_LOFT, ptr, length, [](Blob& in) { return loftGeneral(in); });
}

KAPY_API int32_t kapy_drafted_profile(uint32_t ptr, uint32_t length) noexcept {
    return runOp("draftedProfileShape", SEED_DRAFTED_PROFILE, ptr, length,
                 [](Blob& in) { return loftSections(in, true); });
}

KAPY_API int32_t kapy_drafted_sections(uint32_t ptr, uint32_t length) noexcept {
    return runOp("draftedSectionsShape", SEED_DRAFTED_SECTIONS, ptr, length,
                 [](Blob& in) { return loftSections(in, true); });
}

KAPY_API int32_t kapy_twisted_profile(uint32_t ptr, uint32_t length) noexcept {
    return runOp("twistedProfileShape", SEED_TWISTED_PROFILE, ptr, length,
                 [](Blob& in) { return loftSections(in, false); });
}
