// engine/kernels/occt/build/embind/bindings/capiImportBuild.cpp
//
// A mesh as a B-Rep, from the regions Rust grew: a face per region, sewn into a
// shell, a solid when the shell is watertight (turned outward by its signed
// volume), the coplanar faces merged when asked to detect planes. This is
// `buildMeshShape` of importMesh.ts and what the clearance channel rebuilds its
// solid with.
//
// Who includes it: the embind link (see ../CMakeLists.txt); the callers are
// capiImportMesh.cpp and capiChannel.cpp, through capiImportBuild.hxx.
// What does NOT belong here: the region faces (capiImportRegion.cpp), growing
// regions, naming or storing what was built.

#include "capiImportBuild.hxx"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Message_ProgressRange.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Solid.hxx>

#ifdef __EMSCRIPTEN__
#include <emscripten/console.h>
#endif

#include "capiOp.hxx"

namespace kapy_capi {

namespace {
constexpr double PI = 3.14159265358979323846;

std::vector<uint32_t> readIndices(Blob& in) {
    const uint32_t n = in.u32();
    std::vector<uint32_t> out(n);
    if (n > 0) std::memcpy(out.data(), in.take(static_cast<size_t>(n) * 4), static_cast<size_t>(n) * 4);
    return out;
}

}  // namespace

std::vector<MeshRegion> readRegions(Blob& in, std::vector<float>& positions) {
    const uint32_t floats = in.u32();
    positions.resize(floats);
    if (floats > 0) {
        std::memcpy(positions.data(), in.take(static_cast<size_t>(floats) * 4),
                    static_cast<size_t>(floats) * 4);
    }
    const uint32_t count = in.u32();
    std::vector<MeshRegion> regions(count);
    for (MeshRegion& r : regions) {
        for (double& v : r.normal) v = in.f64();
        for (double& v : r.origin) v = in.f64();
        r.outer = readIndices(in);
        const uint32_t holes = in.u32();
        for (uint32_t i = 0; i < holes; i++) r.inner.push_back(readIndices(in));
    }
    return regions;
}

int countFacesOf(const TopoDS_Shape& shape) {
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(shape, TopAbs_FACE, map);
    return map.Extent();
}

namespace {
// The guarded co-domain cleanup (`importMesh.unify.ts`): known to crash on bad
// input, so it runs in safe-input mode and answers a null shape on any failure.
TopoDS_Shape unifyCoplanar(const TopoDS_Shape& shape, const Params& p) {
    try {
        ShapeUpgrade_UnifySameDomain unify(shape, true, true, false);
        unify.SetSafeInputMode(true);
        unify.SetLinearTolerance(std::max(p.sewingTol, 1e-4));
        unify.SetAngularTolerance((std::max(p.angleTol, 0.5) * PI) / 180);
        unify.Build();
        return unify.Shape();
    } catch (...) {
        return TopoDS_Shape();
    }
}

// The solid of a watertight shell, turned outward: STL / OBJ winding is
// arbitrary and an inside-out solid breaks every boolean after it.
bool solidOf(const TopoDS_Shape& sewed, TopoDS_Shape& out) {
    BRepBuilderAPI_MakeSolid maker(TopoDS::Shell(sewed));
    if (!maker.IsDone()) return false;
    TopoDS_Shape solid = maker.Solid();
    if (solid.IsNull()) return false;
    GProp_GProps props;
    BRepGProp::VolumeProperties(solid, props, true, false, false);
    if (props.Mass() < 0) solid = solid.Reversed();
    out = solid;
    return true;
}
}  // namespace

BuiltMesh buildMeshShape(const std::vector<float>& positions, const std::vector<MeshRegion>& regions,
                         const Params& p, uint32_t inputTris) {
    BRepBuilderAPI_Sewing sewing(p.sewingTol, true, true, true, false);
    int faceCount = 0;
    for (const MeshRegion& region : regions) {
        const TopoDS_Shape face = buildRegionFace(positions, region);
        if (face.IsNull()) continue;
        sewing.Add(face);
        faceCount++;
    }
    if (faceCount == 0) throw OpError("Mesh produced no buildable faces");
    sewing.Perform(Message_ProgressRange());
    const TopoDS_Shape sewed = sewing.SewedShape();
    if (sewed.IsNull()) throw OpError("Sewing produced a null shape");

    // Watertight when sewing reports no free edges left and the result is one
    // shell.
    const int freeEdges = sewing.NbFreeEdges();
    const bool watertight = freeEdges == 0 && sewed.ShapeType() == TopAbs_SHELL;
#ifdef __EMSCRIPTEN__
    if (!watertight) {
        const std::string text = "[importMesh] sew not watertight: freeEdges=" +
                                 std::to_string(freeEdges) + " builtFaces=" +
                                 std::to_string(faceCount) + "/" + std::to_string(regions.size());
        emscripten_console_warn(text.c_str());
    }
#endif
    TopoDS_Shape base = sewed;
    bool isSolid = false;
    if (watertight) isSolid = solidOf(sewed, base);
    // Skipped when detect-planar is off: unifying coplanar faces would re-merge
    // exactly the faceting the user asked to keep.
    TopoDS_Shape shape = base;
    if (p.detectPlanar) {
        const TopoDS_Shape unified = unifyCoplanar(base, p);
        if (!unified.IsNull()) shape = unified;
    }
    BuiltMesh built;
    built.shape = shape;
    built.inputTris = inputTris;
    built.regions = static_cast<uint32_t>(regions.size());
    built.outputFaces = static_cast<uint32_t>(countFacesOf(shape));
    built.isSolid = isSolid;
    return built;
}

}  // namespace kapy_capi
