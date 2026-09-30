// services/occt/build/embind/bindings/capiBlob.hxx
//
// How a C build operation reads its arguments: one little-endian blob the
// Rust core wrote (`engine/crates/kpy-core/src/kernel/capi_build/`), read
// front to back with the same four primitives it was written with. The kernel
// parses NO text: every decision (which way a loop runs, what a section's
// offset is, every sine and cosine) arrives as a number, so the C++ only
// sequences OCCT calls.
//
// A blob that ends early or carries a length that runs past its end is a
// caller bug, not a geometry failure: it throws `BlobError`, which the entry
// points turn into `KAPY_E_BAD_ARG`.
//
// Who includes it: capiProfile.*, capiPrism.cpp, capiLoft.cpp, capiPush.cpp.
// What does NOT belong here: what any field means.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace kapy_capi {

// A blob that does not hold what its reader asked for.
struct BlobError : std::runtime_error {
    BlobError() : std::runtime_error("malformed argument blob") {}
};

// A reader over `length` bytes at `bytes`; nothing is copied until a field is
// read.
class Blob {
public:
    Blob(const void* bytes, size_t length)
        : p_(static_cast<const uint8_t*>(bytes)), end_(p_ + length) {}

    uint8_t u8() {
        need(1);
        return *p_++;
    }

    uint32_t u32() {
        need(4);
        uint32_t v;
        std::memcpy(&v, p_, 4);
        p_ += 4;
        return v;
    }

    double f64() {
        need(8);
        double v;
        std::memcpy(&v, p_, 8);
        p_ += 8;
        return v;
    }

    // A length-prefixed UTF-8 string.
    std::string str() {
        const uint32_t n = u32();
        need(n);
        std::string s(reinterpret_cast<const char*>(p_), n);
        p_ += n;
        return s;
    }

    // Whether every byte was read.
    bool done() const { return p_ == end_; }

private:
    void need(size_t n) const {
        if (static_cast<size_t>(end_ - p_) < n) throw BlobError();
    }

    const uint8_t* p_;
    const uint8_t* end_;
};

}  // namespace kapy_capi
