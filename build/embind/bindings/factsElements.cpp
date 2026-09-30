// services/occt/build/embind/bindings/factsElements.cpp
//
// The per-element facts of a shape's three maps: the raw rows as the facts'
// ELEMENTS block carries them (the same rows `KapyNamer.faceProps` /
// `edgeProps` / `vertexProps` write), and the JSON of one element's props for
// a piece's missing elements. `mapsOf` reads the three maps off the
// JavaScript object that holds them.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: what a fact means (the Rust core's namer).

#include <TopoDS.hxx>

#include "factsInternal.hxx"
#include "factsJson.hxx"

using emscripten::val;

namespace kapy_facts {

Maps mapsOf(const val& holder) {
    return Maps{&holder["faceMap"].as<const ShapeIndexedMap&>(),
                &holder["edgeMap"].as<const ShapeIndexedMap&>(),
                &holder["vertexMap"].as<const ShapeIndexedMap&>()};
}

Block elementsOf(const Maps& maps, bool each) {
    return elementsBlock(each, kapy_namer::face_props(*maps.face),
                         kapy_namer::edge_props(*maps.edge),
                         kapy_namer::vertex_props(*maps.vertex));
}

namespace {

// The surface classes of namer.cpp's `surface_type_code`, in its numbering.
const char* const kSurfaceTypes[] = {"plane", "cylinder", "cone", "sphere",
                                     "torus", "bspline",  "other"};

std::string faceJson(const TopoDS_Shape& shape) {
    double r[kapy_namer::kFacePropsStride];
    kapy_namer::face_row(TopoDS::Face(shape), r);
    const bool planar = r[0] == 1;
    const int code = static_cast<int>(r[8]);
    const char* type = planar ? "plane" : (code >= 0 && code < 7 ? kSurfaceTypes[code] : "other");
    return std::string("{\"normal\":") + jvec3(r + 1) + ",\"centroid\":" + jvec3(r + 4) +
           ",\"area\":" + jnum(r[7]) + ",\"isPlanar\":" + jbool(planar) +
           ",\"surfaceType\":\"" + type + "\"}";
}

std::string edgeJson(const TopoDS_Shape& shape) {
    double r[kapy_namer::kEdgePropsStride];
    kapy_namer::edge_row(TopoDS::Edge(shape), r);
    const int type = static_cast<int>(r[0]);
    std::string out = "{\"endpoints\":[" + jvec3(r + 1) + "," + jvec3(r + 4) +
                      "],\"midpoint\":" + jvec3(r + 7) + ",\"length\":" + jnum(r[10]) +
                      ",\"edgeType\":\"" + (type == 0 ? "line" : type == 1 ? "circle" : "other") +
                      "\"";
    if (r[11] == 1) out += ",\"center\":" + jvec3(r + 12) + ",\"circleRadius\":" + jnum(r[15]);
    return out + "}";
}

std::string vertexJson(const TopoDS_Shape& shape) {
    double r[kapy_namer::kVertexPropsStride];
    kapy_namer::vertex_row(TopoDS::Vertex(shape), r);
    return jvec3(r);
}

// One kind of a piece: `"index":[...]` and `"missing":{"i":props,...}`.
template <typename Json>
void pieceKind(const ShapeIndexedMap& piece, const ShapeIndexedMap& parent, Json json,
               std::string& index, std::string& missing) {
    index = "[";
    missing = "{";
    bool first = true;
    for (int i = 1; i <= piece.Extent(); ++i) {
        const int at = parent.FindIndex(piece.FindKey(i)) - 1;
        index += (i > 1 ? "," : "") + std::to_string(at);
        // An element the parent's table has no entry for (at < 0; the table is
        // parallel to its map, so a found index always has one).
        if (at < 0) {
            missing += (first ? "" : ",") + jstr(std::to_string(i - 1)) + ":" + json(piece.FindKey(i));
            first = false;
        }
    }
    index += "]";
    missing += "}";
}

}  // namespace

std::string pieceSkeleton(const std::string& bornIn, const Maps& piece, const Maps& parent) {
    std::string fi, fm, ei, em, vi, vm;
    pieceKind(*piece.face, *parent.face, faceJson, fi, fm);
    pieceKind(*piece.edge, *parent.edge, edgeJson, ei, em);
    pieceKind(*piece.vertex, *parent.vertex, vertexJson, vi, vm);
    return "{\"builder\":\"subShape\",\"bornIn\":" + jstr(bornIn) +
           ",\"piece\":{\"parentIndex\":{\"face\":" + fi + ",\"edge\":" + ei + ",\"vertex\":" + vi +
           "},\"missing\":{\"face\":" + fm + ",\"edge\":" + em + ",\"vertex\":" + vm + "}}}";
}

}  // namespace kapy_facts
