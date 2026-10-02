// services/occt/build/embind/bindings/capiAsk.hxx
//
// The shell the C questions run in: one blob in (the handles first, then
// whatever the question needs), one blob out in the result arena. A question
// mints nothing and stores nothing, so there is no memo; it numbers the
// kernel's serials from the seed of the binding method it stands for, every
// time, as the binding's own call does.
//
// A handle the store does not hold is the unknown-handle failure (code -1,
// worded as the binding words it). Anything OCCT or the standard library
// raises while the kernel works is a DECLINE (`KAPY_E_DECLINED`): the host
// redoes the question over JSON, which raises or words it the way it always
// did. The lookup is outside the decline on purpose, so an unknown handle
// stays an unknown handle.
//
// Who includes it: the capi*.cpp question files.
// What does NOT belong here: what any question measures.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "capiOp.hxx"

namespace kapy_capi {

// A growing answer: the little-endian mirror of `Blob`, for questions whose
// answer is more than one value.
class Out {
public:
    void u8(uint8_t v) { bytes_.push_back(v); }

    void u32(uint32_t v) { raw(&v, 4); }

    void f64(double v) { raw(&v, 8); }

    void f32(float v) { raw(&v, 4); }

    // `n` bytes as they are in memory (an array of the little-endian values the
    // other writers write one by one).
    void bytes(const void* p, size_t n) { raw(p, n); }

    const std::vector<uint8_t>& bytes() const { return bytes_; }

    // Answer these bytes (an empty answer is "nothing").
    int32_t send() const {
        return bytes_.empty() ? answerNothing() : answer(bytes_.data(), bytes_.size());
    }

private:
    void raw(const void* p, size_t n) {
        const uint8_t* b = static_cast<const uint8_t*>(p);
        bytes_.insert(bytes_.end(), b, b + n);
    }

    std::vector<uint8_t> bytes_;
};

// Run `body(Blob&, Out&)` as the question `name` seeded with `seed`, and answer
// what it wrote. `body` reads its own handles with `need()` first (outside any
// decline), then does the kernel's work inside `declining(name, ...)`.
template <typename Body>
int32_t ask(const char* name, size_t seed, uint32_t ptr, uint32_t length, Body&& body) {
    return runRaw(name, seed, ptr, length, [&](Blob& in) -> int32_t {
        Out out;
        body(in, out);
        return out.send();
    });
}

}  // namespace kapy_capi
