// services/occt/build/embind/bindings/namerCore.hxx
//
// The arithmetic of the naming facts, shared by the two callers that need it:
// the embind batch extractors (namer.cpp, `KapyNamer`) and the facts
// collectors (facts*.cpp, `KapyFacts`, the C API's `kapy_facts_take`). One
// definition of every row, so the two can never disagree by a bit: the
// signatures they produce are matched across regenerations.
//
// Who includes it: namer.cpp, facts*.cpp.
// What does NOT belong here: the embind registrations, the facts log.

#pragma once

#include <vector>

#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepTools_History.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>

// What `TopTools_IndexedMapOfShape` is, spelled out.
using ShapeIndexedMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

namespace kapy_namer {

// The doubles one face, one edge and one vertex row hold.
inline constexpr int kFacePropsStride = 9;
inline constexpr int kEdgePropsStride = 16;
inline constexpr int kVertexPropsStride = 3;

// One row: face [isPlanar, normal xyz, centroid xyz, area, surface class],
// edge [type, p0 xyz, p1 xyz, mid xyz, length, hasCircle, centre xyz, radius],
// vertex [position xyz].
void face_row(const TopoDS_Face& face, double* out);
void edge_row(const TopoDS_Edge& edge, double* out);
void vertex_row(const TopoDS_Vertex& vertex, double* out);

// Every row of a map, in map order.
std::vector<double> face_props(const ShapeIndexedMap& map);
std::vector<double> edge_props(const ShapeIndexedMap& map);
std::vector<double> vertex_props(const ShapeIndexedMap& map);

// An operand's history against a result for one shape kind (a
// `TopAbs_ShapeEnum` value), per operand element `[self, modCount, mod...,
// genCount, gen...]`.
std::vector<int> propagate_maker(BRepBuilderAPI_MakeShape& op, const ShapeIndexedMap& opMap,
                                 const ShapeIndexedMap& resultMap, int kind);
std::vector<int> propagate_history(BRepTools_History& op, const ShapeIndexedMap& opMap,
                                   const ShapeIndexedMap& resultMap, int kind);

}  // namespace kapy_namer
