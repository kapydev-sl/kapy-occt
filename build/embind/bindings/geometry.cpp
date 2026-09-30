// services/occt/build/embind/bindings/geometry.cpp
//
// The gp_* value types: points, directions, axes, transforms and the 2D
// primitives the sketch layer builds curves from.
//
// These are copyable value classes, so extra constructor overloads can be
// registered as free factories named `Class_N`. C++ overload resolution then
// picks the right OCCT constructor from the factory's argument types — which
// is why the ctor renumbering OCCT 8.0 introduced (gp_Dir_4, gp_Dir2d_4,
// gp_Ax2_3 all moved one slot; see ../../compare-overloads.py) costs us
// nothing: the NAME is ours, only the argument types must be right.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Dir2d.hxx>
#include <gp_Elips.hxx>
#include <gp_Lin2d.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

using namespace emscripten;

// `Location`/`Axis`/`Direction` return const references into the object;
// embind copies the return, and these are all cheap value types.
static gp_Pnt Ax1_Location(const gp_Ax1& a) { return a.Location(); }
static gp_Dir Ax1_Direction(const gp_Ax1& a) { return a.Direction(); }
static gp_Pnt Ax2_Location(const gp_Ax2& a) { return a.Location(); }
static gp_Dir Ax2_Direction(const gp_Ax2& a) { return a.Direction(); }
static gp_Ax1 Ax2_Axis(const gp_Ax2& a) { return a.Axis(); }
static gp_Pnt Ax3_Location(const gp_Ax3& a) { return a.Location(); }
static gp_Dir Ax3_Direction(const gp_Ax3& a) { return a.Direction(); }
static gp_Ax1 Ax3_Axis(const gp_Ax3& a) { return a.Axis(); }
static gp_Ax1 Circ_Axis(const gp_Circ& c) { return c.Axis(); }
static gp_Pnt Circ_Location(const gp_Circ& c) { return c.Location(); }
static gp_Ax1 Elips_Axis(const gp_Elips& e) { return e.Axis(); }
static gp_Pnt Elips_Location(const gp_Elips& e) { return e.Location(); }
static gp_Pnt2d Lin2d_Location(const gp_Lin2d& l) { return l.Location(); }
static gp_Dir2d Lin2d_Direction(const gp_Lin2d& l) { return l.Direction(); }

// gp_Trsf setters are overloaded; opencascade.js numbers the first-declared
// overload `_1` in each case.
static void Trsf_SetRotation_1(gp_Trsf& t, const gp_Ax1& a, double ang) { t.SetRotation(a, ang); }
static void Trsf_SetTranslation_1(gp_Trsf& t, const gp_Vec& v) { t.SetTranslation(v); }
static double Trsf_Value(const gp_Trsf& t, int r, int c) { return t.Value(r, c); }
// Mirroring is capability-gated in the app (workers/occt/capabilities.ts looks
// for any SetMirror_* on the prototype), so all three overloads are bound.
static void Trsf_SetMirror_1(gp_Trsf& t, const gp_Pnt& p) { t.SetMirror(p); }
static void Trsf_SetMirror_2(gp_Trsf& t, const gp_Ax1& a) { t.SetMirror(a); }
static void Trsf_SetMirror_3(gp_Trsf& t, const gp_Ax2& a) { t.SetMirror(a); }

EMSCRIPTEN_BINDINGS(kapy_occt_geometry) {
    class_<gp_Pnt>("gp_Pnt")
        .constructor<>()
        .function("X", &gp_Pnt::X)
        .function("Y", &gp_Pnt::Y)
        .function("Z", &gp_Pnt::Z)
        .function("Transformed", &gp_Pnt::Transformed);
    function("gp_Pnt_1", +[]() { return gp_Pnt(); });
    function("gp_Pnt_3", +[](double x, double y, double z) { return gp_Pnt(x, y, z); });

    class_<gp_Pnt2d>("gp_Pnt2d")
        .constructor<>()
        .function("X", &gp_Pnt2d::X)
        .function("Y", &gp_Pnt2d::Y)
        .function("Transformed", &gp_Pnt2d::Transformed);
    function("gp_Pnt2d_3", +[](double x, double y) { return gp_Pnt2d(x, y); });

    class_<gp_Dir>("gp_Dir")
        .constructor<>()
        .function("X", &gp_Dir::X)
        .function("Y", &gp_Dir::Y)
        .function("Z", &gp_Dir::Z)
        .function("Reversed", &gp_Dir::Reversed)
        .function("Transformed", &gp_Dir::Transformed);
    function("gp_Dir_4", +[](double x, double y, double z) { return gp_Dir(x, y, z); });

    class_<gp_Dir2d>("gp_Dir2d")
        .constructor<>()
        .function("X", &gp_Dir2d::X)
        .function("Y", &gp_Dir2d::Y)
        .function("Reversed", &gp_Dir2d::Reversed);
    function("gp_Dir2d_4", +[](double x, double y) { return gp_Dir2d(x, y); });

    class_<gp_Vec>("gp_Vec")
        .constructor<>()
        .function("X", &gp_Vec::X)
        .function("Y", &gp_Vec::Y)
        .function("Z", &gp_Vec::Z)
        .function("Reversed", &gp_Vec::Reversed)
        .function("Transformed", &gp_Vec::Transformed);
    function("gp_Vec_1", +[]() { return gp_Vec(); });
    function("gp_Vec_4", +[](double x, double y, double z) { return gp_Vec(x, y, z); });

    class_<gp_Trsf>("gp_Trsf")
        .constructor<>()
        .function("SetRotation_1", &Trsf_SetRotation_1)
        .function("SetTranslation_1", &Trsf_SetTranslation_1)
        .function("SetScale", &gp_Trsf::SetScale)
        .function("SetMirror_1", &Trsf_SetMirror_1)
        .function("SetMirror_2", &Trsf_SetMirror_2)
        .function("SetMirror_3", &Trsf_SetMirror_3)
        .function("Value", &Trsf_Value);
    function("gp_Trsf_1", +[]() { return gp_Trsf(); });

    class_<gp_Ax1>("gp_Ax1")
        .constructor<>()
        .function("Location", &Ax1_Location)
        .function("Direction", &Ax1_Direction)
        .function("Reversed", &gp_Ax1::Reversed);
    function("gp_Ax1_2", +[](const gp_Pnt& p, const gp_Dir& d) { return gp_Ax1(p, d); });

    class_<gp_Ax2>("gp_Ax2")
        .constructor<>()
        .function("Location", &Ax2_Location)
        .function("Direction", &Ax2_Direction)
        .function("Axis", &Ax2_Axis);
    function("gp_Ax2_2", +[](const gp_Pnt& p, const gp_Dir& n, const gp_Dir& vx) {
        return gp_Ax2(p, n, vx);
    });
    function("gp_Ax2_3", +[](const gp_Pnt& p, const gp_Dir& v) { return gp_Ax2(p, v); });

    class_<gp_Ax3>("gp_Ax3")
        .constructor<>()
        .function("Location", &Ax3_Location)
        .function("Direction", &Ax3_Direction)
        .function("Axis", &Ax3_Axis);
    function("gp_Ax3_3", +[](const gp_Pnt& p, const gp_Dir& n, const gp_Dir& vx) {
        return gp_Ax3(p, n, vx);
    });

    class_<gp_Circ>("gp_Circ")
        .constructor<>()
        .function("Radius", select_overload<double() const>(&gp_Circ::Radius))
        .function("Axis", &Circ_Axis)
        .function("Location", &Circ_Location);
    function("gp_Circ_2", +[](const gp_Ax2& a, double r) { return gp_Circ(a, r); });

    class_<gp_Elips>("gp_Elips")
        .constructor<>()
        .function("Axis", &Elips_Axis)
        .function("Location", &Elips_Location);
    function("gp_Elips_2",
             +[](const gp_Ax2& a, double major, double minor) { return gp_Elips(a, major, minor); });

    class_<gp_Lin2d>("gp_Lin2d")
        .constructor<>()
        .function("Location", &Lin2d_Location)
        .function("Direction", &Lin2d_Direction);
    function("gp_Lin2d_3", +[](const gp_Pnt2d& p, const gp_Dir2d& d) { return gp_Lin2d(p, d); });
}
