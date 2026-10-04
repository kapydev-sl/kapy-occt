// engine/kernels/occt/build/embind/bindings/capiEmbind.cpp
//
// The levers only tests pull, as embind registrations: `Kapy_CapiPerturbForTest`
// makes the C measurements lie by a known amount (the red control of the
// judges), `Kapy_HistoryJudgeForTest` judges a report of boolean history a
// broken kernel would give (15 numbers) and `Kapy_HistoryReportForTest`
// measures the real one; `Kapy_FuseManyAttemptsForTest` counts the glue
// attempts of the last fuseMany call. Nothing in the product calls them: the store, the
// naming tables and the serial counter are reached through the `kapy_*` C API.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the store itself and the C entry points.

#include <emscripten/bind.h>

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "capiFuseMany.hxx"
#include "capiSelfTest.hxx"
#include "capiState.hxx"

using namespace emscripten;

namespace {
void perturb(int mode) { kapy_capi::setPerturbation(mode); }

// The warnings the boot check gives for a report: cutEdges, cutVertices and
// fuseVertices, each as total, modified, deleted, kept, unaccounted, given as
// 15 comma-separated numbers.
std::string historyJudge(const std::string& csv) {
    std::vector<int> n;
    std::stringstream in(csv);
    for (std::string part; std::getline(in, part, ',');) n.push_back(std::atoi(part.c_str()));
    if (n.size() != 15) return "a report is 15 numbers";
    const auto at = [&n](size_t i) {
        return kapy_capi::HistoryCounts{n[i], n[i + 1], n[i + 2], n[i + 3], n[i + 4]};
    };
    return kapy_capi::historyWarnings({at(0), at(5), at(10)});
}

// What the two booleans of the boot check count, as the 15 numbers
// `historyJudge` reads ("" when a boolean did not build).
std::string historyReport() {
    kapy_capi::HistoryReport report;
    if (!kapy_capi::measureHistory(report)) return "";
    std::string out;
    for (const auto& c : {report.cutEdges, report.cutVertices, report.fuseVertices}) {
        for (int v : {c.total, c.modified, c.deleted, c.kept, c.unaccounted}) {
            if (!out.empty()) out += ',';
            out += std::to_string(v);
        }
    }
    return out;
}

// The Fuse runs the last fuseMany call made, "full,shift,plain".
std::string fuseManyAttempts() {
    const kapy_capi::FuseAttempts a = kapy_capi::lastFuseAttempts();
    return std::to_string(a.full) + "," + std::to_string(a.shift) + "," + std::to_string(a.plain);
}
}  // namespace

EMSCRIPTEN_BINDINGS(kapy_capi) {
    function("Kapy_CapiPerturbForTest", &perturb);
    function("Kapy_HistoryJudgeForTest", &historyJudge);
    function("Kapy_HistoryReportForTest", &historyReport);
    function("Kapy_FuseManyAttemptsForTest", &fuseManyAttempts);
}
