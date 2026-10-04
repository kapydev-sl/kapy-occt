// engine/kernels/occt/build/embind/bindings/capiFuseMany.hxx
//
// What the last `kapy_fuse_many` call tried, for the test lever
// `Kapy_FuseManyAttemptsForTest`: how many BRepAlgoAPI_Fuse runs it made at
// each glue level. A test reads it to hold the glue memo to its contract (a
// rejected full glue is remembered per input, never per feature), which the
// fused shape alone cannot show because both levels give the same faces on a
// touching grid.
//
// Who includes it: capiFuseMany.cpp (counts) and capiEmbind.cpp (reports).
// What does NOT belong here: the fuse itself or the memo.

#pragma once

namespace kapy_capi {

// The Fuse runs of one call: full glue, shift glue, and no glue at all.
struct FuseAttempts {
    int full = 0;
    int shift = 0;
    int plain = 0;
};

// The attempts of the most recent `kapy_fuse_many` call (all zero before the
// first call, and for a compound with one member).
FuseAttempts lastFuseAttempts();

}  // namespace kapy_capi
