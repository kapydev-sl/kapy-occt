// engine/kernels/occt/build/embind/bindings/capiMesh.cpp
//
// A body's triangles as one buffer in the result arena, with no embind array
// crossing the boundary: `kapy_mesh` is the tessellation the viewport, the
// thumbnails and the exports read (binding/tessellate.ts), and
// `kapy_hlr_input` is what the drawings' projection reads of each body
// (binding/hlr.raw.ts): positions, indices and every edge's polyline.
//
// `kapy_mesh` blob: u32 handle, f64 linear deflection, f64 angular deflection,
// u32 flag count, then one byte per face of the body's map (1: planar, to be
// checked against its exact area), then optionally u8 fresh: 1 meshes a copy of
// the shape that carries no triangulation (BRepMesh reuses a finer one a shape
// already holds, so a job whose bytes must not depend on what the screen meshed
// before it, the STL export, asks for it).
// Answers: u32 position count (floats) and the f32 positions; u32 index count
// and the u32 indices; u32 triangle count and one u32 face index per triangle;
// the f32 normals (as many as positions); u32 edge count and per edge u32 float
// count and its f32 polyline; u32 mask length (0, or one per edge) and the seam
// mask bytes.
//
// `kapy_hlr_input` blob: u8 exact, f64 linear, f64 angular, u32 body count, then
// per body u32 handle, u32 flag count and the flags. Answers per body: the
// positions, the indices and the edges, laid out as above.
//
// The test-only perturbation 9 moves every position a millimetre along x.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the meshing itself (capiMeshFace.cpp,
// capiMeshEdges.cpp).

#include <vector>

#include <BRepBuilderAPI_Copy.hxx>

#include "capiAsk.hxx"
#include "capiMeshCore.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('meshShape'), ('projectHLRPreview'), ('projectHLRExact').
constexpr size_t SEED_MESH = 1921537192u;
constexpr size_t SEED_HLR_PREVIEW = 682511686u;
constexpr size_t SEED_HLR_EXACT = 1733839593u;
// Perturbation 9 shifts every position by this much along x.
constexpr float PERTURB_SHIFT_MM = 1.0f;

template <typename T>
void putArray(Out& out, const std::vector<T>& values) {
    out.u32(static_cast<uint32_t>(values.size()));
    if (!values.empty()) out.bytes(values.data(), values.size() * sizeof(T));
}

std::vector<uint8_t> readFlags(Blob& in) {
    const uint32_t n = in.u32();
    std::vector<uint8_t> flags;
    flags.reserve(n);
    for (uint32_t i = 0; i < n; i++) flags.push_back(in.u8());
    return flags;
}

void putEdges(Out& out, const BodyMesh& mesh) {
    out.u32(static_cast<uint32_t>(mesh.edges.size()));
    for (const std::vector<float>& polyline : mesh.edges) putArray(out, polyline);
}

void shiftForTest(std::vector<float>& positions) {
    if (perturbation() != 9) return;
    for (size_t i = 0; i < positions.size(); i += 3) positions[i] += PERTURB_SHIFT_MM;
}
}  // namespace

KAPY_API int32_t kapy_mesh(uint32_t ptr, uint32_t length) noexcept {
    return ask("meshShape", SEED_MESH, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        const double linear = in.f64();
        const double angular = in.f64();
        const std::vector<uint8_t> planar = readFlags(in);
        const bool fresh = in.remaining() > 0 && in.u8() != 0;
        BodyMesh mesh;
        std::vector<uint8_t> seams;
        if (fresh) {
            // The copy keeps the original's face order, so the flags and the
            // face indices read the same.
            Entry copy;
            copy.shape = BRepBuilderAPI_Copy(entry.shape, true, false).Shape();
            meshBody(copy, linear, angular, planar, mesh);
            seams = seamMask(copy);
        } else {
            meshBody(entry, linear, angular, planar, mesh);
            seams = seamMask(entry);
        }
        const std::vector<float> normals = vertexNormals(mesh.positions, mesh.indices);
        shiftForTest(mesh.positions);
        putArray(out, mesh.positions);
        putArray(out, mesh.indices);
        putArray(out, mesh.triFace);
        out.bytes(normals.data(), normals.size() * sizeof(float));
        putEdges(out, mesh);
        putArray(out, seams);
    });
}

KAPY_API int32_t kapy_hlr_input(uint32_t ptr, uint32_t length) noexcept {
    const bool exact = length > 0 && ptr != 0 && firstByte(ptr, length) == 1;
    return ask("projectHLR", exact ? SEED_HLR_EXACT : SEED_HLR_PREVIEW, ptr, length,
               [](Blob& in, Out& out) {
                   in.u8();
                   const double linear = in.f64();
                   const double angular = in.f64();
                   const uint32_t count = in.u32();
                   std::vector<Entry*> bodies;
                   std::vector<std::vector<uint8_t>> flags;
                   for (uint32_t i = 0; i < count; i++) {
                       bodies.push_back(&need(in.u32()));
                       flags.push_back(readFlags(in));
                   }
                   out.u32(count);
                   for (uint32_t i = 0; i < count; i++) {
                       BodyMesh mesh;
                       meshBody(*bodies[i], linear, angular, flags[i], mesh);
                       shiftForTest(mesh.positions);
                       putArray(out, mesh.positions);
                       putArray(out, mesh.indices);
                       putEdges(out, mesh);
                   }
               });
}
