// engine/kernels/occt/build/embind/bindings/capiSelfTestHistory.cpp
//
// Boolean history, measured at boot (the check `propagationSelfTest.ts` ran in
// the browser worker before the kernel did it itself).
//
// WHAT "PROPAGATION WORKS" MEANS, measured on OCCT 8.0. `Modified(s)` lists
// what `s` became when the operation CHANGED it; an untouched sub-shape is not
// in it, and neither is a deleted one. The namer relies on all three answers:
// a changed element follows `Modified`, a deleted one is `IsDeleted`, an
// untouched one is found in the result by identity. So every edge and vertex
// of the argument must get exactly one of those answers, and `Modified` must
// report something when something WAS changed:
//
//   - cube 10 minus cube 5 at the origin: of the 8 vertices of the argument,
//     7 are untouched and 1 is deleted, 0 modified; of the 12 edges, 3 are
//     split (modified) and 9 untouched.
//   - cube 10 fused with a cube 10 sharing its x = 10 face: the 4 vertices of
//     the shared face merge with the tool's and ARE modified; 4 untouched.
//
// Zero modified vertices on the cut is the right answer (no vertex of that
// geometry is modified); the fuse is the geometry where a vertex is. Cost: two
// booleans of two boxes, a few milliseconds.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the namer, the link checks.

#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <Message_ProgressRange.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <gp_Pnt.hxx>

#include "capiSelfTest.hxx"

namespace kapy_capi {

namespace {

// What became of the `kind` sub-shapes of `argument` in `op`'s result.
HistoryCounts count(BRepAlgoAPI_BooleanOperation& op, const TopoDS_Shape& argument,
                    TopAbs_ShapeEnum kind) {
    TopTools_IndexedMapOfShape before, after;
    TopExp::MapShapes(argument, kind, before);
    TopExp::MapShapes(op.Shape(), kind, after);
    HistoryCounts out;
    out.total = before.Extent();
    for (int i = 1; i <= before.Extent(); ++i) {
        const TopoDS_Shape& sub = before.FindKey(i);
        if (op.Modified(sub).Size() > 0) ++out.modified;
        else if (op.IsDeleted(sub)) ++out.deleted;
        else if (after.FindIndex(sub) > 0) ++out.kept;
        else ++out.unaccounted;
    }
    return out;
}

// Build `op` over (argument, tool); false when it did not complete.
bool build(BRepAlgoAPI_BooleanOperation& op, const TopoDS_Shape& argument,
           const TopoDS_Shape& tool) {
    TopTools_ListOfShape args, tools;
    args.Append(argument);
    tools.Append(tool);
    op.SetArguments(args);
    op.SetTools(tools);
    op.Build(Message_ProgressRange());
    return op.IsDone();
}

// A 10-high, 10-deep box from x0 to x1.
TopoDS_Shape box(double x0, double x1) {
    return BRepPrimAPI_MakeBox(gp_Pnt(x0, 0, 0), gp_Pnt(x1, 10, 10)).Shape();
}

}  // namespace

std::string historyWarnings(const HistoryReport& report) {
    std::string out;
    const auto add = [&out](const std::string& line) {
        if (!out.empty()) out += '\n';
        out += line;
    };
    if (report.cutEdges.modified == 0) {
        add("BRepAlgoAPI_Cut.Modified returned no edges - TopoId propagation for edges may be "
            "broken in this kernel");
    }
    if (report.fuseVertices.modified == 0) {
        add("BRepAlgoAPI_Fuse.Modified returned no vertices where two corners merge - TopoId "
            "propagation for vertices may be broken in this kernel");
    }
    const int lost =
        report.cutEdges.unaccounted + report.cutVertices.unaccounted + report.fuseVertices.unaccounted;
    if (lost > 0) {
        add(std::to_string(lost) +
            " sub-shape(s) of a boolean's argument are neither modified, deleted nor kept - "
            "boolean history is incomplete in this kernel");
    }
    return out;
}

bool measureHistory(HistoryReport& out) {
    const TopoDS_Shape argument = box(0, 10);
    const TopoDS_Shape small = BRepPrimAPI_MakeBox(5.0, 5.0, 5.0).Shape();
    BRepAlgoAPI_Cut cut;
    BRepAlgoAPI_Fuse fuse;
    if (!build(cut, argument, small) || !build(fuse, argument, box(10, 20))) return false;
    out.cutEdges = count(cut, argument, TopAbs_EDGE);
    out.cutVertices = count(cut, argument, TopAbs_VERTEX);
    out.fuseVertices = count(fuse, argument, TopAbs_VERTEX);
    return true;
}

std::string historyVerdict() {
    HistoryReport report;
    if (!measureHistory(report)) return "a boolean failed - edge/vertex propagation not validated";
    return historyWarnings(report);
}

}  // namespace kapy_capi
