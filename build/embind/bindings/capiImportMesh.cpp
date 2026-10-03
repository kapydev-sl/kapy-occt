// engine/kernels/occt/build/embind/bindings/capiImportMesh.cpp
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

#include <string>
#include <vector>

#include <BRepBuilderAPI_Copy.hxx>

#include "capiAsk.hxx"
#include "capiFinish.hxx"
#include "capiImportBuild.hxx"
#include "capiImportCache.hxx"
#include "factsInternal.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('importMesh').
constexpr size_t SEED_IMPORT_MESH = 1563114636u;

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
