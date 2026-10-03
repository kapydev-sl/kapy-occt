// engine/kernels/occt/build/embind/bindings/capiPush.cpp
//
// The push-face builder of the C API (`extrudeFaceShape`): one face of a body
// the kernel already holds, moved by a translation and swept along a vector.
// It is `builders/extrudeFace.ts` call for call, including the roles it
// collects: for every loop of the copied face the edges and vertices in wire
// order, where each of them sits in the result, which faces and edges the
// prism generated for it, and which edge of the SOURCE body each one copies.
//
// The result is named against the source body's naming table, which the store
// keeps with the source's entry.
//
// Blob: bornIn, roleSuffix, u32 source handle, u32 face index,
//   translation[3], direction[3]
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the other prism builders (capiPrism.cpp).

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <ShapeAnalysis.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "factsJson.hxx"

using namespace kapy_capi;
using kapy_facts::jints;
using kapy_facts::jbool;

namespace {

// serialSeedOf('extrudeFaceShape').
constexpr size_t SEED_EXTRUDE_FACE = 500334386u;

// One loop of a face: whether it is the outer one, and its edges and the
// vertex each starts at, in wire-explorer order.
struct FaceLoop {
    bool isOuter = false;
    std::vector<TopoDS_Shape> edges;
    std::vector<TopoDS_Shape> verts;
};

// The loops of `face`, the outer one first (the first is taken as outer when
// the kernel cannot tell).
std::vector<FaceLoop> collectLoops(const TopoDS_Face& face) {
    TopoDS_Wire outer;
    try {
        outer = ShapeAnalysis::OuterWire(face);
    } catch (...) {
        outer = TopoDS_Wire();
    }
    std::vector<FaceLoop> loops;
    for (TopExp_Explorer wx(face, TopAbs_WIRE); wx.More(); wx.Next()) {
        const TopoDS_Wire wire = TopoDS::Wire(wx.Current());
        FaceLoop loop;
        BRepTools_WireExplorer ex;
        ex.Init(wire);
        while (ex.More()) {
            loop.edges.push_back(ex.Current());
            loop.verts.push_back(ex.CurrentVertex());
            ex.Next();
        }
        if (loop.edges.empty()) continue;
        loop.isOuter = !outer.IsNull() && wire.IsSame(outer);
        loops.push_back(std::move(loop));
    }
    std::stable_sort(loops.begin(), loops.end(), [](const FaceLoop& a, const FaceLoop& b) {
        return a.isOuter && !b.isOuter;
    });
    if (!loops.empty()) loops[0].isOuter = true;
    return loops;
}

int indexIn(const TopTools_IndexedMapOfShape& map, const TopoDS_Shape& s) {
    const int at = map.FindIndex(s);
    return at > 0 ? at - 1 : -1;
}

// The indices (in `map`) of the `want` shapes the prism generated from `base`.
std::vector<int> generatedIn(BRepPrimAPI_MakePrism& prism, const TopoDS_Shape& base,
                             TopAbs_ShapeEnum want, const TopTools_IndexedMapOfShape& map) {
    std::vector<int> out;
    for (const TopoDS_Shape& s : prism.Generated(base)) {
        if (s.ShapeType() != want) continue;
        const int at = indexIn(map, s);
        if (at >= 0) out.push_back(at);
    }
    return out;
}

// The indices (in `map`) of the `kind` sub-shapes of `shape`, in explorer
// order, unfound ones as -1.
std::vector<int> subIndices(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind,
                            const TopTools_IndexedMapOfShape& map) {
    std::vector<int> out;
    for (TopExp_Explorer ex(shape, kind); ex.More(); ex.Next()) out.push_back(indexIn(map, ex.Current()));
    return out;
}

// The role facts of the pushed face, as the JSON `RoleFacts` the core reads.
std::string rolesOf(BRepPrimAPI_MakePrism& prism, const TopoDS_Shape& shape,
                    const std::vector<FaceLoop>& loops,
                    const std::vector<std::vector<TopoDS_Shape>>& sourceEdges, int faceIndex,
                    const TopTools_IndexedMapOfShape& sourceEdgeMap) {
    TopTools_IndexedMapOfShape faces, edges, verts;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    TopExp::MapShapes(shape, TopAbs_VERTEX, verts);

    std::map<int, std::vector<int>> faceEdges;
    std::string loopsOut = "[";
    for (size_t k = 0; k < loops.size(); ++k) {
        const FaceLoop& loop = loops[k];
        std::string edgesOut = "[";
        for (size_t i = 0; i < loop.edges.size(); ++i) {
            const std::vector<int> gen = generatedIn(prism, loop.edges[i], TopAbs_FACE, faces);
            for (int fi : gen) {
                if (faceEdges.count(fi) == 0) {
                    faceEdges[fi] = subIndices(faces.FindKey(fi + 1), TopAbs_EDGE, edges);
                }
            }
            edgesOut += std::string(i ? "," : "") + "{\"self\":" +
                        std::to_string(indexIn(edges, loop.edges[i])) + ",\"gen\":" + jints(gen) +
                        ",\"src\":" + std::to_string(indexIn(sourceEdgeMap, sourceEdges[k][i])) +
                        "}";
        }
        edgesOut += "]";
        std::string vertsOut = "[";
        for (size_t i = 0; i < loop.verts.size(); ++i) {
            std::string genOut = "[";
            const std::vector<int> gen = generatedIn(prism, loop.verts[i], TopAbs_EDGE, edges);
            for (size_t g = 0; g < gen.size(); ++g) {
                genOut += std::string(g ? "," : "") + "{\"edge\":" + std::to_string(gen[g]) +
                          ",\"verts\":" +
                          jints(subIndices(edges.FindKey(gen[g] + 1), TopAbs_VERTEX, verts)) + "}";
            }
            genOut += "]";
            vertsOut += std::string(i ? "," : "") + "{\"self\":" +
                        std::to_string(indexIn(verts, loop.verts[i])) + ",\"gen\":" + genOut + "}";
        }
        vertsOut += "]";
        loopsOut += std::string(k ? "," : "") + "{\"isOuter\":" + jbool(loop.isOuter) +
                    ",\"edges\":" + edgesOut + ",\"verts\":" + vertsOut + "}";
    }
    loopsOut += "]";

    std::string faceEdgesOut = "{";
    bool first = true;
    for (const auto& [fi, list] : faceEdges) {
        faceEdgesOut += std::string(first ? "" : ",") + "\"" + std::to_string(fi) + "\":" + jints(list);
        first = false;
    }
    faceEdgesOut += "}";

    return "{\"kind\":\"extrudeFace\",\"counts\":{\"face\":" + std::to_string(faces.Extent()) +
           ",\"edge\":" + std::to_string(edges.Extent()) +
           ",\"vertex\":" + std::to_string(verts.Extent()) +
           "},\"srcFace\":" + std::to_string(faceIndex) +
           ",\"caps\":{\"first\":" + std::to_string(indexIn(faces, prism.FirstShape())) +
           ",\"last\":" + std::to_string(indexIn(faces, prism.LastShape())) +
           "},\"loops\":" + loopsOut + ",\"faceEdges\":" + faceEdgesOut + "}";
}

}  // namespace

KAPY_API int32_t kapy_extrude_face(uint32_t ptr, uint32_t length) noexcept {
    return runOp("extrudeFaceShape", SEED_EXTRUDE_FACE, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const std::string suffix = in.str();
        const uint32_t source = in.u32();
        const uint32_t faceIndex = in.u32();
        double translation[3], direction[3];
        for (double& v : translation) v = in.f64();
        for (double& v : direction) v = in.f64();

        Entry* entry = find(source);
        if (!entry) throw OpError("OCCT shape handle not found");
        ensureMaps(*entry);
        if (faceIndex >= static_cast<uint32_t>(entry->faces.Extent())) {
            throw OpError("extrudeFace: face index " + std::to_string(faceIndex) +
                          " out of range");
        }
        if (!(std::hypot(std::hypot(direction[0], direction[1]), direction[2]) > 0)) {
            throw OpError("extrudeFace direction vector must be non-zero");
        }
        const TopoDS_Face sourceFace = TopoDS::Face(entry->faces.FindKey(static_cast<Standard_Integer>(faceIndex + 1)));
        gp_Trsf move;
        move.SetTranslation(gp_Vec(translation[0], translation[1], translation[2]));
        BRepBuilderAPI_Transform copier(sourceFace, move, true);
        const TopoDS_Face copied = TopoDS::Face(copier.Shape());

        const std::vector<FaceLoop> sourceLoops = collectLoops(sourceFace);
        const std::vector<FaceLoop> copyLoops = collectLoops(copied);
        if (sourceLoops.empty() || sourceLoops.size() != copyLoops.size()) {
            throw OpError("extrudeFace: face boundary could not be walked");
        }
        std::vector<std::vector<TopoDS_Shape>> sourceEdges;
        for (size_t k = 0; k < copyLoops.size(); ++k) {
            if (sourceLoops[k].edges.size() != copyLoops[k].edges.size() ||
                sourceLoops[k].isOuter != copyLoops[k].isOuter) {
                throw OpError("extrudeFace: copied face loops differ from the source");
            }
            sourceEdges.push_back(sourceLoops[k].edges);
        }

        BRepPrimAPI_MakePrism prism(copied, prismVector(direction), false, true);
        const TopoDS_Shape shape = prism.Shape();
        Finish how;
        how.bornIn = bornIn;
        how.roleSuffix = suffix;
        how.previousTable = entry->tableId;
        how.rolesJson = rolesOf(prism, shape, copyLoops, sourceEdges, static_cast<int>(faceIndex),
                                entry->edges);
        return finishSolid(shape, how);
    });
}
