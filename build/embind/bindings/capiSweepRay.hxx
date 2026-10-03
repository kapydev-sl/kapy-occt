// engine/kernels/occt/build/embind/bindings/capiSweepRay.hxx
//
// The swept volume of a solid along a ray, shared by the two operations that
// build one (`kapy_sweep_ray`, `kapy_clearance_cut`): the part fused with the
// linear prism of each of its faces, as `runSweepRay.ts` builds it. The faces
// are read and classified against the direction (capiSweepRay.faces.cpp), the
// mixed cylinders are split exactly along their silhouette
// (capiSweepRay.split.cpp), and the core (capiSweepRay.cpp) prisms every piece
// and fuses them. Every OCCT call is the binding's, in its order, because the
// objects they make are numbered and the judges compare the numbering.
//
// Who includes it: capiSweepRay*.cpp, capiClearanceCut.cpp.
// What does NOT belong here: reading a blob, naming the result, the clearance
// grow.

#pragma once

#include <memory>
#include <vector>

#include <BRepAlgoAPI_Fuse.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Vec.hxx>

namespace kapy_capi {

// Mesh precision for the faces a wedge needs (mm, rad), and the coarse mesh a
// face that carries none is classified with.
constexpr double SWEEP_LIN_DEFLECTION = 0.02;
constexpr double SWEEP_ANG_DEFLECTION = 0.25;
constexpr double SWEEP_CLASSIFY_LIN = 0.1;
constexpr double SWEEP_CLASSIFY_ANG = 0.5;
// Prisms below this volume (mm3) are degenerate and contribute nothing.
constexpr double SWEEP_MIN_PRISM_VOLUME = 1e-6;
// |unit normal . dir| below this counts as parallel to the sweep.
constexpr double SWEEP_PARALLEL_TOL = 1e-4;

// One triangle in world space: three corners of xyz.
struct SweepTri {
    double p[3][3];
};

// How a face's normals stand against the sweep direction.
enum class FaceClass { kParallel, kMonotone, kMixed };

// The normal of a triangle (not normalised).
void sweepTriNormal(const SweepTri& t, double out[3]);

// The world-space triangles of `face`; false when it has no triangulation.
// `fine` meshes at the wedge precision, otherwise any triangulation the face
// carries is reused and a coarse one is made when it has none.
bool sweepFaceTriangles(const TopoDS_Shape& face, bool fine, std::vector<SweepTri>& out);

// The class of a face from its triangle normals.
FaceClass sweepClassify(const std::vector<SweepTri>& tris, const double dir[3]);

// The solid volume of `shape` (closed shells only), signed by its orientation.
double sweepClosedVolume(const TopoDS_Shape& shape);

// The exact prism of a whole monotone face; null when MakePrism rejects it or
// the result is degenerate. Oriented by the sign of its volume.
TopoDS_Shape sweepFacePrism(const TopoDS_Shape& face, const gp_Vec& vec);

// The full-length wedge of one triangle, inflated in its own plane so the
// chord error can only widen the channel; null when degenerate.
TopoDS_Shape sweepTriangleWedge(const SweepTri& tri, const double dir[3], const gp_Vec& vec);

// The two monotone halves of a mixed cylindrical face, cut along its
// silhouette; false for any other surface or when the split fails.
bool sweepSplitCylinder(const TopoDS_Shape& face, const double dir[3],
                        const std::vector<SweepTri>& tris, std::vector<TopoDS_Shape>& pieces);

// What the swept volume is: the fused shape and the operator that made it (the
// caller names the result through it).
struct SweepResult {
    TopoDS_Shape result;
    std::unique_ptr<BRepAlgoAPI_Fuse> fuse;
};

// `shape` fused with the prism of each of its faces along `dir` (a unit
// vector) for `length`. Throws what the binding throws, in the same cases.
SweepResult sweepRayCore(const TopoDS_Shape& shape, const double dir[3], double length);

}  // namespace kapy_capi
