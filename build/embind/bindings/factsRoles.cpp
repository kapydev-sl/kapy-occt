// engine/kernels/occt/build/embind/bindings/factsRoles.cpp
//
// The role facts of a swept profile (a straight prism, a revolve, a sweep
// along a path), collected while its maker is alive: the result's element
// counts, where `FirstShape` / `LastShape` sit among its faces, and for every
// profile edge (vertex) of every loop where it sits in the result and which
// faces (edges) `Generated` answered for it; on a prism with a sweep vector
// also the top-cap edges and the base edges' midpoints the top-edge
// provenance is matched on. Written as the JSON of `RoleFacts`
// (`facts.types.ts`) the core reads; what the facts MEAN is the core's.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: role names, or building the shape.

#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepPrimAPI_MakeSweep.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>

#include "factsInternal.hxx"
#include "factsJson.hxx"

using emscripten::val;

namespace kapy_facts {

namespace {

struct ResultMaps {
    ShapeIndexedMap face, edge, vertex;
};

int indexIn(const ShapeIndexedMap& map, const TopoDS_Shape& s) { return map.FindIndex(s) - 1; }

// Indices (in `map`) of the outputs of `Generated(base)` of kind `want`, in
// list order, as a JSON array.
std::string generatedJson(BRepBuilderAPI_MakeShape& maker, const TopoDS_Shape& base,
                          TopAbs_ShapeEnum want, const ShapeIndexedMap& map) {
    std::vector<int> out;
    for (const TopoDS_Shape& s : maker.Generated(base)) {
        if (s.ShapeType() != want) continue;
        const int idx = indexIn(map, s);
        if (idx >= 0) out.push_back(idx);
    }
    return jints(out);
}

// The two caps among the result's faces (-1 when absent), or null when the
// maker is not a swept one.
std::string capsJson(BRepBuilderAPI_MakeShape& maker, const ShapeIndexedMap& faces) {
    TopoDS_Shape first, last;
    if (auto* sweep = dynamic_cast<BRepPrimAPI_MakeSweep*>(&maker)) {
        first = sweep->FirstShape();
        last = sweep->LastShape();
    } else if (auto* pipe = dynamic_cast<BRepOffsetAPI_MakePipe*>(&maker)) {
        first = pipe->FirstShape();
        last = pipe->LastShape();
    }
    return "{\"first\":" + std::to_string(indexIn(faces, first)) +
           ",\"last\":" + std::to_string(indexIn(faces, last)) + "}";
}

std::string countsJson(const ResultMaps& m) {
    return "{\"face\":" + std::to_string(m.face.Extent()) +
           ",\"edge\":" + std::to_string(m.edge.Extent()) +
           ",\"vertex\":" + std::to_string(m.vertex.Extent()) + "}";
}

// `[{"self":i,"gen":[...]},...]` of the base shapes in `list`.
std::string basesJson(BRepBuilderAPI_MakeShape& maker, const std::vector<TopoDS_Shape>& list,
                      const ShapeIndexedMap& own, TopAbs_ShapeEnum want,
                      const ShapeIndexedMap& generatedIn) {
    std::string out = "[";
    const size_t n = list.size();
    for (size_t i = 0; i < n; ++i) {
        const TopoDS_Shape& base = list[i];
        out += std::string(i ? "," : "") + "{\"self\":" + std::to_string(indexIn(own, base)) +
               ",\"gen\":" + generatedJson(maker, base, want, generatedIn) + "}";
    }
    return out + "]";
}

std::string loopsJson(BRepBuilderAPI_MakeShape& maker, const std::vector<Loop>& loops,
                      const ResultMaps& m) {
    std::string out = "[";
    for (size_t i = 0; i < loops.size(); ++i) {
        out += std::string(i ? "," : "") + "{\"edges\":" +
               basesJson(maker, loops[i].edges, m.edge, TopAbs_FACE, m.face) +
               ",\"verts\":" + basesJson(maker, loops[i].verts, m.vertex, TopAbs_EDGE, m.edge) +
               "}";
    }
    return out + "]";
}

// The top-cap edges of a prism swept by `direction` and every base edge's
// midpoint per loop.
std::string topEdgesJson(BRepBuilderAPI_MakeShape& maker, const std::vector<Loop>& loops,
                         const ResultMaps& m, const double* direction) {
    std::string mids = "[";
    for (size_t i = 0; i < loops.size(); ++i) {
        mids += std::string(i ? "," : "") + "[";
        const std::vector<TopoDS_Shape>& edges = loops[i].edges;
        for (size_t j = 0; j < edges.size(); ++j) {
            double r[kapy_namer::kEdgePropsStride];
            kapy_namer::edge_row(TopoDS::Edge(edges[j]), r);
            mids += std::string(j ? "," : "") + jvec3(r + 7);
        }
        mids += "]";
    }
    mids += "]";
    std::string top = "[";
    if (auto* sweep = dynamic_cast<BRepPrimAPI_MakeSweep*>(&maker)) {
        const TopoDS_Shape cap = sweep->LastShape();
        bool first = true;
        for (TopExp_Explorer ex(cap, TopAbs_EDGE); ex.More(); ex.Next()) {
            double r[kapy_namer::kEdgePropsStride];
            kapy_namer::edge_row(TopoDS::Edge(ex.Current()), r);
            top += std::string(first ? "" : ",") + "{\"index\":" +
                   std::to_string(indexIn(m.edge, ex.Current())) + ",\"mid\":" + jvec3(r + 7) + "}";
            first = false;
        }
    }
    return "{\"direction\":" + jvec3(direction) + ",\"baseMids\":" + mids + ",\"top\":" + top +
           "]}";
}

ResultMaps mapsOfShape(const TopoDS_Shape& shape) {
    ResultMaps m;
    TopExp::MapShapes(shape, TopAbs_FACE, m.face);
    TopExp::MapShapes(shape, TopAbs_EDGE, m.edge);
    TopExp::MapShapes(shape, TopAbs_VERTEX, m.vertex);
    return m;
}

}  // namespace

std::string prismRolesOf(BRepBuilderAPI_MakeShape& maker, const TopoDS_Shape& shape,
                         const std::vector<Loop>& loops, bool withHoles,
                         const std::string& sourcesJson, const double* direction) {
    const ResultMaps m = mapsOfShape(shape);
    return std::string("{\"kind\":\"prism\",\"withHoles\":") + jbool(withHoles) +
           ",\"counts\":" + countsJson(m) + ",\"caps\":" + capsJson(maker, m.face) +
           ",\"loops\":" + loopsJson(maker, loops, m) + ",\"sources\":" + sourcesJson +
           ",\"topEdges\":" + (direction ? topEdgesJson(maker, loops, m, direction) : std::string("null")) +
           "}";
}

std::string revolveRolesOf(BRepBuilderAPI_MakeShape& maker, const TopoDS_Shape& shape,
                           const std::vector<Loop>& loops, bool withHoles, bool isFull) {
    const ResultMaps m = mapsOfShape(shape);
    return std::string("{\"kind\":\"revolve\",\"withHoles\":") + jbool(withHoles) +
           ",\"isFull\":" + jbool(isFull) + ",\"counts\":" + countsJson(m) +
           ",\"caps\":" + (isFull ? std::string("null") : capsJson(maker, m.face)) +
           ",\"loops\":" + loopsJson(maker, loops, m) + "}";
}

std::string sweepRolesOf(BRepBuilderAPI_MakeShape& maker, const TopoDS_Shape& shape,
                         const std::vector<Loop>& loops, bool withHoles) {
    const ResultMaps m = mapsOfShape(shape);
    return std::string("{\"kind\":\"sweep\",\"withHoles\":") + jbool(withHoles) +
           ",\"counts\":" + countsJson(m) + ",\"caps\":" + capsJson(maker, m.face) +
           ",\"loops\":" + loopsJson(maker, loops, m) + "}";
}

std::string sweptRoles(const std::string& kind, const val& makerVal, const TopoDS_Shape& shape,
                       const val& loopsVal, bool withHoles, bool isFull,
                       const std::string& sourcesJson, bool hasDirection, double dx, double dy,
                       double dz) {
    BRepBuilderAPI_MakeShape& maker = makerVal.as<BRepBuilderAPI_MakeShape&>();
    std::vector<Loop> loops;
    const unsigned count = loopsVal["length"].as<unsigned>();
    for (unsigned i = 0; i < count; ++i) {
        Loop loop;
        for (const char* key : {"edges", "verts"}) {
            const val list = loopsVal[i][key];
            std::vector<TopoDS_Shape>& into = key[0] == 'e' ? loop.edges : loop.verts;
            const unsigned n = list["length"].as<unsigned>();
            for (unsigned k = 0; k < n; ++k) into.push_back(list[k].as<const TopoDS_Shape&>());
        }
        loops.push_back(std::move(loop));
    }
    const ResultMaps m = mapsOfShape(shape);
    const std::string head = "{\"kind\":" + jstr(kind) + ",\"withHoles\":" + jbool(withHoles);
    if (kind == "prism") {
        const double d[3] = {dx, dy, dz};
        return head + ",\"counts\":" + countsJson(m) + ",\"caps\":" + capsJson(maker, m.face) +
               ",\"loops\":" + loopsJson(maker, loops, m) + ",\"sources\":" + sourcesJson +
               ",\"topEdges\":" + (hasDirection ? topEdgesJson(maker, loops, m, d) : "null") + "}";
    }
    if (kind == "revolve") {
        return head + ",\"isFull\":" + jbool(isFull) + ",\"counts\":" + countsJson(m) +
               ",\"caps\":" + (isFull ? "null" : capsJson(maker, m.face)) +
               ",\"loops\":" + loopsJson(maker, loops, m) + "}";
    }
    if (kind == "sweep") {
        return head + ",\"counts\":" + countsJson(m) + ",\"caps\":" + capsJson(maker, m.face) +
               ",\"loops\":" + loopsJson(maker, loops, m) + "}";
    }
    return std::string();
}

std::string indexedRoles(const std::string& prefix, const TopoDS_Shape& shape) {
    return "{\"kind\":\"indexed\",\"prefix\":" + jstr(prefix) +
           ",\"counts\":" + countsJson(mapsOfShape(shape)) + "}";
}

}  // namespace kapy_facts
