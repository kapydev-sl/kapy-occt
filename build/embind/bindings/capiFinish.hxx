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
// Who includes it: capiPrism.cpp, capiLoft.cpp, capiPush.cpp.
// What does NOT belong here: building the shapes, reading the blob.

#pragma once

#include <cstdint>
#include <string>

#include <TopoDS_Shape.hxx>

namespace kapy_capi {

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
uint32_t finishBox(const TopoDS_Shape& shape, const std::string& bornIn, double halfX,
                   double halfY, double halfZ);

}  // namespace kapy_capi
