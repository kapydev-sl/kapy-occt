// services/occt/build/embind/bindings/capiExport.cpp
//
// STL and STEP export through the C API: `exportShape.ts` call for call. The
// shapes are packed into one compound, written to a file of emscripten's FS
// (OCCT's writers want a path) and the bytes read back into the result arena,
// so no embind array crosses the boundary. The empty selection stays the host's
// to refuse, in the words the binding uses.
//
// `kapy_export_stl` blob: u32 count, u32 handle per shape, f64 linear and f64
// angular deflection. `kapy_export_step` blob: u32 count, u32 handle per shape.
// Both answer the file's bytes. A writer that does not finish fails the call, in the
// words the export has always raised.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the empty-selection check, the download.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Message_ProgressRange.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <StlAPI.hxx>
#include <TopoDS_Compound.hxx>

#include "capiAsk.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('exportStl'), ('exportStep').
constexpr size_t SEED_STL = 1695461688u;
constexpr size_t SEED_STEP = 556556536u;
const char* const TMP_STL = "/export.stl";
const char* const TMP_STEP = "/export.step";

std::vector<Entry*> readHandles(Blob& in) {
    const uint32_t count = in.u32();
    std::vector<Entry*> entries;
    for (uint32_t i = 0; i < count; i++) entries.push_back(&need(in.u32()));
    return entries;
}

TopoDS_Compound compoundOf(const std::vector<Entry*>& entries) {
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (Entry* e : entries) builder.Add(compound, e->shape);
    return compound;
}

// The bytes of `path`, which is removed whether or not it could be read.
std::string takeFile(const char* path) {
    std::ifstream is(path, std::ios::in | std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    std::remove(path);
    return bytes;
}
}  // namespace

KAPY_API int32_t kapy_export_stl(uint32_t ptr, uint32_t length) noexcept {
    return ask("exportStl", SEED_STL, ptr, length, [](Blob& in, Out& out) {
        const std::vector<Entry*> entries = readHandles(in);
        const double linear = in.f64();
        const double angular = in.f64();
        const TopoDS_Compound compound = compoundOf(entries);
        {
            BRepMesh_IncrementalMesh mesher(compound, linear, false, angular, false);
        }
        const bool ok = StlAPI::Write(compound, TMP_STL, false);
        const std::string bytes = takeFile(TMP_STL);
        if (!ok || bytes.empty()) throw OpError("OCCT StlAPI.Write returned false.");
        out.bytes(bytes.data(), bytes.size());
    });
}

KAPY_API int32_t kapy_export_step(uint32_t ptr, uint32_t length) noexcept {
    return ask("exportStep", SEED_STEP, ptr, length, [](Blob& in, Out& out) {
        const std::vector<Entry*> entries = readHandles(in);
        const TopoDS_Compound compound = compoundOf(entries);
        STEPControl_Writer writer;
        const IFSelect_ReturnStatus transfer =
            writer.Transfer(compound, STEPControl_AsIs, true, Message_ProgressRange());
        if (transfer != IFSelect_RetDone) {
            takeFile(TMP_STEP);
            throw OpError("STEP transfer failed with status " + std::to_string(static_cast<int>(transfer)));
        }
        const IFSelect_ReturnStatus written = writer.Write(TMP_STEP);
        const std::string bytes = takeFile(TMP_STEP);
        if (written != IFSelect_RetDone || bytes.empty()) {
            throw OpError("STEP write failed with status " + std::to_string(static_cast<int>(written)));
        }
        out.bytes(bytes.data(), bytes.size());
    });
}
