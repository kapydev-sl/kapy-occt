// engine/kernels/occt/build/embind/bindings/capiFaceTriangle.cpp
//
// The normal of a free-form face where a fillet decision needs it: the cross
// product of the triangle nearest a point, on a fine triangulation of that face
// alone (0.02 mm linear, 0.03 rad angular), outward for the face's orientation.
// The fold at an edge between two curved faces is read from these.
//
// The cross product travels RAW with the face's orientation flag; the Rust core
// removes the component along the edge and normalises it with `Math.hypot`'s
// algorithm, as the binding did.
//
// Blob: u32 handle, u32 face index, three f64 (the point). Answers a byte (1
// when a triangle was found), then three f64 and a byte (1 when the face is
// REVERSED). No triangulation, or no non-degenerate triangle, answers a 0 byte.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: which faces are asked, what the fold means.

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include "capiAsk.hxx"
#include "capiMath.hxx"

using namespace kapy_capi;

namespace {

// serialSeedOf('faceTriangle').
constexpr size_t SEED_TRIANGLE = 3106347533u;

constexpr double FINE_LIN_DEFLECTION = 0.02;
constexpr double FINE_ANG_DEFLECTION = 0.03;
// A cross product shorter than this is a degenerate triangle.
constexpr double DEGENERATE_NORMAL = 1e-9;

}  // namespace

KAPY_API int32_t kapy_face_triangle(uint32_t ptr, uint32_t length) noexcept {
    return ask("faceTriangle", SEED_TRIANGLE, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        ensureMaps(entry);
        const uint32_t index = in.u32();
        const double point[3] = {in.f64(), in.f64(), in.f64()};
        if (index >= static_cast<uint32_t>(entry.faces.Extent())) {
            out.u8(0);
            return;
        }
        try {
            const TopoDS_Shape& key = entry.faces.FindKey(static_cast<int>(index) + 1);
            const TopoDS_Face face = TopoDS::Face(key);
            TopLoc_Location loc;
            BRepMesh_IncrementalMesh mesher(face, FINE_LIN_DEFLECTION, false, FINE_ANG_DEFLECTION,
                                            false);
            const occ::handle<Poly_Triangulation> mesh =
                BRep_Tool::Triangulation(face, loc, Poly_MeshPurpose_NONE);
            if (mesh.IsNull()) {
                out.u8(0);
                return;
            }
            const gp_Trsf trsf = loc.Transformation();
            bool found = false;
            double bestDistance = INFINITY;
            double best[3] = {0.0, 0.0, 0.0};
            for (int t = 1; t <= mesh->NbTriangles(); ++t) {
                const Poly_Triangle tri = mesh->Triangle(t);
                const gp_Pnt a = mesh->Node(tri.Value(1)).Transformed(trsf);
                const gp_Pnt b = mesh->Node(tri.Value(2)).Transformed(trsf);
                const gp_Pnt c = mesh->Node(tri.Value(3)).Transformed(trsf);
                const double cx = (a.X() + b.X() + c.X()) / 3 - point[0];
                const double cy = (a.Y() + b.Y() + c.Y()) / 3 - point[1];
                const double cz = (a.Z() + b.Z() + c.Z()) / 3 - point[2];
                const double distance = cx * cx + cy * cy + cz * cz;
                if (distance >= bestDistance) continue;
                const double u[3] = {b.X() - a.X(), b.Y() - a.Y(), b.Z() - a.Z()};
                const double v[3] = {c.X() - a.X(), c.Y() - a.Y(), c.Z() - a.Z()};
                const double cross[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                                         u[0] * v[1] - u[1] * v[0]};
                if (jsHypot(cross, 3) < DEGENERATE_NORMAL) continue;
                bestDistance = distance;
                found = true;
                for (int k = 0; k < 3; ++k) best[k] = cross[k];
            }
            if (!found) {
                out.u8(0);
                return;
            }
            out.u8(1);
            for (int k = 0; k < 3; ++k) out.f64(best[k]);
            out.u8(key.Orientation() == TopAbs_REVERSED ? 1 : 0);
        } catch (...) {
            out.u8(0);
        }
    });
}
