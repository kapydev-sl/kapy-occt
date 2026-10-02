// services/occt/build/embind/bindings/capiImportMesh.cpp
//
// Mesh import through the C API: `runImportMesh` and `buildMeshShape` of
// importMesh.ts call for call, from the regions Rust grew
// (`mesh/region_grow.rs`). A face per region, sewn into a shell, made a solid
// when the shell is watertight (and turned outward by its signed volume), the
// coplanar faces merged when the import asked to detect planes, and the result
// kept under its content hash so an unchanged mesh is built once per session.
// Every call hands out a fresh `BRepBuilderAPI_Copy` of the cached base, named
// as an imported shape and stored WITHOUT collapsing seams (the mesh import's
// store never did).
//
// `kapy_import_mesh` blob: str bornIn, str cacheKey, f64 sewingTol, f64
// angleTol, f64 distTol, u8 detectPlanar, u32 input triangle count, u8 has
// regions; when it has: u32 float count and the f32 positions, u32 region
// count and per region f64[3] normal, f64[3] origin, u32 outline count and its
// indices, u32 hole count and per hole u32 count and its indices.
//
// A blob that carries no regions is the question "do you hold this key?": the
// kernel answers nothing when it does not, and Rust grows the regions (the
// expensive part) and asks again with them. A hit answers u32 handle, u32 input
// triangles, u32 regions, u32 output faces and u8 solid. A mesh that builds no
// face or sews to nothing fails in the words the recipe maps to its error.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the region faces (capiImportRegion.cpp), growing
// regions, the triangle budget (the recipe's).

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <BRepBuilderAPI_Copy.hxx>
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

#include "capiAsk.hxx"
#include "capiFinish.hxx"
#include "capiImportCache.hxx"
#include "factsInternal.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('importMesh').
constexpr size_t SEED_IMPORT_MESH = 1563114636u;
constexpr double PI = 3.14159265358979323846;

struct Params {
    double sewingTol, angleTol, distTol;
    bool detectPlanar;
};

std::vector<uint32_t> readIndices(Blob& in) {
    const uint32_t n = in.u32();
    std::vector<uint32_t> out(n);
    if (n > 0) std::memcpy(out.data(), in.take(static_cast<size_t>(n) * 4), static_cast<size_t>(n) * 4);
    return out;
}

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

int countFaces(const TopoDS_Shape& shape) {
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(shape, TopAbs_FACE, map);
    return map.Extent();
}

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
    built.outputFaces = static_cast<uint32_t>(countFaces(shape));
    built.isSolid = isSolid;
    return built;
}

int32_t handOut(const BuiltMesh& built, const std::string& bornIn) {
    BRepBuilderAPI_Copy copier(built.shape, true, false);
    const TopoDS_Shape shape = copier.Shape();
    const Mapped maps(shape);
    const uint32_t id = allocTable();
    kapy_facts::buildImported(id, maps.view(), bornIn);
    Out out;
    out.u32(finishNamed(shape, maps, id, bornIn, false));
    out.u32(built.inputTris);
    out.u32(built.regions);
    out.u32(built.outputFaces);
    out.u8(built.isSolid ? 1 : 0);
    return out.send();
}
}  // namespace

KAPY_API int32_t kapy_import_mesh(uint32_t ptr, uint32_t length) noexcept {
    return runRaw("importMesh", SEED_IMPORT_MESH, ptr, length, [](Blob& in) -> int32_t {
        const std::string bornIn = in.str();
        const std::string cacheKey = in.str();
        Params p{};
        p.sewingTol = in.f64();
        p.angleTol = in.f64();
        p.distTol = in.f64();
        p.detectPlanar = in.u8() != 0;
        const uint32_t inputTris = in.u32();
        const bool hasRegions = in.u8() != 0;
        std::vector<float> positions;
        std::vector<MeshRegion> regions;
        if (hasRegions) regions = readRegions(in, positions);
        auto& cache = meshCache();
        auto found = cache.find(cacheKey);
        if (found == cache.end()) {
            if (!hasRegions) return answerNothing();
            found = cache.emplace(cacheKey, buildMeshShape(positions, regions, p, inputTris))
                        .first;
        }
        return handOut(found->second, bornIn);
    });
}
