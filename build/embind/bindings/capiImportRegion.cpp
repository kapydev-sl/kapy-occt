// engine/kernels/occt/build/embind/bindings/capiImportRegion.cpp
//
// One planar region of a mesh as an OCCT face: `buildRegionFace` and
// `buildLoopWire` of importMesh.faceBuild.ts call for call. Each outline vertex
// is projected onto the region's plane in doubles (the triangles are only
// nearly planar), the polygon closed into a wire, and the face made on the
// plane with the holes added.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: sewing the faces, the cache, the entry point.

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include "capiImportCache.hxx"
#include "capiMeshCore.hxx"

namespace kapy_capi {

namespace {

// The wire of `loop`, or a null wire when OCCT does not finish it.
TopoDS_Wire loopWire(const std::vector<float>& positions, const std::vector<uint32_t>& loop,
                     const MeshRegion& region) {
    BRepBuilderAPI_MakePolygon poly;
    const double* n = region.normal;
    const double* o = region.origin;
    for (uint32_t vi : loop) {
        const double x = positions.at(static_cast<size_t>(vi) * 3);
        const double y = positions.at(static_cast<size_t>(vi) * 3 + 1);
        const double z = positions.at(static_cast<size_t>(vi) * 3 + 2);
        const double d = (x - o[0]) * n[0] + (y - o[1]) * n[1] + (z - o[2]) * n[2];
        poly.Add(gp_Pnt(x - d * n[0], y - d * n[1], z - d * n[2]));
    }
    poly.Close();
    if (!poly.IsDone()) return TopoDS_Wire();
    return poly.Wire();
}

}  // namespace

TopoDS_Shape buildRegionFace(const std::vector<float>& positions, const MeshRegion& region) {
    if (region.outer.size() < 3) return TopoDS_Shape();
    // A zero normal cannot seed a plane (gp_Dir throws on it): the region
    // contributes nothing.
    if (jsHypot3(region.normal[0], region.normal[1], region.normal[2]) < 1e-9) {
        return TopoDS_Shape();
    }
    const TopoDS_Wire outer = loopWire(positions, region.outer, region);
    if (outer.IsNull()) return TopoDS_Shape();
    const gp_Pnt origin(region.origin[0], region.origin[1], region.origin[2]);
    const gp_Dir dir(region.normal[0], region.normal[1], region.normal[2]);
    const gp_Pln pln(origin, dir);
    BRepBuilderAPI_MakeFace maker(pln, outer, true);
    for (const std::vector<uint32_t>& hole : region.inner) {
        if (hole.size() < 3) continue;
        const TopoDS_Wire w = loopWire(positions, hole, region);
        if (!w.IsNull()) maker.Add(w);
    }
    if (!maker.IsDone()) return TopoDS_Shape();
    return maker.Face();
}

}  // namespace kapy_capi
