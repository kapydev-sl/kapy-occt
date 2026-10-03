// engine/kernels/occt/build/embind/bindings/capiFace.cpp
//
// What the kernel alone knows about one face, for `getFaceGeometry`:
//   surface   what kind of surface the face lies on and, for a cylinder, its
//             axis and radius (binding/sampleCurves.ts, `readFaceSurfaceData`,
//             BRepAdaptor_Surface(face, restriction = false))
//   normal    the sums of the triangulation's cross products and the face's
//             orientation (binding/elementGeometry.ts,
//             `faceTriangulationNormal`), and the face's box (binding/
//             runShapeBounds.ts, `shapeBounds`)
//
// The sums are sent RAW. The binding normalises them with `Math.hypot`, whose
// last bit is the JavaScript engine's own, so Rust finishes that one step with
// the same algorithm (`capi_build/jsmath.rs`) and the answer is the same
// number. The cylinder's inward/outward probes need `Math.hypot` too and so are
// composed in Rust, which asks `kapy_classify_point` twice.
//
// Blob: u32 handle, u32 face index (0-based). `kapy_face_normal` adds a byte:
// 1 when the face is planar (the only faces whose triangulation normal is
// used). Answers: surface one byte (0 other, 1 plane, 2 cylinder) and for a
// cylinder seven f64 (origin, direction, radius); normal one byte (1 when the
// face has a triangulation), then if so three f64 and a byte (1 when the face
// is REVERSED), and then one byte (1 when the box is not void) and six f64.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the store, the arena, the cylinder's probes.

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include "capiAsk.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('getFaceGeometry').
constexpr size_t SEED_FACE = 1347086145u;

// The face at `index` (0-based) of the entry's map; an index outside it is a
// failure.
TopoDS_Face faceAt(Entry& entry, uint32_t index) {
    ensureMaps(entry);
    if (index >= static_cast<uint32_t>(entry.faces.Extent())) {
        throw OpError("face index out of range");
    }
    return TopoDS::Face(entry.faces.FindKey(static_cast<int>(index) + 1));
}

void surface(const TopoDS_Face& face, Out& out) {
    BRepAdaptor_Surface adaptor(face, false);
    const GeomAbs_SurfaceType type = adaptor.GetType();
    if (type == GeomAbs_Plane) {
        out.u8(1);
        return;
    }
    if (type != GeomAbs_Cylinder) {
        out.u8(0);
        return;
    }
    const gp_Cylinder cylinder = adaptor.Cylinder();
    const gp_Ax1 axis = cylinder.Axis();
    out.u8(2);
    out.f64(axis.Location().X());
    out.f64(axis.Location().Y());
    out.f64(axis.Location().Z());
    out.f64(axis.Direction().X());
    out.f64(axis.Direction().Y());
    out.f64(axis.Direction().Z());
    out.f64(cylinder.Radius());
}

// The sums of (b - a) x (c - a) over the face's triangles, in world space;
// the face is triangulated first when it is not yet. False when it has none.
bool triangulationSums(const TopoDS_Face& face, double sums[3]) {
    TopLoc_Location loc;
    occ::handle<Poly_Triangulation> mesh = BRep_Tool::Triangulation(face, loc, Poly_MeshPurpose_NONE);
    if (mesh.IsNull()) {
        BRepMesh_IncrementalMesh mesher(face, 0.1, false, 0.5, false);
        mesh = BRep_Tool::Triangulation(face, loc, Poly_MeshPurpose_NONE);
        if (mesh.IsNull()) return false;
    }
    const gp_Trsf trsf = loc.Transformation();
    double nx = 0.0;
    double ny = 0.0;
    double nz = 0.0;
    for (int t = 1; t <= mesh->NbTriangles(); t++) {
        const Poly_Triangle tri = mesh->Triangle(t);
        const gp_Pnt a = mesh->Node(tri.Value(1)).Transformed(trsf);
        const gp_Pnt b = mesh->Node(tri.Value(2)).Transformed(trsf);
        const gp_Pnt c = mesh->Node(tri.Value(3)).Transformed(trsf);
        const double ux = b.X() - a.X();
        const double uy = b.Y() - a.Y();
        const double uz = b.Z() - a.Z();
        const double vx = c.X() - a.X();
        const double vy = c.Y() - a.Y();
        const double vz = c.Z() - a.Z();
        nx += uy * vz - uz * vy;
        ny += uz * vx - ux * vz;
        nz += ux * vy - uy * vx;
    }
    sums[0] = nx;
    sums[1] = ny;
    sums[2] = nz;
    return true;
}

void normalAndBox(const TopoDS_Face& face, bool planar, Out& out) {
    double sums[3] = {0.0, 0.0, 0.0};
    if (planar && triangulationSums(face, sums)) {
        out.u8(1);
        out.f64(sums[0]);
        out.f64(sums[1]);
        out.f64(sums[2]);
        out.u8(face.Orientation() == TopAbs_REVERSED ? 1 : 0);
    } else {
        out.u8(0);
    }
    Bnd_Box box;
    BRepBndLib::Add(face, box, false);
    if (box.IsVoid()) {
        out.u8(0);
        return;
    }
    out.u8(1);
    out.f64(box.CornerMin().X());
    out.f64(box.CornerMin().Y());
    out.f64(box.CornerMin().Z());
    out.f64(box.CornerMax().X());
    out.f64(box.CornerMax().Y());
    out.f64(box.CornerMax().Z());
}
}  // namespace

KAPY_API int32_t kapy_face_surface(uint32_t ptr, uint32_t length) noexcept {
    return ask("getFaceGeometry", SEED_FACE, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        const uint32_t index = in.u32();
        surface(faceAt(entry, index), out);
    });
}

KAPY_API int32_t kapy_face_normal(uint32_t ptr, uint32_t length) noexcept {
    return ask("getFaceGeometry", SEED_FACE, ptr, length, [](Blob& in, Out& out) {
        Entry& entry = need(in.u32());
        const uint32_t index = in.u32();
        const bool planar = in.u8() != 0;
        normalAndBox(faceAt(entry, index), planar, out);
    });
}
