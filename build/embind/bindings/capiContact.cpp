// services/occt/build/embind/bindings/capiContact.cpp
//
// `kapy_shapes_intersect`: do two stored solids touch or overlap. The
// binding's own predicate (binding/runShapesIntersect.ts), step for step:
//   1. each shape's box, enlarged by the contact tolerance; boxes that do not
//      meet answer no.
//   2. the faces of each shape whose own box reaches into the other shape's.
//   3. every pair of those faces whose boxes meet goes to
//      BRepExtrema_DistShapeShape; a pair within the tolerance answers yes.
//   4. otherwise one shape inside the other: its first vertex classifies IN
//      against the other.
//
// Blob: u32 a, u32 b. Answers one byte.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the store, the arena, the classifier's walk.

#include <vector>

#include <BRepBndLib.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

#include "capiAsk.hxx"
#include "capiClassify.hxx"

using namespace kapy_capi;

namespace {
// serialSeedOf('shapesIntersect').
constexpr size_t SEED_INTERSECT = 449509858u;
// Contact tolerance in mm: coincident faces resolve to ~0, and a small epsilon
// absorbs the noise in the inputs of a boolean.
constexpr double CONTACT_TOL = 1e-4;

struct Box {
    double v[6];
};

// The shape's box enlarged by the tolerance; false for an empty shape.
bool boxOf(const TopoDS_Shape& shape, Box& out) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box, false);
    if (box.IsVoid()) return false;
    const gp_Pnt lo = box.CornerMin();
    const gp_Pnt hi = box.CornerMax();
    out.v[0] = lo.X() - CONTACT_TOL;
    out.v[1] = lo.Y() - CONTACT_TOL;
    out.v[2] = lo.Z() - CONTACT_TOL;
    out.v[3] = hi.X() + CONTACT_TOL;
    out.v[4] = hi.Y() + CONTACT_TOL;
    out.v[5] = hi.Z() + CONTACT_TOL;
    return true;
}

bool overlap(const Box& p, const Box& q) {
    return p.v[0] <= q.v[3] && q.v[0] <= p.v[3] && p.v[1] <= q.v[4] && q.v[1] <= p.v[4] &&
           p.v[2] <= q.v[5] && q.v[2] <= p.v[5];
}

struct BoxedFace {
    TopoDS_Shape face;
    Box box;
};

// The faces of `shape` whose boxes reach into `within`.
std::vector<BoxedFace> facesWithBoxes(const TopoDS_Shape& shape, const Box& within) {
    std::vector<BoxedFace> out;
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(shape, TopAbs_FACE, map);
    for (int i = 1; i <= map.Extent(); i++) {
        BoxedFace f;
        f.face = map.FindKey(i);
        if (boxOf(f.face, f.box) && overlap(f.box, within)) out.push_back(f);
    }
    return out;
}

bool distanceWithin(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    BRepExtrema_DistShapeShape dss;
    dss.LoadS1(a);
    dss.LoadS2(b);
    dss.Perform();
    return dss.IsDone() && dss.Value() <= CONTACT_TOL;
}

// Whether `inner` sits inside `outer`: its first vertex classifies IN.
bool contains(const TopoDS_Shape& outer, const TopoDS_Shape& inner) {
    TopExp_Explorer ex(inner, TopAbs_VERTEX);
    if (!ex.More()) return false;
    const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(ex.Current()));
    return probePoint(outer, p.X(), p.Y(), p.Z()).state == STATE_IN;
}

bool shapesIntersect(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    Box boxA;
    Box boxB;
    if (!boxOf(a, boxA) || !boxOf(b, boxB) || !overlap(boxA, boxB)) return false;
    const std::vector<BoxedFace> facesA = facesWithBoxes(a, boxB);
    const std::vector<BoxedFace> facesB = facesWithBoxes(b, boxA);
    for (const BoxedFace& fa : facesA) {
        for (const BoxedFace& fb : facesB) {
            if (overlap(fa.box, fb.box) && distanceWithin(fa.face, fb.face)) return true;
        }
    }
    return contains(a, b) || contains(b, a);
}
}  // namespace

KAPY_API int32_t kapy_shapes_intersect(uint32_t ptr, uint32_t length) noexcept {
    return ask("shapesIntersect", SEED_INTERSECT, ptr, length, [](Blob& in, Out& out) {
        Entry& a = need(in.u32());
        Entry& b = need(in.u32());
        const bool touch = shapesIntersect(a.shape, b.shape);
        // The red control: no two bodies ever touch, so nothing auto-joins.
        out.u8(touch && perturbation() != 8 ? 1 : 0);
    });
}
