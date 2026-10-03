// engine/kernels/occt/build/embind/bindings/capiChannel.cpp
//
// The solid of a Clearance Cut channel the host computed in the mesh domain
// (`runSweepRayMesh.rebuild.ts` call for call): the welded channel mesh is
// turned back into a B-Rep through the mesh-import face builder, every closed
// shell becomes an outward-oriented solid, and a guarded UnifySameDomain pass
// cuts the face count down. The solid is named as an imported shape and stored
// without announcing it to the host; Rust cuts it from the minuend and releases
// it.
//
// `kapy_channel_solid` blob: str bornIn, u8 merged, f64 sewingTol, f64 angleTol,
// f64 distTol, f64 unifyTol, then the positions and regions as `readRegions`
// reads them (capiImportBuild.hxx). `merged` is the import-style attempt (the
// planar regions merged and unified); not merged is the pure faceted attempt.
// Answers the handle, or nothing when no shell closes into a solid (the caller
// then tries the other attempt).
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the Minkowski step (the host's), the cut.

#include <cmath>
#include <string>
#include <vector>

#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>

#include "capiFinish.hxx"
#include "capiImportBuild.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('channelSolid').
constexpr size_t SEED_CHANNEL = 1508757069u;
constexpr double PI = 3.14159265358979323846;
// A shell with free edges closes into a pseudo-solid of no volume.
constexpr double MIN_VOLUME = 1e-6;

double signedVolume(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props, true, false, false);
    return props.Mass();
}

// MakeSolid keeps the shell orientation; an inside-out solid makes the cut a
// silent no-op.
TopoDS_Shape orientOutward(const TopoDS_Shape& shape) {
    return signedVolume(shape) < 0 ? shape.Reversed() : shape;
}

// Every shell of `shape` closed into an outward solid: the solid, the compound
// of them, or a null shape when none closes.
TopoDS_Shape solidifyShells(const TopoDS_Shape& shape) {
    std::vector<TopoDS_Shape> solids;
    for (TopExp_Explorer ex(shape, TopAbs_SHELL); ex.More(); ex.Next()) {
        BRepBuilderAPI_MakeSolid maker(TopoDS::Shell(ex.Current()));
        if (!maker.IsDone()) continue;
        const TopoDS_Shape solid = maker.Solid();
        if (solid.IsNull()) continue;
        const TopoDS_Shape oriented = orientOutward(solid);
        if (std::fabs(signedVolume(oriented)) > MIN_VOLUME) solids.push_back(oriented);
    }
    if (solids.empty()) return TopoDS_Shape();
    if (solids.size() == 1) return solids[0];
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& s : solids) builder.Add(compound, s);
    return compound;
}

// Merge the coplanar facets; the faceted solid stays when anything fails.
TopoDS_Shape tryUnify(const TopoDS_Shape& shape, double tol, double angleDeg) {
    try {
        ShapeUpgrade_UnifySameDomain unify(shape, true, true, false);
        unify.SetSafeInputMode(true);
        unify.SetLinearTolerance(tol);
        unify.SetAngularTolerance((angleDeg * PI) / 180);
        unify.Build();
        const TopoDS_Shape out = unify.Shape();
        return out.IsNull() ? TopoDS_Shape() : out;
    } catch (...) {
        return TopoDS_Shape();
    }
}

}  // namespace

KAPY_API int32_t kapy_channel_solid(uint32_t ptr, uint32_t length) noexcept {
    return runOp(
        "channelSolid", SEED_CHANNEL, ptr, length,
        [](Blob& in) -> uint32_t {
            const std::string bornIn = in.str();
            Params p{};
            p.detectPlanar = in.u8() != 0;
            p.sewingTol = in.f64();
            p.angleTol = in.f64();
            p.distTol = in.f64();
            const double unifyTol = in.f64();
            std::vector<float> positions;
            const std::vector<MeshRegion> regions = readRegions(in, positions);
            const BuiltMesh built = buildMeshShape(positions, regions, p, 0);
            TopoDS_Shape shape =
                built.isSolid ? orientOutward(built.shape) : solidifyShells(built.shape);
            if (shape.IsNull()) return 0;
            const TopoDS_Shape unified = tryUnify(shape, unifyTol, p.angleTol);
            if (!unified.IsNull()) shape = unified;
            const Mapped maps(shape);
            const uint32_t id = allocTable();
            kapy_facts::buildImported(id, maps.view(), bornIn);
            return finishNamed(shape, maps, id, bornIn, false, false);
        },
        false);
}
