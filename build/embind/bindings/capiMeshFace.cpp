// engine/kernels/occt/build/embind/bindings/capiMeshFace.cpp
//
// One face's triangles, read and checked, and a whole body meshed from them:
// `readFaceTriangles`, `checkedPlanarTriangles` and the face loop of
// binding/tessellate.ts, call for call. BRepMesh can come back "done" with a
// planar face triangulated as if its holes were not there, so a planar face's
// triangles are measured against its exact area and, when they disagree, the
// face is re-meshed on its own with other settings until they do.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: edges and normals (capiMeshEdges.cpp), the C
// entry points (capiMesh.cpp).

#include <algorithm>
#include <cmath>

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Poly_MeshPurpose.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include "capiMeshCore.hxx"

namespace kapy_capi {

namespace {

// A planar face's triangles may undershoot its area by the chord error along
// curved edges; a hole meshed shut overshoots by the hole.
constexpr double AREA_REL_TOL = 0.03;
constexpr double AREA_ABS_TOL_MM2 = 0.5;

// Re-mesh settings tried in order when the first triangulation fails the area
// check. Relative deflection first: it is the one that meshed the production
// face right.
struct Retry {
    double lin;
    bool relative;
    double ang;
};
constexpr Retry RETRIES[] = {
    {0.001, true, 0.25},
    {0.01, false, 0.1},
    {0.005, true, 0.25},
    {0.2, false, 0.5},
};

bool matches(double meshed, double exact) {
    return std::fabs(meshed - exact) <= std::max(AREA_REL_TOL * exact, AREA_ABS_TOL_MM2);
}

double faceArea(const TopoDS_Shape& face) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props, false, false);
    return props.Mass();
}

double trianglesArea(const FaceTriangles& f) {
    const std::vector<double>& p = f.positions;
    double area = 0;
    for (size_t i = 0; i + 2 < f.triangles.size(); i += 3) {
        const size_t a = 3 * static_cast<size_t>(f.triangles[i]);
        const size_t b = 3 * static_cast<size_t>(f.triangles[i + 1]);
        const size_t c = 3 * static_cast<size_t>(f.triangles[i + 2]);
        const double ux = p[b] - p[a], uy = p[b + 1] - p[a + 1], uz = p[b + 2] - p[a + 2];
        const double vx = p[c] - p[a], vy = p[c + 1] - p[a + 1], vz = p[c + 2] - p[a + 2];
        area += jsHypot3(uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx) / 2;
    }
    return area;
}

// The faces of `entry`, appended to `out` in map order.
void readFaces(Entry& entry, const std::vector<uint8_t>& planar, BodyMesh& out) {
    const int faceCount = entry.faces.Extent();
    for (int fi = 1; fi <= faceCount; fi++) {
        const TopoDS_Shape& faceShape = entry.faces.FindKey(fi);
        FaceTriangles read;
        if (!readFaceTriangles(faceShape, read)) continue;
        const bool isPlanar = static_cast<size_t>(fi - 1) < planar.size() && planar[fi - 1] != 0;
        const FaceTriangles face = isPlanar ? checkedPlanarTriangles(faceShape, read) : read;
        const bool reversed = faceShape.Orientation() == TopAbs_REVERSED;
        const uint32_t base = static_cast<uint32_t>(out.positions.size() / 3);
        for (double v : face.positions) out.positions.push_back(static_cast<float>(v));
        const std::vector<int>& t = face.triangles;
        for (size_t i = 0; i + 2 < t.size(); i += 3) {
            const uint32_t n1 = static_cast<uint32_t>(t[i]) + base;
            const uint32_t n2 = static_cast<uint32_t>(t[i + 1]) + base;
            const uint32_t n3 = static_cast<uint32_t>(t[i + 2]) + base;
            out.indices.push_back(n1);
            out.indices.push_back(reversed ? n3 : n2);
            out.indices.push_back(reversed ? n2 : n3);
            out.triFace.push_back(static_cast<uint32_t>(fi - 1));
        }
    }
}

}  // namespace

double jsHypot3(double x, double y, double z) {
    const double a[3] = {std::fabs(x), std::fabs(y), std::fabs(z)};
    double max = 0;
    bool nan = false;
    for (double v : a) {
        if (std::isnan(v)) {
            nan = true;
        } else if (v > max) {
            max = v;
        }
    }
    if (std::isinf(max)) return max;
    if (nan) return std::nan("");
    if (max == 0) return 0;
    double sum = 0, compensation = 0;
    for (double v : a) {
        const double n = v / max;
        const double summand = n * n - compensation;
        const double preliminary = sum + summand;
        compensation = (preliminary - sum) - summand;
        sum = preliminary;
    }
    return std::sqrt(sum) * max;
}

bool readFaceTriangles(const TopoDS_Shape& faceShape, FaceTriangles& out) {
    const TopoDS_Face& face = TopoDS::Face(faceShape);
    TopLoc_Location loc;
    const occ::handle<Poly_Triangulation> tri = BRep_Tool::Triangulation(face, loc, Poly_MeshPurpose_NONE);
    if (tri.IsNull()) return false;
    const gp_Trsf trsf = loc.Transformation();
    out.positions.clear();
    out.triangles.clear();
    for (int i = 1; i <= tri->NbNodes(); i++) {
        const gp_Pnt p = tri->Node(i).Transformed(trsf);
        out.positions.push_back(p.X());
        out.positions.push_back(p.Y());
        out.positions.push_back(p.Z());
    }
    for (int i = 1; i <= tri->NbTriangles(); i++) {
        const Poly_Triangle t = tri->Triangle(i);
        out.triangles.push_back(t.Value(1) - 1);
        out.triangles.push_back(t.Value(2) - 1);
        out.triangles.push_back(t.Value(3) - 1);
    }
    return true;
}

FaceTriangles checkedPlanarTriangles(const TopoDS_Shape& faceShape, const FaceTriangles& first) {
    const double target = faceArea(faceShape);
    if (matches(trianglesArea(first), target)) return first;
    for (const Retry& s : RETRIES) {
        BRepBuilderAPI_Copy copier(faceShape, true, false);
        const TopoDS_Shape copy = copier.Shape();
        {
            BRepMesh_IncrementalMesh mesher(copy, s.lin, s.relative, s.ang, false);
        }
        FaceTriangles retry;
        if (readFaceTriangles(copy, retry) && matches(trianglesArea(retry), target)) return retry;
    }
    return first;
}

void meshBody(Entry& entry, double linear, double angular, const std::vector<uint8_t>& planar,
              BodyMesh& out) {
    ensureMaps(entry);
    {
        // The mesher goes out of scope before the edges are sampled, as the
        // binding deletes it.
        BRepMesh_IncrementalMesh mesher(entry.shape, linear, false, angular, false);
        readFaces(entry, planar, out);
    }
    const int edgeCount = entry.edges.Extent();
    for (int ei = 1; ei <= edgeCount; ei++) {
        out.edges.push_back(edgePolyline(TopoDS::Edge(entry.edges.FindKey(ei))));
    }
}

}  // namespace kapy_capi
