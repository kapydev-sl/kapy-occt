// services/occt/build/embind/bindings/exactAccess.hxx
//
// The private fields the exact B-Rep writes and restores as they are. Every
// public way to build a `gp_Trsf`, a `gp_Dir` or an axis system normalises or
// re-orthogonalises what it is handed, so a frame read back through one is a
// few ulps off the frame that was written; and the shape set keeps its
// geometry tables private. An explicit template instantiation is the one place
// C++ does not check access (`Field<Tag, &Class::member>` hands out a pointer
// to the member), which reads and writes each field exactly, whatever the
// layout.
//
// Everything here is in an unnamed namespace: each translation unit that
// includes it has its own copy, so the instantiations never clash.
//
// Who includes this: exactBrep.cpp and exactGeometry.cpp.
// What does NOT belong here: what is written, or when (those two files).

#pragma once

#include <BinTools_Curve2dSet.hxx>
#include <BinTools_CurveSet.hxx>
#include <BinTools_ShapeSet.hxx>
#include <BinTools_SurfaceSet.hxx>
#include <NCollection_IndexedMap.hxx>
#include <Standard_Transient.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax22d.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Mat.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Trsf.hxx>
#include <gp_XY.hxx>
#include <gp_XYZ.hxx>

namespace {

template <typename Tag, typename Tag::type Member>
struct Field {
    friend typename Tag::type member(Tag) { return Member; }
};

// One tag per field: `obj.*member(Tag())` is that field of `obj`.
#define KAPY_FIELD(Tag, Class, Type, Name)                                                         \
    struct Tag {                                                                                   \
        using type = Type Class::*;                                                                \
        friend type member(Tag);                                                                   \
    };                                                                                             \
    template struct Field<Tag, &Class::Name>;

using GeometryMap = NCollection_IndexedMap<occ::handle<Standard_Transient>>;

KAPY_FIELD(TrsfScale, gp_Trsf, double, scale)
KAPY_FIELD(TrsfForm, gp_Trsf, gp_TrsfForm, shape)
KAPY_FIELD(TrsfMatrix, gp_Trsf, gp_Mat, matrix)
KAPY_FIELD(TrsfLoc, gp_Trsf, gp_XYZ, loc)
KAPY_FIELD(DirCoord, gp_Dir, gp_XYZ, coord)
KAPY_FIELD(Dir2dCoord, gp_Dir2d, gp_XY, coord)
KAPY_FIELD(Ax1Loc, gp_Ax1, gp_Pnt, loc)
KAPY_FIELD(Ax1Dir, gp_Ax1, gp_Dir, vdir)
KAPY_FIELD(Ax2Axis, gp_Ax2, gp_Ax1, axis)
KAPY_FIELD(Ax2Y, gp_Ax2, gp_Dir, vydir)
KAPY_FIELD(Ax2X, gp_Ax2, gp_Dir, vxdir)
KAPY_FIELD(Ax3Axis, gp_Ax3, gp_Ax1, axis)
KAPY_FIELD(Ax3Y, gp_Ax3, gp_Dir, vydir)
KAPY_FIELD(Ax3X, gp_Ax3, gp_Dir, vxdir)
KAPY_FIELD(Ax2dLoc, gp_Ax2d, gp_Pnt2d, loc)
KAPY_FIELD(Ax2dDir, gp_Ax2d, gp_Dir2d, vdir)
KAPY_FIELD(Ax22dPoint, gp_Ax22d, gp_Pnt2d, point)
KAPY_FIELD(Ax22dY, gp_Ax22d, gp_Dir2d, vydir)
KAPY_FIELD(Ax22dX, gp_Ax22d, gp_Dir2d, vxdir)
KAPY_FIELD(SetSurfaces, BinTools_ShapeSet, BinTools_SurfaceSet, mySurfaces)
KAPY_FIELD(SetCurves, BinTools_ShapeSet, BinTools_CurveSet, myCurves)
KAPY_FIELD(SetCurves2d, BinTools_ShapeSet, BinTools_Curve2dSet, myCurves2d)
KAPY_FIELD(SurfaceMap, BinTools_SurfaceSet, GeometryMap, myMap)
KAPY_FIELD(CurveMap, BinTools_CurveSet, GeometryMap, myMap)
KAPY_FIELD(Curve2dMap, BinTools_Curve2dSet, GeometryMap, myMap)

#undef KAPY_FIELD

} // namespace
