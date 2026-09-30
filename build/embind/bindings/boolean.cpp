// services/occt/build/embind/bindings/boolean.cpp
//
// The boolean algorithms and the shape-history methods the persistent
// naming (TopoNamer) is built on: Modified / Generated / IsDeleted.
//
// These are THE naming-critical bindings — propagationSelfTest exercises
// them at boot. In OCCT 8.0 all three are single, public overloads on
// BRepAlgoAPI_BuilderAlgo, returning `const NCollection_List<TopoDS_Shape>&`
// (7.7 spelled the same type `TopTools_ListOfShape`). They take no `_N`
// suffix, matching the unsuffixed `.Modified(` / `.Generated(` call sites.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BOPAlgo_GlueEnum.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <Message_ProgressRange.hxx>
#include <NCollection_List.hxx>
#include <TopExp.hxx>
#include <TopoDS_Shape.hxx>

using namespace emscripten;

using ShapeList = NCollection_List<TopoDS_Shape>;
using ShapeIndexedMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

// Copy the history list out: embind cannot own OCCT's internal reference,
// and the JS side calls .delete() on what it gets back.
template <typename Algo>
static ShapeList history_modified(Algo& a, const TopoDS_Shape& s) {
    return a.Modified(s);
}
template <typename Algo>
static ShapeList history_generated(Algo& a, const TopoDS_Shape& s) {
    return a.Generated(s);
}
template <typename Algo>
static bool history_is_deleted(Algo& a, const TopoDS_Shape& s) {
    return a.IsDeleted(s);
}

// `Build` and `IsDone` are declared on base classes, so a derived-class
// member pointer would register against a type JS never sees.
template <typename Algo>
static void algo_build(Algo& a, const Message_ProgressRange& r) {
    a.Build(r);
}
template <typename Algo>
static bool algo_is_done(Algo& a) {
    return a.IsDone();
}
template <typename Algo>
static TopoDS_Shape algo_shape(Algo& a) {
    return a.Shape();
}

// The options runBoolean sets before Build(). All are declared on the bases
// (BRepAlgoAPI_BuilderAlgo / _Algo / BOPAlgo_Options), so they need wrapping
// like everything else inherited — and omitting them does not fail the
// build, it fails at runtime.
template <typename Algo>
static void algo_set_arguments(Algo& a, const ShapeList& l) {
    a.SetArguments(l);
}
template <typename Algo>
static void algo_set_tools(Algo& a, const ShapeList& l) {
    a.SetTools(l);
}
template <typename Algo>
static void algo_set_fuzzy(Algo& a, double v) {
    a.SetFuzzyValue(v);
}
template <typename Algo>
static void algo_set_run_parallel(Algo& a, bool v) {
    a.SetRunParallel(v);
}
template <typename Algo>
static void algo_set_use_obb(Algo& a, bool v) {
    a.SetUseOBB(v);
}
template <typename Algo>
static void algo_set_check_inverted(Algo& a, bool v) {
    a.SetCheckInverted(v);
}
// Without this the operator is free to inflate tolerances and attach pcurves
// on its ARGUMENTS in place. Our operands are long-lived handles reused by the
// next fold, the op memo and the regen prefix cache, so they must survive a
// boolean unchanged.
template <typename Algo>
static void algo_set_non_destructive(Algo& a, bool v) {
    a.SetNonDestructive(v);
}
template <typename Algo>
static void algo_set_glue(Algo& a, BOPAlgo_GlueEnum g) {
    a.SetGlue(g);
}

// One registration per boolean operator; each gets the same member set.
template <typename Algo>
static class_<Algo, base<BRepBuilderAPI_MakeShape>> bind_boolean(const char* name) {
    return class_<Algo, base<BRepBuilderAPI_MakeShape>>(name)
        .function("Build", &algo_build<Algo>)
        .function("IsDone", &algo_is_done<Algo>)
        .function("Shape", &algo_shape<Algo>)
        .function("Modified", &history_modified<Algo>)
        .function("Generated", &history_generated<Algo>)
        .function("IsDeleted", &history_is_deleted<Algo>)
        .function("SetArguments", &algo_set_arguments<Algo>)
        .function("SetTools", &algo_set_tools<Algo>)
        .function("SetFuzzyValue", &algo_set_fuzzy<Algo>)
        .function("SetRunParallel", &algo_set_run_parallel<Algo>)
        .function("SetUseOBB", &algo_set_use_obb<Algo>)
        .function("SetCheckInverted", &algo_set_check_inverted<Algo>)
        .function("SetNonDestructive", &algo_set_non_destructive<Algo>)
        .function("SetGlue", &algo_set_glue<Algo>);
}

// OCCT algorithms are non-copyable, so a factory returning one by value will
// not compile (embind copies the return). Each numbered overload therefore
// becomes a derived struct registered under the opencascade.js name — the
// same trick its generator emits (src/bindings.py, processOverloadedConstructors).
struct Message_ProgressRange_1 : public Message_ProgressRange {
    Message_ProgressRange_1() : Message_ProgressRange() {}
};
struct BRepAlgoAPI_Cut_3 : public BRepAlgoAPI_Cut {
    BRepAlgoAPI_Cut_3(const TopoDS_Shape& a, const TopoDS_Shape& b, const Message_ProgressRange& r)
        : BRepAlgoAPI_Cut(a, b, r) {}
};
// `_1` is the DEFAULT constructor on all three — runBoolean builds empty and
// then feeds SetArguments/SetTools. Only Cut takes the 3-argument `_3`.
struct BRepAlgoAPI_Cut_1 : public BRepAlgoAPI_Cut {
    BRepAlgoAPI_Cut_1() : BRepAlgoAPI_Cut() {}
};
struct BRepAlgoAPI_Fuse_1 : public BRepAlgoAPI_Fuse {
    BRepAlgoAPI_Fuse_1() : BRepAlgoAPI_Fuse() {}
};
struct BRepAlgoAPI_Common_1 : public BRepAlgoAPI_Common {
    BRepAlgoAPI_Common_1() : BRepAlgoAPI_Common() {}
};

// `oc.TopExp.MapShapes_1(shape, enum, map)`.
struct TopExp_Package {};
static void TopExp_MapShapes_1(const TopoDS_Shape& s, TopAbs_ShapeEnum t, ShapeIndexedMap& m) {
    TopExp::MapShapes(s, t, m);
}

EMSCRIPTEN_BINDINGS(kapy_occt_boolean) {
    class_<Message_ProgressRange>("Message_ProgressRange").constructor<>();
    class_<Message_ProgressRange_1, base<Message_ProgressRange>>("Message_ProgressRange_1")
        .constructor<>();

    bind_boolean<BRepAlgoAPI_Cut>("BRepAlgoAPI_Cut");
    bind_boolean<BRepAlgoAPI_Fuse>("BRepAlgoAPI_Fuse");
    bind_boolean<BRepAlgoAPI_Common>("BRepAlgoAPI_Common");

    class_<BRepAlgoAPI_Cut_3, base<BRepAlgoAPI_Cut>>("BRepAlgoAPI_Cut_3")
        .constructor<const TopoDS_Shape&, const TopoDS_Shape&, const Message_ProgressRange&>();
    class_<BRepAlgoAPI_Cut_1, base<BRepAlgoAPI_Cut>>("BRepAlgoAPI_Cut_1").constructor<>();
    class_<BRepAlgoAPI_Fuse_1, base<BRepAlgoAPI_Fuse>>("BRepAlgoAPI_Fuse_1").constructor<>();
    class_<BRepAlgoAPI_Common_1, base<BRepAlgoAPI_Common>>("BRepAlgoAPI_Common_1").constructor<>();

    class_<TopExp_Package>("TopExp").class_function("MapShapes_1", &TopExp_MapShapes_1);
}
