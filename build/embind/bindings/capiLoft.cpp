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
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the 2D loop arithmetic, straight prisms.

#include <string>
#include <vector>

#include <BRepOffsetAPI_ThruSections.hxx>
#include <Message_ProgressRange.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

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

}  // namespace

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
