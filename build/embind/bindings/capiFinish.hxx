// services/occt/build/embind/bindings/capiFinish.hxx
//
// How a C build operation ends: the shape it built is named as an extrude
// result, collapsed of the seams its constructor left when that simplifies it
// and keeps it valid, stored, bound to its naming table, and answered as a
// handle. It is `nameExtrudeShape` + `storeSolidShape` of the TypeScript
// binding (`namerFacts/builders.ts`, `binding/storeSolid.ts`) step for step,
// writing the same facts in the same order, because the core reads the facts
// and the kernel's serials follow the order of the OCCT calls.
//
// The operations that name a result through its operands' history (booleans,
// transforms, the trim's box) end the same way from the facts on:
// `finishHistory`, which is `nameHistoryShape` + `storeShape`.
//
// Who includes it: the capi*.cpp build operations.
// What does NOT belong here: building the shapes, reading the blob.

#pragma once

#include <cstdint>
#include <string>

#include <BRepBuilderAPI_MakeShape.hxx>
#include <TopoDS_Shape.hxx>

#include "capiStore.hxx"
#include "factsInternal.hxx"

namespace kapy_capi {

// A shape's three canonical maps, as the naming reads them.
struct Mapped {
    ShapeIndexedMap face, edge, vertex;
    explicit Mapped(const TopoDS_Shape& shape);
    kapy_facts::Maps view() const { return {&face, &edge, &vertex}; }
};

// BRepCheck_Analyzer(shape, geometry controls on, not parallel).
bool isValid(const TopoDS_Shape& shape);

// What a build operation hands over once the shape exists.
struct Finish {
    // The feature that owns the shape, and the suffix its roles carry.
    std::string bornIn;
    std::string roleSuffix;
    // The role facts as the JSON the core reads.
    std::string rolesJson;
    // The naming table the shape is named against (0: none). A pushed face is
    // named against the body it was taken from.
    uint32_t previousTable = 0;
    // Collapse same-domain seams (a compound is stored as it is).
    bool unify = true;
};

// Name, unify, store and bind `shape`; answers the new handle. Throws what the
// OCCT calls throw.
uint32_t finishSolid(const TopoDS_Shape& shape, const Finish& how);

// A box named by its half sizes, stored and bound; there is nothing to unify.
// `announce` false keeps it from the host (an intermediate that is released
// before the call ends).
uint32_t finishBox(const TopoDS_Shape& shape, const std::string& bornIn, double halfX,
                   double halfY, double halfZ, bool announce = true);

// Store `shape` under the naming table `tableId` (already written), collapsing
// same-domain seams first when `unify` and it helps (the first table is then
// released); binds the table to the handle. `announce` as above.
uint32_t finishNamed(const TopoDS_Shape& shape, const Mapped& maps, uint32_t tableId,
                     const std::string& bornIn, bool unify, bool announce = true);

// A result named through `maker`'s history of `previous` (and of `tool` when
// there is one): the boolean construction, then `finishNamed`.
uint32_t finishHistory(const TopoDS_Shape& shape, BRepBuilderAPI_MakeShape& maker,
                       Entry& previous, Entry* tool, const std::string& bornIn, bool unify,
                       bool announce = true);

// Drop an intermediate handle and its naming table, in the order the binding's
// `releaseShape` does (the store first, then the namer).
void dropNamed(uint32_t handle);

}  // namespace kapy_capi
