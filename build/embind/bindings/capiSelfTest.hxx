// engine/kernels/occt/build/embind/bindings/capiSelfTest.hxx
//
// The boot check that the kernel reports boolean history for edges AND
// vertices, not only faces: what `kapy_self_test` (capiCore.cpp) asks of
// capiSelfTestHistory.cpp after the link checks. A regression there would not
// fail anything loudly; the naming would follow a history that silently lost
// sub-shapes and every regeneration would hand out wrong TopoIds.
//
// Who includes it: capiCore.cpp, capiSelfTestHistory.cpp, capiEmbind.cpp (the
// test hook that judges a report a broken kernel would give).
// What does NOT belong here: the link checks (capiCore.cpp), the namer.

#pragma once

#include <string>

namespace kapy_capi {

// How one kind of sub-shape of a boolean's ARGUMENT fared.
struct HistoryCounts {
    int total = 0;
    int modified = 0;
    int deleted = 0;
    int kept = 0;
    // Neither modified, nor deleted, nor in the result by identity: a sub-shape
    // the history lost. Always a bug.
    int unaccounted = 0;
};

// What the two booleans report, per kind of sub-shape.
struct HistoryReport {
    HistoryCounts cutEdges, cutVertices, fuseVertices;
};

// The warnings a report deserves, one per line ("" when it is healthy). Pure,
// so the rule can be held to the reports a broken kernel would give.
std::string historyWarnings(const HistoryReport& report);

// Build the two booleans and count; false when either did not build.
bool measureHistory(HistoryReport& out);

// Measure and judge: "" when the history is complete, else the warnings (or
// the one that says a boolean did not build).
std::string historyVerdict();

}  // namespace kapy_capi
