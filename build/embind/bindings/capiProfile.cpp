// services/occt/build/embind/bindings/capiProfile.cpp
//
// Profiles into OCCT, call for call as the TypeScript builders make them
// (`buildLoopEdges.ts`, `buildWire.ts`, `extrudeProfile.helpers.ts`): the same
// constructors in the same order, because OCCT numbers what it creates and the
// kernel's hashing follows that numbering.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: prisms, lofts, the naming facts.

#include "capiProfile.hxx"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Curve.hxx>
#include <GeomAbs_Shape.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Elips.hxx>
#include <gp_Pnt.hxx>

namespace kapy_capi {

Plane readPlane(Blob& in) {
    Plane p;
    for (double& v : p.origin) v = in.f64();
    for (double& v : p.x) v = in.f64();
    for (double& v : p.y) v = in.f64();
    for (double& v : p.n) v = in.f64();
    return p;
}

namespace {
// How many numbers each fixed-size kind carries in `v`.
size_t countOf(SegmentKind kind) {
    switch (kind) {
        case kLine: return 4;
        case kArc: return 8;
        case kCircle: return 3;
        case kEllipse: return 6;
        default: return 0;
    }
}
}  // namespace

std::vector<Segment> readSegments(Blob& in) {
    const uint32_t n = in.u32();
    std::vector<Segment> out;
    for (uint32_t i = 0; i < n; ++i) {
        Segment s;
        const uint8_t kind = in.u8();
        if (kind > kEllipse) throw BlobError();
        s.kind = static_cast<SegmentKind>(kind);
        if (s.kind == kBSpline) {
            const uint32_t count = in.u32();
            for (uint32_t k = 0; k < count * 2; ++k) s.points.push_back(in.f64());
        } else {
            for (size_t k = 0; k < countOf(s.kind); ++k) s.v[k] = in.f64();
        }
        s.hasSource = in.u8() != 0;
        if (s.hasSource) s.source = in.str();
        out.push_back(std::move(s));
    }
    return out;
}

std::vector<TopoDS_Edge> buildLoopEdges(const std::vector<Segment>& segments, const Plane& plane,
                                        const double* origin) {
    const double* xa = plane.x;
    const double* ya = plane.y;
    const auto lift = [&](double x, double y) {
        return gp_Pnt(origin[0] + x * xa[0] + y * ya[0], origin[1] + x * xa[1] + y * ya[1],
                      origin[2] + x * xa[2] + y * ya[2]);
    };
    const double nx = plane.n[0], ny = plane.n[1], nz = plane.n[2], nLen = plane.n[3];
    const auto makeAx2 = [&](double cx, double cy, double sign) {
        const gp_Pnt c = lift(cx, cy);
        const gp_Dir normal((nx / nLen) * sign, (ny / nLen) * sign, (nz / nLen) * sign);
        const gp_Dir xd(xa[0], xa[1], xa[2]);
        return gp_Ax2(c, normal, xd);
    };

    std::vector<TopoDS_Edge> edges;
    for (const Segment& s : segments) {
        const double* v = s.v;
        switch (s.kind) {
            case kLine: {
                const gp_Pnt a = lift(v[0], v[1]);
                const gp_Pnt b = lift(v[2], v[3]);
                BRepBuilderAPI_MakeEdge em(a, b);
                edges.push_back(em.Edge());
                break;
            }
            case kArc: {
                const gp_Ax2 ax2 = makeAx2(v[0], v[1], v[7]);
                const gp_Circ circ(ax2, v[6]);
                const gp_Pnt start = lift(v[2], v[3]);
                const gp_Pnt end = lift(v[4], v[5]);
                BRepBuilderAPI_MakeEdge em(circ, start, end);
                edges.push_back(em.Edge());
                break;
            }
            case kBSpline: {
                const Standard_Integer n = static_cast<Standard_Integer>(s.points.size() / 2);
                TColgp_Array1OfPnt arr(1, n);
                for (Standard_Integer i = 0; i < n; ++i) {
                    arr.SetValue(i + 1, lift(s.points[2 * i], s.points[2 * i + 1]));
                }
                GeomAPI_PointsToBSpline fit(arr, 3, 8, GeomAbs_C2, 1e-4);
                Handle(Geom_BSplineCurve) bspline = fit.Curve();
                Handle(Geom_Curve) curve = bspline;
                BRepBuilderAPI_MakeEdge em(curve);
                edges.push_back(em.Edge());
                break;
            }
            case kEllipse: {
                const gp_Pnt c = lift(v[0], v[1]);
                const double cos = v[4], sin = v[5];
                const double mx = cos * xa[0] + sin * ya[0];
                const double my = cos * xa[1] + sin * ya[1];
                const double mz = cos * xa[2] + sin * ya[2];
                const gp_Dir normal(nx / nLen, ny / nLen, nz / nLen);
                const gp_Dir major(mx, my, mz);
                const gp_Ax2 ax2(c, normal, major);
                const gp_Elips elips(ax2, v[2], v[3]);
                BRepBuilderAPI_MakeEdge em(elips);
                edges.push_back(em.Edge());
                break;
            }
            case kCircle: {
                const gp_Ax2 ax2 = makeAx2(v[0], v[1], 1.0);
                const gp_Circ circ(ax2, v[2]);
                BRepBuilderAPI_MakeEdge em(circ);
                edges.push_back(em.Edge());
                break;
            }
        }
    }
    return edges;
}

namespace {
[[noreturn]] void wireMismatch(size_t walked, size_t built) {
    throw OpError("Wire has " + std::to_string(walked) + " edges but we built " +
                  std::to_string(built) +
                  " \xE2\x80\x94 BRepLib_MakeWire dropped or duplicated an edge.");
}
}  // namespace

Explored exploredWire(const std::vector<TopoDS_Edge>& edges) {
    BRepBuilderAPI_MakeWire wm;
    for (const TopoDS_Edge& e : edges) wm.Add(e);
    Explored out;
    out.wire = wm.Wire();
    BRepTools_WireExplorer explorer;
    explorer.Init(out.wire);
    while (explorer.More()) {
        out.edges.push_back(explorer.Current());
        out.vertices.push_back(explorer.CurrentVertex());
        explorer.Next();
    }
    if (out.edges.size() != edges.size()) wireMismatch(out.edges.size(), edges.size());
    return out;
}

void verifyWireOrder(const TopoDS_Wire& wire, size_t expected) {
    size_t count = 0;
    BRepTools_WireExplorer explorer;
    explorer.Init(wire);
    while (explorer.More()) {
        ++count;
        explorer.Next();
    }
    if (count != expected) wireMismatch(count, expected);
}

std::vector<TopoDS_Vertex> wireVertices(const TopoDS_Wire& wire) {
    std::vector<TopoDS_Vertex> out;
    BRepTools_WireExplorer explorer;
    explorer.Init(wire);
    while (explorer.More()) {
        out.push_back(explorer.CurrentVertex());
        explorer.Next();
    }
    return out;
}

}  // namespace kapy_capi
