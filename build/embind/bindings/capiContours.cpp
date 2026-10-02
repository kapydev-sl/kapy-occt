// services/occt/build/embind/bindings/capiContours.cpp
//
// A face's outline as polylines, for the hover outline, the thread's rim and
// the bounded clip of a sketch:
//   polylines  every edge of the face, tessellated on its own, in explorer
//              order, an edge with fewer than two points dropped (binding/
//              sampleCurves.ts, `collectFaceBoundaryPolylines`)
//   wires      the face's wires, each a closed polyline sampled in connection
//              order, split into the outer one and the holes (binding/
//              faceBoundary.ts, `collectFaceBoundaryWires`)
//
// An edge is sampled with GCPnts_UniformDeflection over its own parameter
// range, a chord deflection of 0.5 mm (a circle's scales with its radius,
// between 0.005 and 0.5).
//
// Blob: u32 handle, u32 face index (0-based). A face index outside the shape
// answers an empty outline, as the binding does. Answers: polylines `u32 n`,
// then per polyline `u32 points` and three f64 per point; wires `u32 holes`,
// the outer polyline, then each hole, a polyline being `u32 points` and three
// f64 per point (an outer of zero points is "none").
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the store, the arena, the cylinder's probes.

#include <algorithm>
#include <limits>
#include <vector>

#include <BRepAdaptor_Curve.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <GCPnts_UniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <ShapeAnalysis.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>

#include "capiAsk.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('getFaceBoundaryPolylines'), ('getFaceBoundaryWires').
constexpr size_t SEED_POLYLINES = 938468884u;
constexpr size_t SEED_WIRES = 1307835559u;

using Polyline = std::vector<gp_Pnt>;

// One edge as a polyline: empty when the discretizer does not finish.
Polyline sampleEdge(const TopoDS_Edge& edge) {
    BRepAdaptor_Curve curve(edge);
    double deflection = 0.5;
    if (curve.GetType() == GeomAbs_Circle) {
        deflection = std::min(0.5, std::max(0.005, curve.Circle().Radius() * 0.005));
    }
    GCPnts_UniformDeflection discretizer;
    discretizer.Initialize(curve, deflection, curve.FirstParameter(), curve.LastParameter(), false);
    Polyline out;
    if (!discretizer.IsDone()) return out;
    for (int i = 1; i <= discretizer.NbPoints(); i++) out.push_back(discretizer.Value(i));
    return out;
}

// One wire as a closed polyline (the repeat of the start dropped), its edges
// in connection order and each read the way the wire walks it.
Polyline sampleWire(const TopoDS_Wire& wire) {
    BRepTools_WireExplorer explorer;
    explorer.Init(wire);
    Polyline line;
    bool first = true;
    for (; explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = explorer.Current();
        Polyline points = sampleEdge(edge);
        if (points.empty()) continue;
        if (edge.Orientation() == TopAbs_REVERSED) std::reverse(points.begin(), points.end());
        const size_t from = first ? 0 : 1;
        line.insert(line.end(), points.begin() + from, points.end());
        first = false;
    }
    if (line.size() > 1) {
        const gp_Pnt& a = line.front();
        const gp_Pnt& b = line.back();
        const double dx = a.X() - b.X();
        const double dy = a.Y() - b.Y();
        const double dz = a.Z() - b.Z();
        if (dx * dx + dy * dy + dz * dz < 1e-12) line.pop_back();
    }
    return line;
}

void put(Out& out, const Polyline& line) {
    out.u32(static_cast<uint32_t>(line.size()));
    for (const gp_Pnt& p : line) {
        out.f64(p.X());
        out.f64(p.Y());
        out.f64(p.Z());
    }
}

// The area of a polyline's box on x and y: the stand-in for "which is the
// outer wire" when the kernel cannot say.
double boxArea(const Polyline& line) {
    if (line.empty()) return 0.0;
    double lox = std::numeric_limits<double>::infinity();
    double loy = lox;
    double hix = -lox;
    double hiy = -lox;
    for (const gp_Pnt& p : line) {
        lox = std::min(lox, p.X());
        hix = std::max(hix, p.X());
        loy = std::min(loy, p.Y());
        hiy = std::max(hiy, p.Y());
    }
    return std::max(0.0, hix - lox) * std::max(0.0, hiy - loy);
}

// The face at `index`, or none when the index is outside the shape.
bool faceAt(Entry& entry, uint32_t index, TopoDS_Face& face) {
    ensureMaps(entry);
    if (index >= static_cast<uint32_t>(entry.faces.Extent())) return false;
    face = TopoDS::Face(entry.faces.FindKey(static_cast<int>(index) + 1));
    return true;
}

void polylines(const TopoDS_Face& face, Out& out) {
    std::vector<Polyline> lines;
    for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
        Polyline line = sampleEdge(TopoDS::Edge(ex.Current()));
        if (line.size() >= 2) lines.push_back(std::move(line));
    }
    out.u32(static_cast<uint32_t>(lines.size()));
    for (const Polyline& line : lines) put(out, line);
}

void wires(const TopoDS_Face& face, Out& out) {
    TopoDS_Wire outerWire;
    bool hasOuter = false;
    try {
        outerWire = ShapeAnalysis::OuterWire(face);
        hasOuter = true;
    } catch (...) {
        hasOuter = false;
    }
    Polyline outer;
    std::vector<Polyline> inner;
    for (TopExp_Explorer ex(face, TopAbs_WIRE); ex.More(); ex.Next()) {
        const TopoDS_Wire wire = TopoDS::Wire(ex.Current());
        Polyline line = sampleWire(wire);
        if (line.size() < 3) continue;
        if (hasOuter && wire.IsSame(outerWire)) {
            outer = std::move(line);
        } else {
            inner.push_back(std::move(line));
        }
    }
    if (outer.empty() && !inner.empty()) {
        size_t best = 0;
        double bestArea = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < inner.size(); i++) {
            const double area = boxArea(inner[i]);
            if (area > bestArea) {
                bestArea = area;
                best = i;
            }
        }
        outer = std::move(inner[best]);
        inner.erase(inner.begin() + best);
    }
    out.u32(static_cast<uint32_t>(inner.size()));
    put(out, outer);
    for (const Polyline& line : inner) put(out, line);
}
}  // namespace

KAPY_API int32_t kapy_face_polylines(uint32_t ptr, uint32_t length) noexcept {
    return ask("getFaceBoundaryPolylines", SEED_POLYLINES, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        const uint32_t index = in.u32();
        TopoDS_Face face;
        if (faceAt(entry, index, face)) polylines(face, out);
        else out.u32(0);
    });
}

KAPY_API int32_t kapy_face_wires(uint32_t ptr, uint32_t length) noexcept {
    return ask("getFaceBoundaryWires", SEED_WIRES, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        const uint32_t index = in.u32();
        TopoDS_Face face;
        if (faceAt(entry, index, face)) wires(face, out);
        else {
            out.u32(0);
            out.u32(0);
        }
    });
}
