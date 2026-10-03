// engine/kernels/occt/build/embind/bindings/capiClassify.hxx
//
// Where a point sits against a body, as the binding asks it
// (binding/pointClassify.ts): one BRepClass3d_SolidClassifier per solid inside
// the shape, the strongest state across them winning (IN > ON > OUT >
// UNKNOWN), IN settling it at once. A shape with no solid inside it is
// classified as it stands, and a classifier that fails there is the state
// 'error', which callers must not read as OUT.
//
// States cross the boundary as one byte: 0 UNKNOWN, 1 OUT, 2 ON, 3 IN, 4 error.
//
// Who includes it: capiClassify.cpp, capiContact.cpp.
// What does NOT belong here: what a caller makes of the state.

#pragma once

#include <cstdint>

#include <BRepClass3d_SolidClassifier.hxx>
#include <TopAbs_State.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

namespace kapy_capi {

// The classifier's answer as a byte.
enum PointState : uint8_t { STATE_UNKNOWN = 0, STATE_OUT = 1, STATE_ON = 2, STATE_IN = 3, STATE_ERROR = 4 };

// What a probe found: the state and how many solids were tested.
struct PointProbe {
    uint8_t state = STATE_UNKNOWN;
    uint32_t solids = 0;
};

inline uint8_t stateOf(TopAbs_State s) {
    if (s == TopAbs_IN) return STATE_IN;
    if (s == TopAbs_OUT) return STATE_OUT;
    if (s == TopAbs_ON) return STATE_ON;
    return STATE_UNKNOWN;
}

// Classify `(x, y, z)` against `body`. An exception inside a solid's
// classification propagates, as it does in the binding.
inline PointProbe probePoint(const TopoDS_Shape& body, double x, double y, double z) {
    const gp_Pnt point(x, y, z);
    PointProbe probe;
    for (TopExp_Explorer ex(body, TopAbs_SOLID); ex.More(); ex.Next()) {
        probe.solids++;
        BRepClass3d_SolidClassifier classifier(ex.Current(), point, 1e-6);
        const uint8_t tag = stateOf(classifier.State());
        if (tag == STATE_IN) {
            probe.state = STATE_IN;
            break;
        }
        if (tag == STATE_ON && (probe.state == STATE_OUT || probe.state == STATE_UNKNOWN)) {
            probe.state = STATE_ON;
        } else if (tag == STATE_OUT && probe.state == STATE_UNKNOWN) {
            probe.state = STATE_OUT;
        }
    }
    if (probe.solids == 0) {
        try {
            BRepClass3d_SolidClassifier classifier(body, point, 1e-6);
            probe.state = stateOf(classifier.State());
        } catch (...) {
            probe.state = STATE_ERROR;
        }
    }
    return probe;
}

}  // namespace kapy_capi
