// engine/kernels/occt/build/embind/bindings/factsJson.hxx
//
// The few JSON spellings the facts' skeletons need, written the way
// `JSON.stringify` writes them so the core reads the same values: strings
// escaped, doubles as the shortest text that reads back bit for bit, a
// negative zero kept as `-0` (the core reads it back as -0; `JSON.stringify`
// alone writes 0 — factsWire.ts' `signedJson`), and a number that JSON cannot
// hold as `null`.
//
// Who includes it: factsBuilders.cpp, factsRoles.cpp.
// What does NOT belong here: the skeletons themselves.

#pragma once

#include <charconv>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace kapy_facts {

// A JSON string literal of `s`.
inline std::string jstr(const std::string& s) {
    std::string out = "\"";
    for (const unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

// A JSON number of `x`.
inline std::string jnum(double x) {
    if (!std::isfinite(x)) return "null";
    if (x == 0) return std::signbit(x) ? "-0" : "0";
    char buf[40];
    const auto r = std::to_chars(buf, buf + sizeof buf, x);
    return std::string(buf, r.ptr);
}

// A JSON array of the three doubles at `p`.
inline std::string jvec3(const double* p) {
    return "[" + jnum(p[0]) + "," + jnum(p[1]) + "," + jnum(p[2]) + "]";
}

// A JSON array of integers.
inline std::string jints(const std::vector<int>& xs) {
    std::string out = "[";
    for (size_t i = 0; i < xs.size(); ++i) out += (i ? "," : "") + std::to_string(xs[i]);
    return out + "]";
}

// A JSON boolean.
inline const char* jbool(bool b) { return b ? "true" : "false"; }

}  // namespace kapy_facts
