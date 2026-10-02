// services/occt/build/embind/bindings/capiSweepRay.split.cpp
//
// The exact split of a mixed cylindrical face along its silhouette, so each
// half prisms exactly instead of falling back to wedges
// (`runSweepRay.cylinderSplit.ts`). The two generators where the normal is
// perpendicular to the sweep lie in the plane through the cylinder axis whose
// normal is the sweep's component across the axis; the face is trimmed against
// a slab on each side of that plane (common of the face and a box).
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: prisming the pieces, the other surfaces (they
// fall back to wedges).

#include <algorithm>
#include <cmath>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Message_ProgressRange.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Pnt.hxx>

#include "capiMeshCore.hxx"
#include "capiSweepRay.hxx"

namespace kapy_capi {

namespace {

// A planar slab: the rectangle through four corners swept along `sweep`; null
// when any of it does not build.
TopoDS_Shape planarFacePrism(const double corners[4][3], const double sweep[3]) {
    try {
        BRepBuilderAPI_MakeWire wire;
        gp_Pnt pts[4];
        for (int i = 0; i < 4; ++i) pts[i] = gp_Pnt(corners[i][0], corners[i][1], corners[i][2]);
        for (int i = 0; i < 4; ++i) {
            BRepBuilderAPI_MakeEdge edge(pts[i], pts[(i + 1) % 4]);
            wire.Add(edge.Edge());
        }
        const TopoDS_Wire w = wire.Wire();
        BRepBuilderAPI_MakeFace maker(w, true);
        const TopoDS_Face face = maker.Face();
        const gp_Vec vec(sweep[0], sweep[1], sweep[2]);
        BRepPrimAPI_MakePrism op(face, vec, false, true);
        const TopoDS_Shape shape = op.Shape();
        return shape.IsNull() ? TopoDS_Shape() : shape;
    } catch (...) {
        return TopoDS_Shape();
    }
}

}  // namespace

bool sweepSplitCylinder(const TopoDS_Shape& faceShape, const double dir[3],
                        const std::vector<SweepTri>& tris, std::vector<TopoDS_Shape>& pieces) {
    const TopoDS_Face& face = TopoDS::Face(faceShape);
    BRepAdaptor_Surface adaptor(face, false);
    if (adaptor.GetType() != GeomAbs_Cylinder) return false;
    const gp_Ax1 ax = adaptor.Cylinder().Axis();
    const double c[3] = {ax.Location().X(), ax.Location().Y(), ax.Location().Z()};
    const double a[3] = {ax.Direction().X(), ax.Direction().Y(), ax.Direction().Z()};

    // The split plane's normal: the sweep minus its component along the axis.
    // A mixed cylinder is never parallel to the sweep, so it never degenerates.
    const double da = dir[0] * a[0] + dir[1] * a[1] + dir[2] * a[2];
    double m[3] = {dir[0] - da * a[0], dir[1] - da * a[1], dir[2] - da * a[2]};
    const double ml = jsHypot3(m[0], m[1], m[2]);
    if (!(ml > 1e-9)) return false;
    for (double& v : m) v = v / ml;
    const double w[3] = {m[1] * a[2] - m[2] * a[1], m[2] * a[0] - m[0] * a[2],
                         m[0] * a[1] - m[1] * a[0]};

    // The slab's half size: every vertex of the face, with margin.
    double r = 1;
    for (const SweepTri& t : tris) {
        for (int k = 0; k < 3; ++k) {
            r = std::max(r, jsHypot3(t.p[k][0] - c[0], t.p[k][1] - c[1], t.p[k][2] - c[2]));
        }
    }
    r = r * 2 + 1;

    pieces.clear();
    for (const double sign : {1.0, -1.0}) {
        double rect[4][3];
        for (int i = 0; i < 3; ++i) {
            rect[0][i] = c[i] + (a[i] + w[i]) * r;
            rect[1][i] = c[i] + (w[i] - a[i]) * r;
            rect[2][i] = c[i] - (a[i] + w[i]) * r;
            rect[3][i] = c[i] + (a[i] - w[i]) * r;
        }
        const double sweep[3] = {m[0] * sign * r, m[1] * sign * r, m[2] * sign * r};
        const TopoDS_Shape box = planarFacePrism(rect, sweep);
        if (box.IsNull()) {
            pieces.clear();
            return false;
        }
        TopTools_ListOfShape args;
        TopTools_ListOfShape tools;
        args.Append(faceShape);
        tools.Append(box);
        BRepAlgoAPI_Common common;
        const Message_ProgressRange range;
        common.SetArguments(args);
        common.SetTools(tools);
        common.SetNonDestructive(true);
        common.Build(range);
        if (!common.IsDone()) {
            pieces.clear();
            return false;
        }
        const TopoDS_Shape trimmed = common.Shape();
        if (trimmed.IsNull()) {
            pieces.clear();
            return false;
        }
        for (TopExp_Explorer fx(trimmed, TopAbs_FACE, TopAbs_SHAPE); fx.More(); fx.Next()) {
            pieces.push_back(fx.Current().Reversed().Reversed());
        }
    }
    return !pieces.empty();
}

}  // namespace kapy_capi
