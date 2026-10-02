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

#include "capiProfile.hxx"
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

Mapped::Mapped(const TopoDS_Shape& shape) {
    TopExp::MapShapes(shape, TopAbs_FACE, face);
    TopExp::MapShapes(shape, TopAbs_EDGE, edge);
    TopExp::MapShapes(shape, TopAbs_VERTEX, vertex);
}

bool isValid(const TopoDS_Shape& shape) {
    BRepCheck_Analyzer analyzer(shape, true, false);
    return analyzer.IsValid();
}

namespace {

// Store `shape` as the handle of table `id`: announced to the host, or kept
// from it (an intermediate).
uint32_t store(const TopoDS_Shape& shape, uint32_t id, bool announce) {
    uint32_t handle;
    if (announce) {
        handle = putNative(shape, id);
    } else {
        handle = put(shape);
        find(handle)->tableId = id;
    }
    kapy_facts::bind(id, "h_" + std::to_string(handle));
    return handle;
}

}  // namespace

uint32_t finishNamed(const TopoDS_Shape& shape, const Mapped& maps, uint32_t tableId,
                     const std::string& bornIn, bool unify, bool announce) {
    uint32_t id = tableId;
    TopoDS_Shape stored = shape;
    if (unify) {
        uint32_t unifyId = 0;
        const TopoDS_Shape unified = tryUnify(shape, maps, tableId, bornIn, unifyId);
        if (!unified.IsNull()) {
            kapy_facts::release(tableId);
            stored = unified;
            id = unifyId;
        }
    }
    return store(stored, id, announce);
}

uint32_t finishHistory(const TopoDS_Shape& shape, BRepBuilderAPI_MakeShape& maker,
                       Entry& previous, Entry* tool, const std::string& bornIn, bool unify,
                       bool announce) {
    if (previous.tableId == 0 || (tool && tool->tableId == 0)) {
        throw OpError("an operand namer carries no naming table");
    }
    ensureMaps(previous);
    if (tool) ensureMaps(*tool);
    const Mapped maps(shape);
    const uint32_t id = allocTable();
    const kapy_facts::Maps before{&previous.faces, &previous.edges, &previous.vertices};
    const kapy_facts::Maps with{tool ? &tool->faces : nullptr, tool ? &tool->edges : nullptr,
                                tool ? &tool->vertices : nullptr};
    kapy_facts::buildBooleanOfMaker(id, previous.tableId, tool ? tool->tableId : 0, maps.view(),
                                    bornIn, maker, before, tool ? &with : nullptr);
    return finishNamed(shape, maps, id, bornIn, unify, announce);
}

uint32_t finishHistoryOfMakers(const TopoDS_Shape& shape,
                               const std::vector<BRepBuilderAPI_MakeShape*>& makers,
                               Entry& previous, const std::string& bornIn, bool unify) {
    if (previous.tableId == 0) throw OpError("an operand namer carries no naming table");
    ensureMaps(previous);
    const Mapped maps(shape);
    const uint32_t id = allocTable();
    const kapy_facts::Maps before{&previous.faces, &previous.edges, &previous.vertices};
    kapy_facts::buildBooleanOfMakers(id, previous.tableId, maps.view(), bornIn, makers, before);
    return finishNamed(shape, maps, id, bornIn, unify, true);
}

void dropNamed(uint32_t handle) {
    Entry* entry = find(handle);
    if (!entry) return;
    const uint32_t table = entry->tableId;
    release(handle, false);
    if (table != 0) kapy_facts::release(table);
}

uint32_t finishSolid(const TopoDS_Shape& shape, const Finish& how) {
    const Mapped maps(shape);
    const uint32_t extrudeId = allocTable();
    kapy_facts::buildExtrude(extrudeId, how.previousTable, maps.view(), how.bornIn, how.rolesJson,
                             how.roleSuffix);
    return finishNamed(shape, maps, extrudeId, how.bornIn, how.unify);
}

uint32_t finishBox(const TopoDS_Shape& shape, const std::string& bornIn, double halfX,
                   double halfY, double halfZ, bool announce) {
    const Mapped maps(shape);
    const uint32_t id = allocTable();
    kapy_facts::buildBox(id, maps.view(), bornIn, halfX, halfY, halfZ);
    return store(shape, id, announce);
}

}  // namespace kapy_capi
