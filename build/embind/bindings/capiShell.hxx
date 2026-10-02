// services/occt/build/embind/bindings/capiShell.hxx
//
// The thick-solid attempts the shell runs, shared by the build (capiShell.cpp)
// and the cleanup fallback that rebuilds the body first (capiShellCleanup.cpp).
// The ladder is data: each rung is a tolerance and a join, taken from the
// blob, in the order the Rust recipe wrote them. This file only runs them.
//
// Who includes it: capiShell.cpp, capiShellCleanup.cpp, capiShellFacts.cpp.
// What does NOT belong here: which rungs to try (the Rust ladder), naming.

#pragma once

#include <memory>
#include <vector>

#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Shape.hxx>

#include "capiStore.hxx"

namespace kapy_capi {

// One attempt of a ladder: the offset tolerance and the join it names.
struct Rung {
    double tol;
    bool intersection;
};

// The winning operator (the naming reads its history, so it outlives the
// call that made it) and its result. `op` is null when no attempt built.
struct ThickResult {
    std::unique_ptr<BRepOffsetAPI_MakeThickSolid> op;
    TopoDS_Shape result;
};

// MakeThickSolidByJoin on `shape` through `rungs`, in order: the first attempt
// that finishes and passes the result gate with a valid shape; failing that,
// the first usable but invalid one; failing that, an empty result.
// `inputVolume` is the volume a result must differ from.
ThickResult makeThickSolid(const TopoDS_Shape& shape, const TopTools_ListOfShape& closing,
                           double thickness, const std::vector<Rung>& rungs, double inputVolume);

// The fallback for a body whose micro-wall slivers defeat the plain offset:
// drop them, sew the rest into a solid, and hollow that through `rungs`. The
// openings of the original body are found again on the clean one by centroid.
ThickResult shellCleaned(const TopoDS_Shape& shape, const std::vector<TopoDS_Shape>& removed,
                         double thickness, const std::vector<Rung>& rungs);

// One solid of a body a shell opens, with the opening faces that lie on it
// (`removed` is empty for a solid nothing is removed from, which passes through
// a shell unchanged) and the same faces as the list MakeThickSolid takes.
struct ShellPiece {
    TopoDS_Shape shape;
    TopTools_ListOfShape closing;
    std::vector<TopoDS_Shape> removed;
};

// The solids of a body, in the order every shell question speaks (the index of
// a solid in a failure, in the facts and in the probe is its place here). A body
// of at most one solid is taken whole with every picked face; a body of
// several is split, each solid owning the picked faces it holds.
struct ShellPieces {
    bool whole;
    std::vector<ShellPiece> pieces;
};

// The picked face indices checked against the body, by the words the shell
// always gave; a failure throws `OpError`.
void checkShellFaces(const Entry& prev, const std::vector<uint32_t>& indices);

// `prev` split into the solids a shell opens.
ShellPieces shellPieces(const Entry& prev, const std::vector<uint32_t>& indices);

}  // namespace kapy_capi
