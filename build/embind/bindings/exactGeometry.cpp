// services/occt/build/embind/bindings/exactGeometry.cpp
//
// The frames of a shape set's geometry, carried exactly. OCCT's binary shape
// set writes a frame's every double, but reads a direction through `gp_Dir`
// (which renormalises a vector whose squared length is not exactly 1) and an
// axis system through `gp_Ax3(P, N, Vx)` / `gp_Ax2` / `gp_Ax22d` (which rebuild
// X and Y from N by cross products). A rotated plane, cylinder or circle comes
// back a few ulps off. So the writer records, per surface, curve and 2D curve
// of the set's tables, the frames that carry a direction — recursing into the
// basis a swept, offset or trimmed one wraps — and the reader, once the tables
// are read and before any shape takes them, writes those values back field by
// field through each object's own setter (which clears what it evaluated).
//
// A record: u8 kind, then the kind's frames. Kinds come from the registry
// (exactRegistry.cpp), by exact class: the writer throws
// `kapy_exact::Unregistered` at the first class it does not list, so a class
// nobody checked is never written at all; a kind the reader does not find on
// the object it read is a failure too. Either way the cache is refused, never
// misread.
//
// Who includes this: the embind link; exactBrep.cpp calls it.
// What does NOT belong here: locations or flags (exactBrep.cpp). B-splines,
// Béziers and raw values (radii, parameters, offsets): the shape set already
// reads those exactly.

#include "exactGeometry.hxx"
#include "exactAccess.hxx"
#include "exactRegistry.hxx"

#include <BinTools.hxx>
#include <Geom2d_Conic.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_OffsetCurve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom_Conic.hxx>
#include <Geom_Curve.hxx>
#include <Geom_ElementarySurface.hxx>
#include <Geom_Line.hxx>
#include <Geom_OffsetCurve.hxx>
#include <Geom_OffsetSurface.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_Surface.hxx>
#include <Geom_SurfaceOfLinearExtrusion.hxx>
#include <Geom_SurfaceOfRevolution.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>

namespace {
using namespace kapy_exact;

// The registered kind of `g` in `table`; a null geometry has none to carry.
// Writing, an unregistered class is refused by name; reading, it is a failure.
template <typename H>
Kind kindIn(Table table, const H& g, bool writing) {
    if (g.IsNull()) return NONE;
    const int k = registeredKind(table, g->DynamicType());
    if (k >= 0) return static_cast<Kind>(k);
    if (writing) throw Unregistered{g->DynamicType()->Name()};
    throw Standard_Failure("exact brep: an unregistered geometry class");
}

double getReal(std::istream& is) {
    double v = 0.;
    BinTools::GetReal(is, v);
    return v;
}

void putXYZ(std::ostream& os, const gp_XYZ& v) {
    for (int i = 1; i <= 3; i++) BinTools::PutReal(os, v.Coord(i));
}
gp_XYZ getXYZ(std::istream& is) {
    const double x = getReal(is), y = getReal(is);
    return gp_XYZ(x, y, getReal(is));
}
void putXY(std::ostream& os, const gp_XY& v) {
    BinTools::PutReal(os, v.X());
    BinTools::PutReal(os, v.Y());
}
gp_XY getXY(std::istream& is) {
    const double x = getReal(is);
    return gp_XY(x, getReal(is));
}

gp_Dir getDir(std::istream& is) {
    gp_Dir d;
    d.*member(DirCoord()) = getXYZ(is);
    return d;
}
gp_Dir2d getDir2d(std::istream& is) {
    gp_Dir2d d;
    d.*member(Dir2dCoord()) = getXY(is);
    return d;
}

void putAx1(std::ostream& os, const gp_Ax1& a) {
    putXYZ(os, a.Location().XYZ());
    putXYZ(os, a.Direction().XYZ());
}
gp_Ax1 getAx1(std::istream& is) {
    gp_Ax1 a;
    a.*member(Ax1Loc()) = gp_Pnt(getXYZ(is));
    a.*member(Ax1Dir()) = getDir(is);
    return a;
}

// gp_Ax2 and gp_Ax3 carry the same four fields: location, main, X and Y.
template <typename Ax>
void putFrame(std::ostream& os, const Ax& a) {
    putAx1(os, a.Axis());
    putXYZ(os, a.XDirection().XYZ());
    putXYZ(os, a.YDirection().XYZ());
}
gp_Ax3 getAx3(std::istream& is) {
    gp_Ax3 a;
    a.*member(Ax3Axis()) = getAx1(is);
    a.*member(Ax3X()) = getDir(is);
    a.*member(Ax3Y()) = getDir(is);
    return a;
}
gp_Ax2 getAx2(std::istream& is) {
    gp_Ax2 a;
    a.*member(Ax2Axis()) = getAx1(is);
    a.*member(Ax2X()) = getDir(is);
    a.*member(Ax2Y()) = getDir(is);
    return a;
}

void putAx2d(std::ostream& os, const gp_Ax2d& a) {
    putXY(os, a.Location().XY());
    putXY(os, a.Direction().XY());
}
gp_Ax2d getAx2d(std::istream& is) {
    gp_Ax2d a;
    a.*member(Ax2dLoc()) = gp_Pnt2d(getXY(is));
    a.*member(Ax2dDir()) = getDir2d(is);
    return a;
}
void putAx22d(std::ostream& os, const gp_Ax22d& a) {
    putXY(os, a.Location().XY());
    putXY(os, a.XDirection().XY());
    putXY(os, a.YDirection().XY());
}
gp_Ax22d getAx22d(std::istream& is) {
    gp_Ax22d a;
    a.*member(Ax22dPoint()) = gp_Pnt2d(getXY(is));
    a.*member(Ax22dX()) = getDir2d(is);
    a.*member(Ax22dY()) = getDir2d(is);
    return a;
}

void expectKind(std::istream& is, Kind found) {
    if (is.get() != found) throw Standard_Failure("exact brep: a geometry changed kind");
}

// --- 3D curves --------------------------------------------------------------

void putCurve(std::ostream& os, const occ::handle<Geom_Curve>& c) {
    const Kind k = kindIn(CURVE, c, true);
    os.put(static_cast<char>(k));
    if (k == FRAME) putAx1(os, occ::down_cast<Geom_Line>(c)->Position());
    if (k == CONIC) putFrame(os, occ::down_cast<Geom_Conic>(c)->Position());
    if (k == TRIMMED) putCurve(os, occ::down_cast<Geom_TrimmedCurve>(c)->BasisCurve());
    if (k == OFFSET) {
        const occ::handle<Geom_OffsetCurve> o = occ::down_cast<Geom_OffsetCurve>(c);
        putXYZ(os, o->Direction().XYZ());
        putCurve(os, o->BasisCurve());
    }
}

void restoreCurve(std::istream& is, const occ::handle<Geom_Curve>& c) {
    const Kind k = kindIn(CURVE, c, false);
    expectKind(is, k);
    if (k == FRAME) occ::down_cast<Geom_Line>(c)->SetPosition(getAx1(is));
    if (k == CONIC) occ::down_cast<Geom_Conic>(c)->SetPosition(getAx2(is));
    if (k == TRIMMED) restoreCurve(is, occ::down_cast<Geom_TrimmedCurve>(c)->BasisCurve());
    if (k == OFFSET) {
        const occ::handle<Geom_OffsetCurve> o = occ::down_cast<Geom_OffsetCurve>(c);
        const gp_Dir d = getDir(is);
        restoreCurve(is, o->BasisCurve());
        o->SetDirection(d);
    }
}

// --- 2D curves --------------------------------------------------------------

void putCurve2d(std::ostream& os, const occ::handle<Geom2d_Curve>& c) {
    const Kind k = kindIn(CURVE2D, c, true);
    os.put(static_cast<char>(k));
    if (k == FRAME) putAx2d(os, occ::down_cast<Geom2d_Line>(c)->Position());
    if (k == CONIC) putAx22d(os, occ::down_cast<Geom2d_Conic>(c)->Position());
    if (k == TRIMMED) putCurve2d(os, occ::down_cast<Geom2d_TrimmedCurve>(c)->BasisCurve());
    if (k == OFFSET) putCurve2d(os, occ::down_cast<Geom2d_OffsetCurve>(c)->BasisCurve());
}

void restoreCurve2d(std::istream& is, const occ::handle<Geom2d_Curve>& c) {
    const Kind k = kindIn(CURVE2D, c, false);
    expectKind(is, k);
    if (k == FRAME) occ::down_cast<Geom2d_Line>(c)->SetPosition(getAx2d(is));
    if (k == CONIC) occ::down_cast<Geom2d_Conic>(c)->SetAxis(getAx22d(is));
    if (k == TRIMMED) restoreCurve2d(is, occ::down_cast<Geom2d_TrimmedCurve>(c)->BasisCurve());
    if (k == OFFSET) restoreCurve2d(is, occ::down_cast<Geom2d_OffsetCurve>(c)->BasisCurve());
}

// --- surfaces ---------------------------------------------------------------

void putSurface(std::ostream& os, const occ::handle<Geom_Surface>& s) {
    const Kind k = kindIn(SURFACE, s, true);
    os.put(static_cast<char>(k));
    if (k == FRAME) putFrame(os, occ::down_cast<Geom_ElementarySurface>(s)->Position());
    if (k == EXTRUSION) {
        const occ::handle<Geom_SurfaceOfLinearExtrusion> e =
            occ::down_cast<Geom_SurfaceOfLinearExtrusion>(s);
        putXYZ(os, e->Direction().XYZ());
        putCurve(os, e->BasisCurve());
    }
    if (k == REVOLUTION) {
        const occ::handle<Geom_SurfaceOfRevolution> r = occ::down_cast<Geom_SurfaceOfRevolution>(s);
        putAx1(os, r->Axis());
        putCurve(os, r->BasisCurve());
    }
    if (k == OFFSET) putSurface(os, occ::down_cast<Geom_OffsetSurface>(s)->BasisSurface());
    if (k == TRIMMED) {
        putSurface(os, occ::down_cast<Geom_RectangularTrimmedSurface>(s)->BasisSurface());
    }
}

void restoreSurface(std::istream& is, const occ::handle<Geom_Surface>& s) {
    const Kind k = kindIn(SURFACE, s, false);
    expectKind(is, k);
    if (k == FRAME) occ::down_cast<Geom_ElementarySurface>(s)->SetPosition(getAx3(is));
    if (k == EXTRUSION) {
        const occ::handle<Geom_SurfaceOfLinearExtrusion> e =
            occ::down_cast<Geom_SurfaceOfLinearExtrusion>(s);
        const gp_Dir d = getDir(is);
        restoreCurve(is, e->BasisCurve());
        e->SetDirection(d);
    }
    if (k == REVOLUTION) {
        const occ::handle<Geom_SurfaceOfRevolution> r = occ::down_cast<Geom_SurfaceOfRevolution>(s);
        const gp_Ax1 a = getAx1(is);
        restoreCurve(is, r->BasisCurve());
        r->SetAxis(a);
    }
    if (k == OFFSET) {
        // What it derived from its basis when read (an equivalent surface, an
        // osculating one) is derived again from the exact basis, C0 checked.
        const occ::handle<Geom_OffsetSurface> o = occ::down_cast<Geom_OffsetSurface>(s);
        const occ::handle<Geom_Surface> basis = o->BasisSurface();
        restoreSurface(is, basis);
        o->SetBasisSurface(basis, false);
    }
    if (k == TRIMMED) {
        restoreSurface(is, occ::down_cast<Geom_RectangularTrimmedSurface>(s)->BasisSurface());
    }
}

// The tables' own counts: each is an indexed map, numbered from 1.
int countOf(BinTools_ShapeSet& set, int table) {
    if (table == 0) return (set.*member(SetSurfaces()).*member(SurfaceMap())).Extent();
    if (table == 1) return (set.*member(SetCurves()).*member(CurveMap())).Extent();
    return (set.*member(SetCurves2d()).*member(Curve2dMap())).Extent();
}

} // namespace

namespace kapy_exact {

void writeFrames(std::ostream& os, BinTools_ShapeSet& set) {
    BinTools_SurfaceSet& surfaces = set.*member(SetSurfaces());
    BinTools_CurveSet& curves = set.*member(SetCurves());
    BinTools_Curve2dSet& curves2d = set.*member(SetCurves2d());
    int n[3];
    for (int t = 0; t < 3; t++) {
        n[t] = countOf(set, t);
        BinTools::PutInteger(os, n[t]);
    }
    for (int i = 1; i <= n[0]; i++) putSurface(os, surfaces.Surface(i));
    for (int i = 1; i <= n[1]; i++) putCurve(os, curves.Curve(i));
    for (int i = 1; i <= n[2]; i++) putCurve2d(os, curves2d.Curve2d(i));
}

void restoreFrames(std::istream& is, BinTools_ShapeSet& set) {
    BinTools_SurfaceSet& surfaces = set.*member(SetSurfaces());
    BinTools_CurveSet& curves = set.*member(SetCurves());
    BinTools_Curve2dSet& curves2d = set.*member(SetCurves2d());
    int n[3];
    for (int t = 0; t < 3; t++) {
        BinTools::GetInteger(is, n[t]);
        if (!is.good() || n[t] != countOf(set, t)) {
            throw Standard_Failure("exact brep: the geometry tables do not match their frames");
        }
    }
    for (int i = 1; i <= n[0]; i++) restoreSurface(is, surfaces.Surface(i));
    for (int i = 1; i <= n[1]; i++) restoreCurve(is, curves.Curve(i));
    for (int i = 1; i <= n[2]; i++) restoreCurve2d(is, curves2d.Curve2d(i));
    if (!is.good()) throw Standard_Failure("exact brep: the frames ended early");
}

} // namespace kapy_exact
