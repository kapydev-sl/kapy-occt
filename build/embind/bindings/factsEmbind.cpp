// services/occt/build/embind/bindings/factsEmbind.cpp
//
// `KapyFacts`, the naming facts as the TypeScript binding calls them: one
// static per builder of `namerFacts/builders.ts` (plus release, bind and
// adopt), each collecting what the core's namer reads straight from the maps
// and the live makers, and appending the record to the log (factsLog.hxx). The
// binding only routes here when the C transport is on; the log is drained by
// `kapy_facts_take` (factsCapi.cpp) and, for tests, by `KapyFacts.take`.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: collecting (facts*.cpp) or the C entry point.

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "factsInternal.hxx"

using namespace emscripten;
using namespace kapy_facts;

namespace {

struct KapyFacts {};

Maps maps(const val& holder) { return mapsOf(holder); }

void extrude(uint32_t id, uint32_t previous, const val& result, const std::string& bornIn,
             const std::string& roles, const std::string& suffix) {
    buildExtrude(id, previous, maps(result), bornIn, roles, suffix);
}

void boolean(uint32_t id, uint32_t previous, uint32_t tool, const val& result,
             const std::string& bornIn, int kind, const val& source, const val& previousMaps,
             const val& toolMaps) {
    const Maps prev = maps(previousMaps);
    if (toolMaps.isNull() || toolMaps.isUndefined()) {
        buildBoolean(id, previous, tool, maps(result), bornIn, kind, source, prev, nullptr);
        return;
    }
    const Maps t = maps(toolMaps);
    buildBoolean(id, previous, tool, maps(result), bornIn, kind, source, prev, &t);
}

void unify(uint32_t id, uint32_t previous, const val& result, const std::string& bornIn,
           const val& history, const val& previousMaps) {
    buildUnify(id, previous, maps(result), bornIn, history, maps(previousMaps));
}

void offsetFaces(uint32_t id, uint32_t previous, const val& result, const std::string& bornIn,
                 double offset) {
    buildOffsetFaces(id, previous, maps(result), bornIn, offset);
}

void subShape(uint32_t id, uint32_t previous, const val& piece, const val& parent,
              const std::string& bornIn) {
    buildSubShape(id, previous, maps(piece), maps(parent), bornIn);
}

void imported(uint32_t id, const val& result, const std::string& bornIn) {
    buildImported(id, maps(result), bornIn);
}

void box(uint32_t id, const val& result, const std::string& bornIn, double hx, double hy,
         double hz) {
    buildBox(id, maps(result), bornIn, hx, hy, hz);
}

void empty(uint32_t id) { buildEmpty(id); }

void preview(uint32_t id, const val& result, const std::string& bornIn) {
    buildPreview(id, maps(result), bornIn);
}

void releaseTable(uint32_t id) { kapy_facts::release(id); }
void bindTable(uint32_t id, const std::string& handle) { kapy_facts::bind(id, handle); }
void adoptTable(uint32_t id, const std::string& text) { kapy_facts::adopt(id, text); }

// The role facts of a swept profile as JSON; empty when the maker is not one
// the collector knows.
std::string roles(const std::string& kind, const val& maker, const TopoDS_Shape& shape,
                  const val& loops, bool withHoles, bool isFull, const std::string& sources,
                  bool hasDirection, double dx, double dy, double dz) {
    return sweptRoles(kind, maker, shape, loops, withHoles, isFull, sources, hasDirection, dx, dy,
                      dz);
}

std::string indexed(const std::string& prefix, const TopoDS_Shape& shape) {
    return indexedRoles(prefix, shape);
}

// The log drained into a Uint8Array copy: what the tests compare with the
// host's own writer.
val take() {
    const std::vector<uint8_t>& log = kapy_facts::bytes();
    val out = val::global("Uint8Array").new_(static_cast<unsigned>(log.size()));
    if (!log.empty()) {
        out.call<void>("set", val(typed_memory_view(log.size(), log.data())));
    }
    kapy_facts::clear();
    return out;
}

// A copy of the record appended last, without draining: the tests compare it
// with the host writer's bytes for the same construction.
val last() {
    const size_t n = kapy_facts::lastSize();
    val out = val::global("Uint8Array").new_(static_cast<unsigned>(n));
    if (n > 0) out.call<void>("set", val(typed_memory_view(n, kapy_facts::lastData())));
    return out;
}

unsigned pendingBytes() { return static_cast<unsigned>(kapy_facts::bytes().size()); }

}  // namespace

EMSCRIPTEN_BINDINGS(kapy_facts) {
    class_<KapyFacts>("KapyFacts")
        .class_function("extrude", &extrude)
        .class_function("boolean", &boolean)
        .class_function("unify", &unify)
        .class_function("offsetFaces", &offsetFaces)
        .class_function("subShape", &subShape)
        .class_function("imported", &imported)
        .class_function("box", &box)
        .class_function("empty", &empty)
        .class_function("preview", &preview)
        .class_function("release", &releaseTable)
        .class_function("bind", &bindTable)
        .class_function("adopt", &adoptTable)
        .class_function("roles", &roles)
        .class_function("indexed", &indexed)
        .class_function("take", &take)
        .class_function("last", &last)
        .class_function("pendingBytes", &pendingBytes);
}
