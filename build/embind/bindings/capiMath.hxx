// engine/kernels/occt/build/embind/bindings/capiMath.hxx
//
// `Math.hypot`, as V8 computes it, for the few numbers the kernel normalises
// itself: scale by the largest, Kahan-sum the squares, `sqrt`, scale back. The
// platform's `hypot` differs in the last bit and the TypeScript binding the
// answers were first written against used V8's, so a direction the kernel
// derives (the radial of a cylinder's face) is computed with the same algorithm
// as the Rust side's `capi_build/jsmath.rs`.
//
// Who includes it: capiShellFacts.cpp.
// What does NOT belong here: anything that is not a JavaScript builtin.

#pragma once

#include <cmath>
#include <cstddef>
#include <limits>

namespace kapy_capi {

// `Math.hypot(v[0], ..., v[n - 1])`.
inline double jsHypot(const double* v, size_t n) {
    double largest = 0.0;
    bool nan = false;
    for (size_t i = 0; i < n; ++i) {
        if (std::isnan(v[i])) {
            nan = true;
        } else if (std::fabs(v[i]) > largest) {
            largest = std::fabs(v[i]);
        }
    }
    if (largest == std::numeric_limits<double>::infinity()) return largest;
    if (nan) return std::numeric_limits<double>::quiet_NaN();
    if (largest == 0.0) return 0.0;
    double sum = 0.0;
    double compensation = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double scaled = std::fabs(v[i]) / largest;
        const double summand = scaled * scaled - compensation;
        const double preliminary = sum + summand;
        compensation = (preliminary - sum) - summand;
        sum = preliminary;
    }
    return std::sqrt(sum) * largest;
}

}  // namespace kapy_capi
