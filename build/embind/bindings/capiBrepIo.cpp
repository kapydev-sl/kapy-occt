// engine/kernels/occt/build/embind/bindings/capiBrepIo.cpp
//
// The open-with-cache's shapes as exact bytes through the C API: the same
// `kapy_exact::writeExact` / `readExact` the embind functions move through
// emscripten's FS (exactBrep.cpp), with the bytes in the result arena instead
// of a file. `brepIo.ts` writes ONE compound for every shape asked for, so the
// bytes are a function of the shapes alone.
//
// `kapy_brep_write` blob: u32 count, u32 handle per shape. Answers the bytes.
// A geometry class the registry does not list is a failure (nothing is
// written) worded with the registry's own refusal.
//
// `kapy_brep_read` blob: the bytes themselves. Stores every child of the
// compound read, in order, and answers u32 count and one u32 handle each; the
// shapes carry no naming table (the host binds the cache's tables by index).
// Bytes that do not read are a failure and store nothing.
//
// The test-only perturbation 9 flips the last byte of what is written.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the format (exactBrepCore.cpp), the tables.

#include <string>
#include <vector>

#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>

#include "capiAsk.hxx"
#include "exactBrep.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('writeShapesBrep'), ('storeShapesBrep').
constexpr size_t SEED_WRITE = 370943555u;
constexpr size_t SEED_READ = 643100076u;
}  // namespace

KAPY_API int32_t kapy_brep_write(uint32_t ptr, uint32_t length) noexcept {
    return ask("writeShapesBrep", SEED_WRITE, ptr, length, [](Blob& in, Out& out) {
        const uint32_t count = in.u32();
        std::vector<Entry*> entries;
        for (uint32_t i = 0; i < count; i++) entries.push_back(&need(in.u32()));
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (Entry* e : entries) builder.Add(compound, e->shape);
        std::string bytes;
        const std::string refusal = kapy_exact::writeExact(compound, bytes);
        if (!refusal.empty() || bytes.empty()) {
            throw OpError(refusal.empty() ? "the shapes wrote no bytes" : refusal);
        }
        if (perturbation() == 9) bytes.back() = static_cast<char>(bytes.back() ^ 0xFF);
        out.bytes(bytes.data(), bytes.size());
    });
}

KAPY_API int32_t kapy_brep_read(uint32_t ptr, uint32_t length) noexcept {
    return runRaw("storeShapesBrep", SEED_READ, ptr, length, [](Blob& in) -> int32_t {
        const size_t size = in.remaining();
        const std::string bytes(reinterpret_cast<const char*>(in.take(size)), size);
        TopoDS_Shape compound;
        if (!kapy_exact::readExact(bytes, compound)) throw OpError("the bytes are not a B-Rep this kernel wrote");
        std::vector<TopoDS_Shape> children;
        for (TopoDS_Iterator it(compound, true, true); it.More(); it.Next()) {
            children.push_back(it.Value());
        }
        Out out;
        out.u32(static_cast<uint32_t>(children.size()));
        for (const TopoDS_Shape& child : children) out.u32(put(child));
        return out.send();
    });
}
