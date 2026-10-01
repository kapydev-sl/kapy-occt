// services/occt/build/embind/bindings/capiBoolean.hxx
//
// The boolean the C API runs, shared by the operation that answers a boolean
// (capiBoolean.cpp) and the one that cuts a plane's far side away (capiTrim.cpp),
// which the binding also builds on `runBoolean`.
//
// Who includes it: capiBoolean.cpp, capiTrim.cpp.
// What does NOT belong here: reading a blob, the entry points.

#pragma once

#include <cstdint>
#include <string>

namespace kapy_capi {

// The kinds of a boolean, numbered as the blob writes them.
enum BooleanKind : uint8_t { kCut = 0, kFuse = 1, kCommon = 2 };

// `runBoolean`: the operation on two stored shapes, healed when it comes back
// invalid, named through its history and stored. Answers the new handle, or 0
// for an empty intersection.
uint32_t booleanOf(uint32_t previous, uint32_t tool, uint8_t kind, const std::string& bornIn,
                   double fuzzy, bool glue, bool unify);

}  // namespace kapy_capi
