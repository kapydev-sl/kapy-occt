// engine/kernels/occt/build/embind/bindings/capiEdgeProbe.cpp
//
// The measures of named edges a fillet or chamfer decision reads, in block (one
// crossing however many edges, which is the cost the block exists to avoid):
//   0  the tangent of an edge at its parametric midpoint
//   1  the tangent of an edge at one of its vertices
//   2  the minimum distance between two edges
//   3  the axis direction of a circular edge (no value for any other edge)
// The tangent travels RAW, as OCCT's derivative is: normalising it uses
// `Math.hypot`, which the Rust core does with the same algorithm as the
// binding's JavaScript so the unit vector is the same number.
//
// An index outside the maps, a derivative OCCT cannot evaluate and a distance
// that does not converge each answer "no value" for that one item: the recipe
// reads them as unknown, never as a failure of the call.
//
// Blob: u32 handle, u32 n, then n x (u8 kind, u32 a, u32 b) (`a` is the edge,
// `b` the vertex of kind 1 or the second edge of kind 2, unused for kind 0).
// Answers per item a byte (1 when there is a value) and then three f64 (kinds 0,
// 1 and 3) or one f64 (kind 2).
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: what a tangent or a distance is used for.

#include <BRepAdaptor_Curve.hxx>
#include <GeomAbs_CurveType.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <Message_ProgressRange.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include "capiAsk.hxx"

using namespace kapy_capi;

namespace {

// serialSeedOf('edgeProbe').
constexpr size_t SEED_PROBE = 1106880223u;

enum Kind : uint8_t { kTangentAtMid = 0, kTangentAtVertex = 1, kDistance = 2, kCircleAxis = 3 };

// The raw tangent of `edge` at parameter `t`.
void tangent(const TopoDS_Edge& edge, double t, Out& out) {
    BRepAdaptor_Curve curve(edge);
    gp_Pnt point;
    gp_Vec derivative;
    curve.D1(t, point, derivative);
    out.u8(1);
    out.f64(derivative.X());
    out.f64(derivative.Y());
    out.f64(derivative.Z());
}

// One item, answered; a failure inside OCCT is "no value" for this item only.
void measure(Entry& entry, uint8_t kind, uint32_t a, uint32_t b, Out& out) {
    const uint32_t edges = static_cast<uint32_t>(entry.edges.Extent());
    const uint32_t vertices = static_cast<uint32_t>(entry.vertices.Extent());
    const uint32_t bounds = kind == kTangentAtVertex ? vertices : edges;
    const bool inRange = a < edges && (kind == kTangentAtMid || b < bounds);
    if (!inRange || kind > kCircleAxis) {
        out.u8(0);
        return;
    }
    try {
        const TopoDS_Edge edge = TopoDS::Edge(entry.edges.FindKey(static_cast<int>(a) + 1));
        if (kind == kTangentAtMid) {
            BRepAdaptor_Curve curve(edge);
            tangent(edge, (curve.FirstParameter() + curve.LastParameter()) / 2, out);
        } else if (kind == kCircleAxis) {
            BRepAdaptor_Curve curve(edge);
            if (curve.GetType() != GeomAbs_Circle) {
                out.u8(0);
                return;
            }
            const gp_Dir direction = curve.Circle().Axis().Direction();
            out.u8(1);
            out.f64(direction.X());
            out.f64(direction.Y());
            out.f64(direction.Z());
        } else if (kind == kTangentAtVertex) {
            const TopoDS_Vertex vertex =
                TopoDS::Vertex(entry.vertices.FindKey(static_cast<int>(b) + 1));
            tangent(edge, BRep_Tool::Parameter(vertex, edge), out);
        } else {
            BRepExtrema_DistShapeShape distance;
            distance.LoadS1(entry.edges.FindKey(static_cast<int>(a) + 1));
            distance.LoadS2(entry.edges.FindKey(static_cast<int>(b) + 1));
            distance.Perform(Message_ProgressRange());
            if (!distance.IsDone()) {
                out.u8(0);
                return;
            }
            out.u8(1);
            out.f64(distance.Value());
        }
    } catch (...) {
        out.u8(0);
    }
}

}  // namespace

KAPY_API int32_t kapy_edge_probe(uint32_t ptr, uint32_t length) noexcept {
    return ask("edgeProbe", SEED_PROBE, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        ensureMaps(entry);
        const uint32_t count = in.u32();
        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t kind = in.u8();
            const uint32_t a = in.u32();
            const uint32_t b = in.u32();
            measure(entry, kind, a, b, out);
        }
    });
}
