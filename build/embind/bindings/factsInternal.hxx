// services/occt/build/embind/bindings/factsInternal.hxx
//
// What the facts collectors share: how a JavaScript object carrying a shape's
// three maps is read, where an operand's history comes from, and the
// declarations of the collectors that live in their own files. Not a public
// surface: the embind registration (factsEmbind.cpp) is the only caller.
//
// Who includes it: facts*.cpp.
// What does NOT belong here: the log (factsLog.hxx), JSON spellings
// (factsJson.hxx), the row arithmetic (namerCore.hxx).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <emscripten/val.h>

#include "factsLog.hxx"
#include "namerCore.hxx"

namespace kapy_facts {

// A shape's three canonical maps, borrowed from the JavaScript object that
// holds them (a `TopoNamer`, or the `{faceMap, edgeMap, vertexMap}` a builder
// just made). Valid while that object is alive.
struct Maps {
    const ShapeIndexedMap* face;
    const ShapeIndexedMap* edge;
    const ShapeIndexedMap* vertex;
};

Maps mapsOf(const emscripten::val& holder);

// Where an operand's history comes from, as the binding names them: one maker
// (boolean, fillet, chamfer, transform), several behind one compound (a shell
// of a multi-solid body), or a `BRepTools_History` (unify).
enum SourceKind { kSourceMaker = 0, kSourceMakers = 1, kSourceHistory = 2 };

// The three INTS blocks (face, edge, vertex) of `operand`'s history against
// `result`.
std::vector<Block> historyBlocks(int sourceKind, const emscripten::val& source,
                                 const Maps& operand, const Maps& result);

// The ELEMENTS block of a result's maps.
Block elementsOf(const Maps& maps, bool each);

// The skeleton of a piece (a solid of a shape the parent already names): each
// element's index in the parent's map, and the props of the ones it lacks.
std::string pieceSkeleton(const std::string& bornIn, const Maps& piece, const Maps& parent);

// The role facts of a swept profile, as the JSON the core reads; empty when
// the maker is not one this collector knows (the host then collects them).
std::string sweptRoles(const std::string& kind, const emscripten::val& maker,
                       const TopoDS_Shape& shape, const emscripten::val& loops, bool withHoles,
                       bool isFull, const std::string& sourcesJson, bool hasDirection,
                       double dx, double dy, double dz);

// The role facts of index roles with a fixed prefix.
std::string indexedRoles(const std::string& prefix, const TopoDS_Shape& shape);

// The constructions, one per builder of builders.ts: each appends its record
// to the log under table `id`, against the tables `previous` and `tool`.
void buildExtrude(uint32_t id, uint32_t previous, const Maps& result, const std::string& bornIn,
                  const std::string& rolesJson, const std::string& roleSuffix);
void buildBoolean(uint32_t id, uint32_t previous, uint32_t tool, const Maps& result,
                  const std::string& bornIn, int sourceKind, const emscripten::val& source,
                  const Maps& previousMaps, const Maps* toolMaps);
void buildUnify(uint32_t id, uint32_t previous, const Maps& result, const std::string& bornIn,
                const emscripten::val& history, const Maps& previousMaps);
void buildOffsetFaces(uint32_t id, uint32_t previous, const Maps& result,
                      const std::string& bornIn, double offset);
void buildSubShape(uint32_t id, uint32_t previous, const Maps& piece, const Maps& parent,
                   const std::string& bornIn);
void buildImported(uint32_t id, const Maps& result, const std::string& bornIn);
void buildBox(uint32_t id, const Maps& result, const std::string& bornIn, double hx, double hy,
              double hz);
void buildEmpty(uint32_t id);
void buildPreview(uint32_t id, const Maps& result, const std::string& bornIn);

}  // namespace kapy_facts
