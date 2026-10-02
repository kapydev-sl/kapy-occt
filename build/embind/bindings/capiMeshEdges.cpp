// services/occt/build/embind/bindings/capiMeshEdges.cpp
//
// The two parts of a tessellation that are arithmetic rather than meshing: the
// polyline each edge is drawn and picked with (`sampleEdgePolyline`) and the
// area-weighted vertex normals (`computeVertexNormals`), both of
// binding/tessellate.ts.
//
// The normals reproduce a Float32Array's arithmetic: each component lives as an
// f32, a triangle's contribution is computed in doubles and added in doubles,
// and the sum is rounded to f32 when it is stored back. The length is
// `Math.hypot` as V8 computes it.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: meshing, the C entry points.

#include <algorithm>
#include <cmath>

#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_UniformDeflection.hxx>
#include <GeomAbs_CurveType.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>

#include "capiMeshCore.hxx"

namespace kapy_capi {

namespace {

// Curves are drawn with a dashed material: a chord comparable to the dash would
// read as a polygon, so a circle is sampled to a 0.2 mm chord, at least 48
// points. Other curves are sampled by a 0.05 mm deflection.
constexpr double TARGET_CHORD_MM = 0.2;
constexpr double MIN_CIRCLE_SAMPLES = 48;
constexpr double OTHER_DEFLECTION = 0.05;

void push(std::vector<float>& out, const gp_Pnt& p) {
    out.push_back(static_cast<float>(p.X()));
    out.push_back(static_cast<float>(p.Y()));
    out.push_back(static_cast<float>(p.Z()));
}

// The add of `normals[at] += v` on a Float32Array.
void accumulate(std::vector<float>& normals, size_t at, double v) {
    normals[at] = static_cast<float>(static_cast<double>(normals[at]) + v);
}

}  // namespace

std::vector<float> edgePolyline(const TopoDS_Edge& edge) {
    BRepAdaptor_Curve curve(edge);
    std::vector<float> out;
    const double u0 = curve.FirstParameter();
    const double u1 = curve.LastParameter();
    const GeomAbs_CurveType type = curve.GetType();
    if (type == GeomAbs_Line) {
        gp_Pnt p0, p1;
        curve.D0(u0, p0);
        curve.D0(u1, p1);
        push(out, p0);
        push(out, p1);
    } else if (type == GeomAbs_Circle) {
        const double radius = curve.Circle().Radius();
        const double arcLen = std::fabs(u1 - u0) * radius;
        const double samples = std::max(MIN_CIRCLE_SAMPLES, std::ceil(arcLen / TARGET_CHORD_MM) + 1);
        gp_Pnt p;
        for (int i = 0; i < static_cast<int>(samples); i++) {
            const double u = u0 + ((u1 - u0) * static_cast<double>(i)) / (samples - 1);
            curve.D0(u, p);
            push(out, p);
        }
    } else {
        GCPnts_UniformDeflection disc;
        disc.Initialize(curve, OTHER_DEFLECTION, u0, u1, false);
        if (disc.IsDone()) {
            const int n = disc.NbPoints();
            for (int i = 1; i <= n; i++) push(out, disc.Value(i));
        }
    }
    return out;
}

std::vector<float> vertexNormals(const std::vector<float>& positions,
                                 const std::vector<uint32_t>& indices) {
    std::vector<float> normals(positions.size(), 0.0f);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const size_t a = static_cast<size_t>(indices[i]) * 3;
        const size_t b = static_cast<size_t>(indices[i + 1]) * 3;
        const size_t c = static_cast<size_t>(indices[i + 2]) * 3;
        const double abx = static_cast<double>(positions[b]) - positions[a];
        const double aby = static_cast<double>(positions[b + 1]) - positions[a + 1];
        const double abz = static_cast<double>(positions[b + 2]) - positions[a + 2];
        const double acx = static_cast<double>(positions[c]) - positions[a];
        const double acy = static_cast<double>(positions[c + 1]) - positions[a + 1];
        const double acz = static_cast<double>(positions[c + 2]) - positions[a + 2];
        const double nx = aby * acz - abz * acy;
        const double ny = abz * acx - abx * acz;
        const double nz = abx * acy - aby * acx;
        for (size_t v : {a, b, c}) {
            accumulate(normals, v, nx);
            accumulate(normals, v + 1, ny);
            accumulate(normals, v + 2, nz);
        }
    }
    for (size_t i = 0; i + 2 < normals.size(); i += 3) {
        const double x = normals[i], y = normals[i + 1], z = normals[i + 2];
        const double len = jsHypot3(x, y, z);
        if (len > 0) {
            normals[i] = static_cast<float>(x / len);
            normals[i + 1] = static_cast<float>(y / len);
            normals[i + 2] = static_cast<float>(z / len);
        }
    }
    return normals;
}

}  // namespace kapy_capi
