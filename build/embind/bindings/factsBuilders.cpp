// engine/kernels/occt/build/embind/bindings/factsBuilders.cpp
//
// One construction of the facts log per builder of the host's naming
// (`namerFacts/builders.ts`): the skeleton JSON the core reads, with every
// bulk field (the element rows, the histories) replaced by `{"$b":k}` and
// carried as block `k`. Nothing here decides a name.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: collecting the rows (factsElements.cpp), the
// histories (factsHistory.cpp), the roles (factsRoles.cpp).

#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS.hxx>

#include "factsInternal.hxx"
#include "factsJson.hxx"

using emscripten::val;

namespace kapy_facts {

namespace {

std::string ref(size_t k) { return "{\"$b\":" + std::to_string(k) + "}"; }

std::string head(const char* builder, const std::string& bornIn) {
    return std::string("{\"builder\":\"") + builder + "\",\"bornIn\":" + jstr(bornIn);
}

// `{"face":{"$b":k},"edge":{"$b":k+1},"vertex":{"$b":k+2}}`.
std::string historyRef(size_t k) {
    return "{\"face\":" + ref(k) + ",\"edge\":" + ref(k + 1) + ",\"vertex\":" + ref(k + 2) + "}";
}

void extend(std::vector<Block>& blocks, std::vector<Block>&& more) {
    for (Block& b : more) blocks.push_back(std::move(b));
}

}  // namespace

void buildExtrude(uint32_t id, uint32_t previous, const Maps& result, const std::string& bornIn,
                  const std::string& rolesJson, const std::string& roleSuffix) {
    std::string skeleton = head("extrude", bornIn) + ",\"elements\":" + ref(0) +
                           ",\"roles\":" + rolesJson;
    if (!roleSuffix.empty()) skeleton += ",\"roleSuffix\":" + jstr(roleSuffix);
    construct(id, previous, 0, skeleton + "}", {elementsOf(result, false)});
}

void buildBoolean(uint32_t id, uint32_t previous, uint32_t tool, const Maps& result,
                  const std::string& bornIn, int sourceKind, const val& source,
                  const Maps& previousMaps, const Maps* toolMaps) {
    std::vector<Block> blocks{elementsOf(result, false)};
    extend(blocks, historyBlocks(sourceKind, source, previousMaps, result));
    std::string skeleton = head("boolean", bornIn) + ",\"elements\":" + ref(0) +
                           ",\"previous\":" + historyRef(1) + ",\"tool\":";
    if (toolMaps) {
        extend(blocks, historyBlocks(sourceKind, source, *toolMaps, result));
        skeleton += historyRef(4);
    } else {
        skeleton += "null";
    }
    construct(id, previous, tool, skeleton + "}", blocks);
}

void buildBooleanOfMaker(uint32_t id, uint32_t previous, uint32_t tool, const Maps& result,
                         const std::string& bornIn, BRepBuilderAPI_MakeShape& maker,
                         const Maps& previousMaps, const Maps* toolMaps) {
    std::vector<Block> blocks{elementsOf(result, false)};
    extend(blocks, historyBlocksOfMaker(maker, previousMaps, result));
    std::string skeleton = head("boolean", bornIn) + ",\"elements\":" + ref(0) +
                           ",\"previous\":" + historyRef(1) + ",\"tool\":";
    if (toolMaps) {
        extend(blocks, historyBlocksOfMaker(maker, *toolMaps, result));
        skeleton += historyRef(4);
    } else {
        skeleton += "null";
    }
    construct(id, previous, tool, skeleton + "}", blocks);
}

void buildBooleanOfMakers(uint32_t id, uint32_t previous, const Maps& result,
                          const std::string& bornIn,
                          const std::vector<BRepBuilderAPI_MakeShape*>& makers,
                          const Maps& previousMaps) {
    std::vector<Block> blocks{elementsOf(result, false)};
    extend(blocks, historyBlocksOfMakers(makers, previousMaps, result));
    construct(id, previous, 0,
              head("boolean", bornIn) + ",\"elements\":" + ref(0) +
                  ",\"previous\":" + historyRef(1) + ",\"tool\":null}",
              blocks);
}

void buildUnify(uint32_t id, uint32_t previous, const Maps& result, const std::string& bornIn,
                const val& history, const Maps& previousMaps) {
    buildUnifyOf(id, previous, result, bornIn, history.as<BRepTools_History&>(), previousMaps);
}

void buildUnifyOf(uint32_t id, uint32_t previous, const Maps& result, const std::string& bornIn,
                  BRepTools_History& history, const Maps& previousMaps) {
    std::vector<Block> blocks{elementsOf(result, false)};
    extend(blocks, historyBlocksOfHistory(history, previousMaps, result));
    construct(id, previous, 0,
              head("unify", bornIn) + ",\"elements\":" + ref(0) + ",\"previous\":" + historyRef(1) +
                  "}",
              blocks);
}

void buildOffsetFaces(uint32_t id, uint32_t previous, const Maps& result,
                      const std::string& bornIn, double offset) {
    construct(id, previous, 0,
              head("offsetFaces", bornIn) + ",\"elements\":" + ref(0) +
                  ",\"offset\":" + jnum(offset) + "}",
              {elementsOf(result, false)});
}

void buildSubShape(uint32_t id, uint32_t previous, const Maps& piece, const Maps& parent,
                   const std::string& bornIn) {
    construct(id, previous, 0, pieceSkeleton(bornIn, piece, parent), {});
}

void buildImported(uint32_t id, const Maps& result, const std::string& bornIn) {
    construct(id, 0, 0, head("imported", bornIn) + ",\"elements\":" + ref(0) + "}",
              {elementsOf(result, true)});
}

void buildBox(uint32_t id, const Maps& result, const std::string& bornIn, double hx, double hy,
              double hz) {
    const double half[3] = {hx, hy, hz};
    construct(id, 0, 0,
              head("box", bornIn) + ",\"elements\":" + ref(0) + ",\"half\":" + jvec3(half) + "}",
              {elementsOf(result, true)});
}

void buildEmpty(uint32_t id) { construct(id, 0, 0, "{\"builder\":\"empty\"}", {}); }

void buildPreview(uint32_t id, const Maps& result, const std::string& bornIn) {
    std::string planar = "[";
    for (int i = 1; i <= result.face->Extent(); ++i) {
        const BRepAdaptor_Surface surface(TopoDS::Face(result.face->FindKey(i)), false);
        planar += std::string(i > 1 ? "," : "") + jbool(surface.GetType() == GeomAbs_Plane);
    }
    std::string vertices = "[";
    for (int i = 1; i <= result.vertex->Extent(); ++i) {
        double r[kapy_namer::kVertexPropsStride];
        kapy_namer::vertex_row(TopoDS::Vertex(result.vertex->FindKey(i)), r);
        vertices += std::string(i > 1 ? "," : "") + jvec3(r);
    }
    construct(id, 0, 0,
              head("preview", bornIn) + ",\"isPlanar\":" + planar +
                  "],\"edges\":" + std::to_string(result.edge->Extent()) +
                  ",\"vertices\":" + vertices + "]}",
              {});
}

}  // namespace kapy_facts
