// services/occt/build/embind/bindings/capiOffsetSlab.cpp
//
// The slab fallback of Offset Faces (see capiOffsetSlab.hxx), call for call
// as `runOffsetFaces.slab.ts`: the thickening, the boolean that is built empty
// and staged (arguments, tools, the fuzzy value) before its one Build, and the
// final validity gate.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the true offset, naming.

#include "capiOffsetSlab.hxx"

#include <memory>

#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepOffset_Mode.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Message_ProgressRange.hxx>
#include <TopTools_ListOfShape.hxx>

#include "capiGuards.hxx"

namespace {

// Thickening tolerance and the fuzzy value of the boolean (mm).
constexpr double SLAB_TOL = 1e-3;
constexpr double SLAB_FUZZY = 1e-3;

// The solid between `face` and its copy offset by `distance` along the face's
// outward normal; null when the offset does not build.
TopoDS_Shape thicken(const TopoDS_Face& face, double distance) {
    try {
        BRepOffset_MakeOffset op;
        // Thickening turns the face into the solid between it and its offset.
        op.Initialize(face, distance, SLAB_TOL, BRepOffset_Skin, false, false, GeomAbs_Arc, true,
                      false);
        op.MakeOffsetShape();
        if (!op.IsDone()) return TopoDS_Shape();
        const TopoDS_Shape r = op.Shape();
        if (r.IsNull()) return TopoDS_Shape();
        return kapy_capi::ensureSolid(r);
    } catch (...) {
        return TopoDS_Shape();
    }
}

// `base` united with `slab` or `base` minus `slab`; null when it does not
// finish.
TopoDS_Shape combine(const TopoDS_Shape& base, const TopoDS_Shape& slab, bool fuse) {
    TopTools_ListOfShape args;
    TopTools_ListOfShape tools;
    const Message_ProgressRange range;
    std::unique_ptr<BRepAlgoAPI_BooleanOperation> op;
    if (fuse) op.reset(new BRepAlgoAPI_Fuse());
    else op.reset(new BRepAlgoAPI_Cut());
    args.Append(base);
    tools.Append(slab);
    op->SetArguments(args);
    op->SetTools(tools);
    op->SetFuzzyValue(SLAB_FUZZY);
    op->Build(range);
    if (!op->IsDone()) return TopoDS_Shape();
    const TopoDS_Shape r = op->Shape();
    return r.IsNull() ? TopoDS_Shape() : r;
}

}  // namespace

namespace kapy_capi {

TopoDS_Shape offsetFacesBySlabs(const TopoDS_Shape& shape, const std::vector<TopoDS_Face>& faces,
                                double distance) {
    try {
        TopoDS_Shape current = shape;
        for (const TopoDS_Face& face : faces) {
            const TopoDS_Shape slab = thicken(face, distance);
            if (slab.IsNull()) return TopoDS_Shape();
            const TopoDS_Shape next = combine(current, slab, distance > 0);
            if (next.IsNull()) return TopoDS_Shape();
            current = next;
        }
        const TopoDS_Shape fixed = validOrHealed(current);
        if (fixed.IsNull()) return TopoDS_Shape();
        return ensureSolid(fixed);
    } catch (...) {
        return TopoDS_Shape();
    }
}

}  // namespace kapy_capi
