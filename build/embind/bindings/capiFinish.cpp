// services/occt/build/embind/bindings/capiFinish.cpp
//
// The end of a C build operation; see capiFinish.hxx. The unify step repeats
// `unifyFuse.ts` call for call: the same tolerances, the same refusals in the
// same order (no simplification, an invalid result, a boundary that moved),
// and the same silence when anything throws, because each of those calls
// creates OCCT objects and their numbering is part of what the judges compare.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: building shapes, the arguments' layout.

#include "capiFinish.hxx"

#include <cmath>
#include <algorithm>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopExp.hxx>

#include "capiStore.hxx"
#include "factsInternal.hxx"

namespace kapy_capi {

namespace {

// The linear and angular tolerances handed to the unifier (unifyFuse.ts).
constexpr double UNIFY_LINEAR_TOL = 1e-3;
constexpr double UNIFY_ANGULAR_TOL = 1e-4;
// How far the boundary may seem to have moved (unifyFuse.ts).
constexpr double MAX_BOUNDARY_SHIFT_MM = 1e-5;
constexpr double MAX_AREA_DRIFT_REL = 1e-5;

// A shape's three canonical maps.
struct Mapped {
    ShapeIndexedMap face, edge, vertex;
    explicit Mapped(const TopoDS_Shape& shape) {
        TopExp::MapShapes(shape, TopAbs_FACE, face);
        TopExp::MapShapes(shape, TopAbs_EDGE, edge);
        TopExp::MapShapes(shape, TopAbs_VERTEX, vertex);
    }
    kapy_facts::Maps view() const { return {&face, &edge, &vertex}; }
};

bool isValid(const TopoDS_Shape& shape) {
    BRepCheck_Analyzer analyzer(shape, true, false);
    return analyzer.IsValid();
}

struct Measure {
    double area, volume;
};

// Total surface area, then solid volume, on one properties object, as
// `measure` in unifyFuse.ts.
Measure measure(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(shape, props, false, false);
    const double area = props.Mass();
    BRepGProp::VolumeProperties(shape, props, true, false, false);
    return {area, props.Mass()};
}

bool boundaryMoved(const Measure& before, const Measure& after) {
    const double span = std::max(1.0, std::fabs(before.area));
    return std::fabs(after.area - before.area) > MAX_AREA_DRIFT_REL * span ||
           std::fabs(after.volume - before.volume) > MAX_BOUNDARY_SHIFT_MM * span;
}

// Facts of the extrude construction `id` and the unified body that replaces
// it, when unifying simplifies; answers the unified shape and its table, or a
// null shape.
TopoDS_Shape tryUnify(const TopoDS_Shape& shape, const Mapped& before, uint32_t extrudeId,
                      const std::string& bornIn, uint32_t& unifyId) {
    try {
        ShapeUpgrade_UnifySameDomain u(shape, true, true, false);
        u.SetLinearTolerance(UNIFY_LINEAR_TOL);
        u.SetAngularTolerance(UNIFY_ANGULAR_TOL);
        u.Build();
        const TopoDS_Shape unified = u.Shape();
        if (unified.IsNull()) return TopoDS_Shape();
        Mapped after(unified);
        if (after.face.Extent() >= before.face.Extent() &&
            after.edge.Extent() >= before.edge.Extent()) {
            return TopoDS_Shape();
        }
        if (!isValid(unified)) return TopoDS_Shape();
        if (isValid(shape)) {
            const Measure a = measure(shape);
            const Measure b = measure(unified);
            if (boundaryMoved(a, b)) return TopoDS_Shape();
        }
        BRepTools_History& history = *u.History();
        unifyId = allocTable();
        kapy_facts::buildUnifyOf(unifyId, extrudeId, after.view(), bornIn, history, before.view());
        return unified;
    } catch (...) {
        return TopoDS_Shape();
    }
}

}  // namespace

uint32_t finishSolid(const TopoDS_Shape& shape, const Finish& how) {
    const Mapped maps(shape);
    const uint32_t extrudeId = allocTable();
    kapy_facts::buildExtrude(extrudeId, how.previousTable, maps.view(), how.bornIn, how.rolesJson,
                             how.roleSuffix);
    uint32_t tableId = extrudeId;
    TopoDS_Shape stored = shape;
    if (how.unify) {
        uint32_t unifyId = 0;
        const TopoDS_Shape unified = tryUnify(shape, maps, extrudeId, how.bornIn, unifyId);
        if (!unified.IsNull()) {
            kapy_facts::release(extrudeId);
            stored = unified;
            tableId = unifyId;
        }
    }
    const uint32_t handle = putNative(stored, tableId);
    kapy_facts::bind(tableId, "h_" + std::to_string(handle));
    return handle;
}

uint32_t finishBox(const TopoDS_Shape& shape, const std::string& bornIn, double halfX,
                   double halfY, double halfZ) {
    const Mapped maps(shape);
    const uint32_t id = allocTable();
    kapy_facts::buildBox(id, maps.view(), bornIn, halfX, halfY, halfZ);
    const uint32_t handle = putNative(shape, id);
    kapy_facts::bind(id, "h_" + std::to_string(handle));
    return handle;
}

}  // namespace kapy_capi
