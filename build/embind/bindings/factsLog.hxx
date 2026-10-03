// engine/kernels/occt/build/embind/bindings/factsLog.hxx
//
// The naming facts' binary channel, the kernel's half (D-N4): the ordered log
// every construction, release, binding and adoption is appended to, in the
// layout the Rust core reads (`engine/crates/kpy-core/src/topo/store/wire.rs`
// spells it; factsWire.ts is the TypeScript writer this one mirrors byte for
// byte). Little-endian, every payload padded to 8.
//
// There is ONE log per module. The embind collectors (factsEmbind.cpp) append
// to it as the TypeScript binding names shapes; the C API drains it
// (`kapy_facts_take`, factsCapi.cpp) into the result arena for Rust to read.
//
// Who includes it: facts*.cpp.
// What does NOT belong here: collecting facts (factsBuilders.cpp), JSON
// spelling (factsJson.hxx).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kapy_facts {

// One block of a construction record: its header `[kind, a, b, c, d]` and its
// payload bytes (padded to 8 when written).
struct Block {
    uint32_t head[5];
    std::vector<uint8_t> data;
};

// An ELEMENTS block (kind 1): the raw rows of the element props, `each` when
// they were read one element at a time.
Block elementsBlock(bool each, const std::vector<double>& faces,
                    const std::vector<double>& edges, const std::vector<double>& vertices);

// An INTS block (kind 2): an operand's history of one kind.
Block intsBlock(const std::vector<int>& xs);

// A construction named `id` from the JSON `skeleton` and its `blocks`,
// against the tables `previous` and `tool` (0: none).
void construct(uint32_t id, uint32_t previous, uint32_t tool, const std::string& skeleton,
               const std::vector<Block>& blocks);

// Table `id` is gone.
void release(uint32_t id);

// `handle` now carries table `id`.
void bind(uint32_t id, const std::string& handle);

// Table `id` is `text` (a table read back from a cache).
void adopt(uint32_t id, const std::string& text);

// Records written elsewhere (the host's own log), appended as they are.
void append(const uint8_t* bytes, size_t length);

// Everything written so far.
const std::vector<uint8_t>& bytes();

// The bytes of the record written last.
const uint8_t* lastData();
size_t lastSize();

// Empty the log.
void clear();

}  // namespace kapy_facts
