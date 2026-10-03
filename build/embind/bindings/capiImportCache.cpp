// engine/kernels/occt/build/embind/bindings/capiImportCache.cpp
//
// The two import caches (see capiImportCache.hxx).
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the imports themselves.

#include "capiImportCache.hxx"

namespace kapy_capi {

std::unordered_map<std::string, TopoDS_Shape>& stepCache() {
    static std::unordered_map<std::string, TopoDS_Shape> cache;
    return cache;
}

std::unordered_map<std::string, BuiltMesh>& meshCache() {
    static std::unordered_map<std::string, BuiltMesh> cache;
    return cache;
}

void clearImportCaches() {
    stepCache().clear();
    meshCache().clear();
}

}  // namespace kapy_capi
