// services/occt/build/embind/bindings/measure.cpp
//
// Measurement and validity: bounding boxes, mass properties, distances,
// point-in-solid classification and shape checking.
//
// Overload numbers here were read off the OCCT 8.0 headers and cross-checked
// against the 7.7 typings by ../compare-overloads.py:
//   Bnd_Box::Add  -> _1 (Bnd_Box), _2 (gp_Pnt)
//   Bnd_Box::IsOut -> _4 (Bnd_Box), the box-vs-box test the code uses
//   BRepCheck_Analyzer::IsValid -> _2 (no argument; _1 takes a shape)
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <Extrema_ExtAlgo.hxx>
#include <Extrema_ExtFlag.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Message_ProgressRange.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

using namespace emscripten;

static void Box_Add_1(Bnd_Box& b, const Bnd_Box& o) { b.Add(o); }
static void Box_Add_2(Bnd_Box& b, const gp_Pnt& p) { b.Add(p); }
static bool Box_IsOut_4(const Bnd_Box& b, const Bnd_Box& o) { return b.IsOut(o); }

// BRepBndLib is a package of statics: `oc.BRepBndLib.Add(shape, box, tri)`.
struct BRepBndLib_Package {};
static void BndLib_Add(const TopoDS_Shape& s, Bnd_Box& b, bool useTriangulation) {
    BRepBndLib::Add(s, b, useTriangulation);
}

// BRepGProp likewise. Bind the FULL arity: embind silently drops surplus
// arguments on a free function, so a 2-parameter binding would let
// `VolumeProperties_1(shape, props, true, false, false)` run with
// OnlyClosed = false — a wrong volume, with no error anywhere.
struct BRepGProp_Package {};
static void GProp_Linear(const TopoDS_Shape& s, GProp_GProps& g, bool skipShared,
                         bool useTriangulation) {
    BRepGProp::LinearProperties(s, g, skipShared, useTriangulation);
}
static void GProp_Surface_1(const TopoDS_Shape& s, GProp_GProps& g, bool skipShared,
                            bool useTriangulation) {
    BRepGProp::SurfaceProperties(s, g, skipShared, useTriangulation);
}
static void GProp_Volume_1(const TopoDS_Shape& s, GProp_GProps& g, bool onlyClosed,
                           bool skipShared, bool useTriangulation) {
    BRepGProp::VolumeProperties(s, g, onlyClosed, skipShared, useTriangulation);
}

static gp_Pnt Props_CentreOfMass(const GProp_GProps& g) { return g.CentreOfMass(); }

static bool Dist_Perform(BRepExtrema_DistShapeShape& d, const Message_ProgressRange& r) {
    return d.Perform(r);
}

// The two knobs that decide how much work a distance costs. MIN drops the
// far-point search, and the Tree algorithm beats the gradient one on the
// tessellated shapes we hand it. Both are inline setters on the class, and
// both have to be set before Perform.
static void Dist_SetFlag(BRepExtrema_DistShapeShape& d, Extrema_ExtFlag f) { d.SetFlag(f); }
static void Dist_SetAlgo(BRepExtrema_DistShapeShape& d, Extrema_ExtAlgo a) { d.SetAlgo(a); }

// State() is inherited from BRepClass3d_SClassifier.
static TopAbs_State Classifier_State(BRepClass3d_SolidClassifier& c) { return c.State(); }
struct BRepClass3d_SolidClassifier_3 : public BRepClass3d_SolidClassifier {
    BRepClass3d_SolidClassifier_3(const TopoDS_Shape& s, const gp_Pnt& p, double tol)
        : BRepClass3d_SolidClassifier(s, p, tol) {}
};

// `IsValid()` (no argument) is the second declared overload.
static bool Analyzer_IsValid_2(const BRepCheck_Analyzer& a) { return a.IsValid(); }

EMSCRIPTEN_BINDINGS(kapy_occt_measure) {
    class_<Bnd_Box>("Bnd_Box")
        .constructor<>()
        .function("IsVoid", &Bnd_Box::IsVoid)
        .function("SetGap", &Bnd_Box::SetGap)
        .function("CornerMin", &Bnd_Box::CornerMin)
        .function("CornerMax", &Bnd_Box::CornerMax)
        .function("Add_1", &Box_Add_1)
        .function("Add_2", &Box_Add_2)
        .function("IsOut_4", &Box_IsOut_4);
    function("Bnd_Box_1", +[]() { return Bnd_Box(); });

    class_<BRepBndLib_Package>("BRepBndLib").class_function("Add", &BndLib_Add);

    class_<GProp_GProps>("GProp_GProps")
        .constructor<>()
        .function("Mass", &GProp_GProps::Mass)
        .function("CentreOfMass", &Props_CentreOfMass);
    function("GProp_GProps_1", +[]() { return GProp_GProps(); });

    class_<BRepGProp_Package>("BRepGProp")
        .class_function("LinearProperties", &GProp_Linear)
        .class_function("SurfaceProperties_1", &GProp_Surface_1)
        .class_function("VolumeProperties_1", &GProp_Volume_1);

    class_<BRepExtrema_DistShapeShape>("BRepExtrema_DistShapeShape")
        .constructor<>()
        .function("LoadS1", &BRepExtrema_DistShapeShape::LoadS1)
        .function("LoadS2", &BRepExtrema_DistShapeShape::LoadS2)
        .function("Perform", &Dist_Perform)
        .function("SetFlag", &Dist_SetFlag)
        .function("SetAlgo", &Dist_SetAlgo)
        .function("IsDone", &BRepExtrema_DistShapeShape::IsDone)
        .function("Value", &BRepExtrema_DistShapeShape::Value);
    function("BRepExtrema_DistShapeShape_1", +[]() { return BRepExtrema_DistShapeShape(); });

    class_<BRepClass3d_SolidClassifier>("BRepClass3d_SolidClassifier")
        .function("State", &Classifier_State);
    class_<BRepClass3d_SolidClassifier_3, base<BRepClass3d_SolidClassifier>>(
        "BRepClass3d_SolidClassifier_3")
        .constructor<const TopoDS_Shape&, const gp_Pnt&, double>();

    // OCCT 8.0 added a fourth ctor parameter (theIsExact, defaulted). The
    // 7.7 typings — and every call site — pass three, so bind three.
    class_<BRepCheck_Analyzer>("BRepCheck_Analyzer")
        .constructor<const TopoDS_Shape&, bool, bool>()
        .function("IsValid_2", &Analyzer_IsValid_2);
}
