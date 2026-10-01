// services/occt/build/embind/bindings/capiGuards.hxx
//
// The gates an offset-family result has to pass before it becomes a body, with
// the same decisions as the TypeScript binding's `healShape.ts`,
// `solidGuards.ts` and `hasDegenerateFace` (`runShell.helpers.ts`): is it
// valid (and if not, can ShapeFix make it so), is it a real solid that
// changed the body, does it hold a face collapsed to nothing, and how much
// tolerance does it carry. The order of the OCCT calls is the binding's too,
// because ShapeFix_Shape and the solid builder are numbered objects.
//
// A null shape is the answer for "nothing usable"; nothing here throws except
// what OCCT itself throws, and the callers that treat a throw as a refusal
// catch it.
//
// Who includes it: capiShell.cpp, capiShellCleanup.cpp, capiOffset.cpp,
// capiOffsetFaces.cpp.
// What does NOT belong here: running an offset, naming a result.

#pragma once

#include <cstddef>

#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>

namespace kapy_capi {

// A face below this area (mm2) is a collapsed one: the attempt is refused.
constexpr double DEGENERATE_FACE_AREA = 1e-4;

// Cap on the tolerances a successful offset-family attempt leaves behind (mm).
constexpr double TOLERANCE_CAP = 1e-3;

// ShapeFix_Shape on `shape`: the repaired shape, or null when it throws or
// does not come out valid.
TopoDS_Shape healShape(const TopoDS_Shape& shape);

// `shape` itself when it is valid, its repair when it is not, null when there
// is none.
TopoDS_Shape validOrHealed(const TopoDS_Shape& shape);

// Cap every tolerance in `shape` at `tmax` mm, in place; false when it throws.
bool limitTolerance(const TopoDS_Shape& shape, double tmax);

// How many sub-shapes of `kind` `shape` holds (explorer, nothing avoided).
size_t countSubShapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind);

// The volume `shape` encloses, signed by its orientation.
double signedVolume(const TopoDS_Shape& shape);

// `shape` when it already holds a solid; otherwise the solid sewn from its
// shells, oriented, valid and enclosing the same volume; otherwise null.
TopoDS_Shape ensureSolid(const TopoDS_Shape& shape);

// Whether `result` holds a solid of positive volume that differs from
// `inputVolume` beyond a thousandth: the attempt hollowed or grew something.
bool changedSolidVolume(const TopoDS_Shape& result, double inputVolume);

// Whether any face of `shape` has an area below DEGENERATE_FACE_AREA.
bool hasDegenerateFace(const TopoDS_Shape& shape);

}  // namespace kapy_capi
