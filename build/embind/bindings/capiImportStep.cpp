// services/occt/build/embind/bindings/capiImportStep.cpp
//
// STEP import through the C API: `runImportStep` of importShape.ts call for
// call. The file's bytes are written to a file of emscripten's FS (OCCT's reader
// wants a path), parsed into one shape (a compound when there are several roots)
// the first time a content hash is seen, and every call hands out a fresh
// `BRepBuilderAPI_Copy` of the cached base, named as an imported shape and
// stored the way a solid is (same-domain seams collapsed when that helps).
//
// `kapy_import_step` blob: str bornIn, str cacheKey, u32 length, the bytes.
// Answers the new handle. A file the reader rejects, one with no roots or no
// geometry, is a decline: the host redoes the import over embind, which raises
// the user-facing message. The import is not memoised (the binding's is not).
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the cache (capiImportCache.cpp), placement.

#include <cstdio>
#include <fstream>
#include <string>

#include <BRepBuilderAPI_Copy.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Message_ProgressRange.hxx>
#include <STEPControl_Reader.hxx>

#include "capiFinish.hxx"
#include "capiImportCache.hxx"
#include "capiOp.hxx"
#include "factsInternal.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('importStep').
constexpr size_t SEED_IMPORT_STEP = 2005051027u;
const char* const TMP_STEP = "/import.step";

// Parse `bytes` into one shape; a decline when the reader refuses.
TopoDS_Shape parseStep(const uint8_t* bytes, uint32_t length) {
    {
        std::ofstream os(TMP_STEP, std::ios::out | std::ios::binary | std::ios::trunc);
        os.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(length));
        os.flush();
        if (!os.good()) throw DeclinedError("importStep: no file");
    }
    TopoDS_Shape shape;
    try {
        STEPControl_Reader reader;
        if (reader.ReadFile(TMP_STEP) != IFSelect_RetDone) throw DeclinedError("importStep: read");
        if (reader.NbRootsForTransfer() < 1) throw DeclinedError("importStep: no roots");
        reader.TransferRoots(Message_ProgressRange());
        if (reader.NbShapes() < 1) throw DeclinedError("importStep: no shapes");
        shape = reader.OneShape();
        if (shape.IsNull()) throw DeclinedError("importStep: null shape");
    } catch (...) {
        std::remove(TMP_STEP);
        throw;
    }
    std::remove(TMP_STEP);
    return shape;
}
}  // namespace

KAPY_API int32_t kapy_import_step(uint32_t ptr, uint32_t length) noexcept {
    return runOp(
        "importStep", SEED_IMPORT_STEP, ptr, length,
        [](Blob& in) -> uint32_t {
            const std::string bornIn = in.str();
            const std::string cacheKey = in.str();
            const uint32_t size = in.u32();
            const uint8_t* bytes = in.take(size);
            return declining("importStep", [&]() -> uint32_t {
                auto& cache = stepCache();
                auto found = cache.find(cacheKey);
                if (found == cache.end()) {
                    found = cache.emplace(cacheKey, parseStep(bytes, size)).first;
                }
                BRepBuilderAPI_Copy copier(found->second, true, false);
                const TopoDS_Shape shape = copier.Shape();
                const Mapped maps(shape);
                const uint32_t id = allocTable();
                kapy_facts::buildImported(id, maps.view(), bornIn);
                return finishNamed(shape, maps, id, bornIn, true);
            });
        },
        false);
}
