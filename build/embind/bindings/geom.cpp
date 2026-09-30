// services/occt/build/embind/bindings/geom.cpp
//
// The analytic geometry the helical sweep builds its wire from, plus the
// Handle_* wrappers around it.
//
// `Handle_X` is not an OCCT type: opencascade.js synthesises a class per
// handle it exposes, whose `get()` hands back the pointee. buildHelixWire.ts
// leans on that chain — `new Handle_Geom2d_Curve_2(trimmedHandle.get())`
// upcasts by passing the pointee — so the shape is reproduced here.
//
// MEMORY: every Geom_* is a Standard_Transient, i.e. reference-counted. A
// factory returning a RAW pointer hands embind an owning wrapper, and the
// TypeScript's `track(...)` calls `.delete()` on it — destroying an object
// that live handles still reference. That is a use-after-free, and it
// surfaced as `shellShape: memory access out of bounds`. So the factories
// return `occ::handle<T>` and JS only ever holds handles; `.delete()` then
// merely drops a reference. Building a handle from a raw `get()` pointer is
// safe: the constructor takes its own reference.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <Geom2d_Curve.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Surface.hxx>
#include <gp_Ax3.hxx>
#include <gp_Lin2d.hxx>

using namespace emscripten;

template <typename T>
using H = occ::handle<T>;

// One handle registration: `_2` adopts a pointer, `get()` returns it back.
template <typename T>
static T* handle_get(H<T>& h) {
    return h.get();
}
template <typename T>
static bool handle_is_null(const H<T>& h) {
    return h.IsNull();
}
template <typename T>
static void bind_handle(const char* name) {
    class_<H<T>>(name)
        .function("get", &handle_get<T>, allow_raw_pointers())
        .function("IsNull", &handle_is_null<T>);
}

// MakeEdge_30 lives here rather than in builders.cpp: it is the one
// constructor whose arguments are handles (a 2D curve on a surface), and
// it is what turns the helix's parametric line into a real edge.
struct BRepBuilderAPI_MakeEdge_30 : public BRepBuilderAPI_MakeEdge {
    BRepBuilderAPI_MakeEdge_30(const H<Geom2d_Curve>& c, const H<Geom_Surface>& s)
        : BRepBuilderAPI_MakeEdge(c, s) {}
};

EMSCRIPTEN_BINDINGS(kapy_occt_geom) {
    class_<Geom_Surface>("Geom_Surface");
    class_<Geom_CylindricalSurface, base<Geom_Surface>>("Geom_CylindricalSurface");
    function("Geom_CylindricalSurface_1", +[](const gp_Ax3& a, double r) {
        return H<Geom_CylindricalSurface>(new Geom_CylindricalSurface(a, r));
    });

    class_<Geom2d_Curve>("Geom2d_Curve");
    class_<Geom2d_Line, base<Geom2d_Curve>>("Geom2d_Line");
    function("Geom2d_Line_2",
             +[](const gp_Lin2d& l) { return H<Geom2d_Line>(new Geom2d_Line(l)); });

    class_<Geom2d_TrimmedCurve, base<Geom2d_Curve>>("Geom2d_TrimmedCurve");
    function("Geom2d_TrimmedCurve",
             +[](const H<Geom2d_Curve>& c, double u1, double u2, bool sense, bool adjustPeriodic) {
                 return H<Geom2d_TrimmedCurve>(
                     new Geom2d_TrimmedCurve(c, u1, u2, sense, adjustPeriodic));
             });

    bind_handle<Geom_Surface>("Handle_Geom_Surface");
    bind_handle<Geom_CylindricalSurface>("Handle_Geom_CylindricalSurface");
    bind_handle<Geom2d_Curve>("Handle_Geom2d_Curve");
    bind_handle<Geom2d_Line>("Handle_Geom2d_Line");
    bind_handle<Geom2d_TrimmedCurve>("Handle_Geom2d_TrimmedCurve");

    function("Handle_Geom_Surface_2", +[](Geom_Surface* p) { return H<Geom_Surface>(p); },
             allow_raw_pointers());
    // Called with a handle (the factory's result), not a raw pointer.
    function("Handle_Geom_CylindricalSurface_2",
             +[](const H<Geom_CylindricalSurface>& h) { return h; });
    function("Handle_Geom2d_Curve_2", +[](Geom2d_Curve* p) { return H<Geom2d_Curve>(p); },
             allow_raw_pointers());
    function("Handle_Geom2d_Line_2", +[](const H<Geom2d_Line>& h) { return h; });
    function("Handle_Geom2d_TrimmedCurve_2",
             +[](const H<Geom2d_TrimmedCurve>& h) { return h; });

    class_<BRepBuilderAPI_MakeEdge_30, base<BRepBuilderAPI_MakeEdge>>("BRepBuilderAPI_MakeEdge_30")
        .constructor<const H<Geom2d_Curve>&, const H<Geom_Surface>&>();
}
