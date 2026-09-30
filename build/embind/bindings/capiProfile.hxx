// services/occt/build/embind/bindings/capiProfile.hxx
//
// A sketch profile as a C build operation receives it, and the two steps that
// turn it into OCCT: its segments lifted onto the plane as edges
// (`buildLoopEdges`, the port of `binding/builders/buildLoopEdges.ts`) and
// chained into an explored wire (`exploredWire`, `builders/buildWire.ts`).
// Everything that needs a sine, a cosine or a decision about direction was
// settled by the caller and arrives as plain numbers, so nothing here depends
// on the kernel's libm agreeing with the core's.
//
// Who includes it: capiProfile.cpp, capiPrism.cpp, capiLoft.cpp, capiPush.cpp.
// What does NOT belong here: the prism and loft makers, the naming facts.

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>

#include "capiBlob.hxx"

namespace kapy_capi {

// A failure with the message the TypeScript binding threw for the same case,
// so a refused profile reads the same on both transports.
struct OpError : std::runtime_error {
    explicit OpError(const std::string& message) : std::runtime_error(message) {}
};

// The kinds of a segment, numbered as the blob writes them.
enum SegmentKind : uint8_t { kLine = 0, kArc = 1, kCircle = 2, kBSpline = 3, kEllipse = 4 };

// One segment of a profile, in sketch coordinates. `v` holds what the kind
// needs: line `ax ay bx by`; arc `cx cy sx sy ex ey radius sweepSign`; circle
// `cx cy radius`; ellipse `cx cy majorRadius minorRadius cos(rotation)
// sin(rotation)`; a B-spline keeps its points in `points` (x, y pairs).
struct Segment {
    SegmentKind kind = kLine;
    double v[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    std::vector<double> points;
    bool hasSource = false;
    std::string source;
};

// The plane a profile lives on: origin and axes, and the cross product of the
// axes with its length (`nx ny nz nLen`), computed by the caller.
struct Plane {
    double origin[3];
    double x[3];
    double y[3];
    double n[4];
};

// Read a plane, and a segment list (`u32` count, then each segment).
Plane readPlane(Blob& in);
std::vector<Segment> readSegments(Blob& in);

// The edges of `segments` lifted onto `plane` with its origin replaced by
// `origin`, one per segment in order.
std::vector<TopoDS_Edge> buildLoopEdges(const std::vector<Segment>& segments, const Plane& plane,
                                        const double* origin);

// A wire of `edges` and what BRepTools_WireExplorer walks over it: the edges
// and the vertex each starts at, in walking order.
struct Explored {
    TopoDS_Wire wire;
    std::vector<TopoDS_Edge> edges;
    std::vector<TopoDS_Vertex> vertices;
};
Explored exploredWire(const std::vector<TopoDS_Edge>& edges);

// How many edges BRepTools_WireExplorer walks over `wire`; throws the
// binding's error when that is not `expected`.
void verifyWireOrder(const TopoDS_Wire& wire, size_t expected);

// The vertices BRepTools_WireExplorer walks over `wire`, in order.
std::vector<TopoDS_Vertex> wireVertices(const TopoDS_Wire& wire);

}  // namespace kapy_capi
