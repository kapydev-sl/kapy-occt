// engine/kernels/occt/build/embind/bindings/capiShellFacts.cpp
//
// What the shell recipe reads about a body around its attempts, as two
// questions that measure and decide nothing (which of these explains a failure,
// at which threshold, and which thinner wall to offer is the Rust recipe's):
//
//   shell facts   per solid the picked faces open (the order of `shellPieces`),
//                 the numbers a diagnosis weighs: the smallest convex cylinder
//                 that is not removed (`round`) and the smallest of those that
//                 goes all the way round (`shaft`), the volume and the volume of
//                 the box, the smallest face area, the shortest edge of positive
//                 length and whether an opening face is curved
//   thick probe   whether a wall of a given thickness builds on one solid
//                 through the given rungs, the result discarded
//
// Blob: u32 handle, u32 n, n x u32 face index. The probe adds f64 thickness,
// u32 rungs, rungs x (f64 tol, u8 join) and u32 solid index. Answers: the facts
// `u32 solids`, then per solid a byte (0 when nothing is removed from it) and
// otherwise, in order: shaft, round (each a byte 1 and an f64, or a byte 0),
// f64 volume, box (byte and f64), min face area (byte and f64), min edge length
// (byte and f64), and a byte for a curved opening; the probe one byte.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: any threshold, any retry, the build itself.

#include <algorithm>
#include <cmath>
#include <vector>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax1.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Pnt.hxx>

#include "capiAsk.hxx"
#include "capiClassify.hxx"
#include "capiGuards.hxx"
#include "capiMath.hxx"
#include "capiShell.hxx"

using namespace kapy_capi;

namespace {

// serialSeedOf('shellFacts') and of ('thickSolidProbe').
constexpr size_t SEED_FACTS = 543229364u;
constexpr size_t SEED_PROBE = 1488457042u;

constexpr double PI = 3.14159265358979323846;

// A cylindrical face: its radius, whether the material lies inside it (a shaft,
// a rounded outer edge) and whether it covers (nearly) a full turn.
struct CylinderFace {
    double radius;
    bool convex;
    bool fullTurn;
};

// `face` read as a cylinder, or none. The side the material is on comes from
// classifying a point just inside the surface (toward the axis, halfway along
// it); the turn it covers from its area against radius x axial height.
bool cylinderFace(const TopoDS_Shape& shape, const TopoDS_Face& face, CylinderFace& found) {
    try {
        BRepAdaptor_Surface adaptor(face, false);
        if (adaptor.GetType() != GeomAbs_Cylinder) return false;
        const gp_Cylinder cylinder = adaptor.Cylinder();
        const double radius = cylinder.Radius();
        const gp_Ax1 axis = cylinder.Axis();
        const double o[3] = {axis.Location().X(), axis.Location().Y(), axis.Location().Z()};
        const double a[3] = {axis.Direction().X(), axis.Direction().Y(), axis.Direction().Z()};
        TopTools_IndexedMapOfShape verts;
        TopExp::MapShapes(face, TopAbs_VERTEX, verts);
        double tMin = INFINITY;
        double tMax = -INFINITY;
        bool haveRadial = false;
        double radial[3] = {0.0, 0.0, 0.0};
        for (int i = 1; i <= verts.Extent(); ++i) {
            const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(verts.FindKey(i)));
            const double v[3] = {p.X() - o[0], p.Y() - o[1], p.Z() - o[2]};
            const double t = v[0] * a[0] + v[1] * a[1] + v[2] * a[2];
            tMin = std::min(tMin, t);
            tMax = std::max(tMax, t);
            const double r[3] = {v[0] - t * a[0], v[1] - t * a[1], v[2] - t * a[2]};
            const double rl = jsHypot(r, 3);
            if (!haveRadial && rl > 1e-9) {
                haveRadial = true;
                for (int k = 0; k < 3; ++k) radial[k] = r[k] / rl;
            }
        }
        const double height = tMax - tMin;
        if (!haveRadial || !(height > 0)) return false;
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props, false, false);
        const double area = props.Mass();
        const double tMid = (tMin + tMax) / 2;
        const double inset = radius - std::min(0.01, radius / 2);
        double probe[3];
        for (int k = 0; k < 3; ++k) probe[k] = o[k] + a[k] * tMid + radial[k] * inset;
        found.radius = radius;
        found.convex = probePoint(shape, probe[0], probe[1], probe[2]).state == STATE_IN;
        found.fullTurn = area / (radius * height) > 2 * PI * 0.99;
        return true;
    } catch (...) {
        return false;
    }
}

// An optional number: a byte for whether there is one, then the f64.
void optional(Out& out, bool present, double value) {
    out.u8(present ? 1 : 0);
    if (present) out.f64(value);
}

bool isRemoved(const std::vector<TopoDS_Shape>& removed, const TopoDS_Shape& face) {
    return std::any_of(removed.begin(), removed.end(),
                       [&](const TopoDS_Shape& r) { return r.IsSame(face); });
}

// The smallest convex cylindrical face not being removed, and the smallest of
// those that goes all the way round.
void convexRounds(const ShellPiece& piece, bool& haveRound, double& round, bool& haveShaft,
                  double& shaft) {
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(piece.shape, TopAbs_FACE, faces);
    for (int i = 1; i <= faces.Extent(); ++i) {
        const TopoDS_Shape& face = faces.FindKey(i);
        if (isRemoved(piece.removed, face)) continue;
        CylinderFace cyl;
        if (!cylinderFace(piece.shape, TopoDS::Face(face), cyl) || !cyl.convex) continue;
        if (!haveRound || cyl.radius < round) {
            haveRound = true;
            round = cyl.radius;
        }
        if (cyl.fullTurn && (!haveShaft || cyl.radius < shaft)) {
            haveShaft = true;
            shaft = cyl.radius;
        }
    }
}

// The solid's facts, written in the layout above.
void solidFacts(const ShellPiece& piece, Out& out) {
    bool haveRound = false;
    bool haveShaft = false;
    double round = 0.0;
    double shaft = 0.0;
    convexRounds(piece, haveRound, round, haveShaft, shaft);
    optional(out, haveShaft, shaft);
    optional(out, haveRound, round);
    out.f64(signedVolume(piece.shape));

    Bnd_Box box;
    BRepBndLib::Add(piece.shape, box, false);
    const bool haveBox = !box.IsVoid();
    double volume = 0.0;
    if (haveBox) {
        volume = (box.CornerMax().X() - box.CornerMin().X()) *
                 (box.CornerMax().Y() - box.CornerMin().Y()) *
                 (box.CornerMax().Z() - box.CornerMin().Z());
    }
    optional(out, haveBox, volume);

    // The smallest face area, and the shortest edge of positive length (a zero
    // one is a degenerated edge, a cone apex or a sphere pole, not a sliver).
    GProp_GProps props;
    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(piece.shape, TopAbs_FACE, faces);
    TopExp::MapShapes(piece.shape, TopAbs_EDGE, edges);
    bool haveFace = false;
    bool haveEdge = false;
    double minFace = 0.0;
    double minEdge = 0.0;
    for (int i = 1; i <= faces.Extent(); ++i) {
        BRepGProp::SurfaceProperties(faces.FindKey(i), props, false, false);
        const double area = props.Mass();
        if (!haveFace || area < minFace) {
            haveFace = true;
            minFace = area;
        }
    }
    for (int i = 1; i <= edges.Extent(); ++i) {
        BRepGProp::LinearProperties(edges.FindKey(i), props, false, false);
        const double length = props.Mass();
        if (length > 0 && (!haveEdge || length < minEdge)) {
            haveEdge = true;
            minEdge = length;
        }
    }
    optional(out, haveFace, minFace);
    optional(out, haveEdge, minEdge);

    bool curved = false;
    for (const TopoDS_Shape& face : piece.removed) {
        BRepAdaptor_Surface adaptor(TopoDS::Face(face), false);
        if (adaptor.GetType() != GeomAbs_Plane) curved = true;
    }
    out.u8(curved ? 1 : 0);
}

// The previous body and the picked faces every shell question starts with.
Entry& readOpenings(Blob& in, std::vector<uint32_t>& indices) {
    Entry& prev = need(in.u32());
    ensureMaps(prev);
    const uint32_t count = in.u32();
    for (uint32_t i = 0; i < count; ++i) indices.push_back(in.u32());
    checkShellFaces(prev, indices);
    return prev;
}

}  // namespace

KAPY_API int32_t kapy_shell_facts(uint32_t ptr, uint32_t length) noexcept {
    return ask("shellFacts", SEED_FACTS, ptr, length, [](Blob& in, Out& out) {
        std::vector<uint32_t> indices;
        Entry& prev = readOpenings(in, indices);
        const ShellPieces split = shellPieces(prev, indices);
        out.u32(static_cast<uint32_t>(split.pieces.size()));
        for (const ShellPiece& piece : split.pieces) {
            out.u8(piece.removed.empty() ? 0 : 1);
            if (!piece.removed.empty()) solidFacts(piece, out);
        }
    });
}

KAPY_API int32_t kapy_thick_probe(uint32_t ptr, uint32_t length) noexcept {
    return ask("thickSolidProbe", SEED_PROBE, ptr, length, [](Blob& in, Out& out) {
        std::vector<uint32_t> indices;
        Entry& prev = readOpenings(in, indices);
        const double thickness = in.f64();
        std::vector<Rung> rungs;
        const uint32_t nRungs = in.u32();
        for (uint32_t i = 0; i < nRungs; ++i) {
            const double tol = in.f64();
            rungs.push_back({tol, in.u8() != 0});
        }
        const uint32_t solid = in.u32();
        if (!in.done()) throw BlobError();
        const ShellPieces split = shellPieces(prev, indices);
        bool builds = false;
        if (solid < split.pieces.size() && !split.pieces[solid].removed.empty()) {
            const ShellPiece& piece = split.pieces[solid];
            const ThickResult made = makeThickSolid(piece.shape, piece.closing, thickness, rungs,
                                                    signedVolume(piece.shape));
            builds = static_cast<bool>(made.op);
        }
        out.u8(builds ? 1 : 0);
    });
}
