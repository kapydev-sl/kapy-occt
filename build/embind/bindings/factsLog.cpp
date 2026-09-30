// services/occt/build/embind/bindings/factsLog.cpp
//
// The facts log's writer (factsLog.hxx). The record layouts are the ones
// `topo/store/wire.rs` reads: a 24-byte header of six u32, a payload padded to
// 8, and for a construction its blocks after the skeleton.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: what a fact means, or collecting one.

#include "factsLog.hxx"

#include <cstring>

namespace kapy_facts {

namespace {

constexpr uint32_t kTagConstruct = 1;
constexpr uint32_t kTagRelease = 2;
constexpr uint32_t kTagBind = 3;
constexpr uint32_t kTagAdopt = 4;
constexpr uint32_t kBlockElements = 1;
constexpr uint32_t kBlockInts = 2;

std::vector<uint8_t> g_log;
size_t g_lastStart = 0;

size_t pad8(size_t n) { return (n + 7) & ~static_cast<size_t>(7); }

void put32(uint32_t v) {
    const uint8_t b[4] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8),
                          static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24)};
    g_log.insert(g_log.end(), b, b + 4);
}

void head(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f) {
    put32(a);
    put32(b);
    put32(c);
    put32(d);
    put32(e);
    put32(f);
}

// `n` bytes at `p`, then zeros up to the next multiple of 8.
void payload(const uint8_t* p, size_t n) {
    g_log.insert(g_log.end(), p, p + n);
    g_log.insert(g_log.end(), pad8(n) - n, 0);
}

template <typename T>
std::vector<uint8_t> raw(const std::vector<T>& xs) {
    std::vector<uint8_t> out(xs.size() * sizeof(T));
    if (!out.empty()) std::memcpy(out.data(), xs.data(), out.size());
    return out;
}

}  // namespace

Block elementsBlock(bool each, const std::vector<double>& faces,
                    const std::vector<double>& edges, const std::vector<double>& vertices) {
    Block b{{kBlockElements, each ? 1u : 0u, static_cast<uint32_t>(faces.size() / 9),
             static_cast<uint32_t>(edges.size() / 16), static_cast<uint32_t>(vertices.size() / 3)},
            {}};
    for (const auto* part : {&faces, &edges, &vertices}) {
        const std::vector<uint8_t> bytes = raw(*part);
        b.data.insert(b.data.end(), bytes.begin(), bytes.end());
    }
    return b;
}

Block intsBlock(const std::vector<int>& xs) {
    return Block{{kBlockInts, static_cast<uint32_t>(xs.size()), 0, 0, 0}, raw(xs)};
}

void construct(uint32_t id, uint32_t previous, uint32_t tool, const std::string& skeleton,
               const std::vector<Block>& blocks) {
    g_lastStart = g_log.size();
    head(kTagConstruct, id, previous, tool, static_cast<uint32_t>(blocks.size()),
         static_cast<uint32_t>(skeleton.size()));
    payload(reinterpret_cast<const uint8_t*>(skeleton.data()), skeleton.size());
    for (const Block& b : blocks) {
        head(b.head[0], b.head[1], b.head[2], b.head[3], b.head[4], 0);
        payload(b.data.data(), b.data.size());
    }
}

void release(uint32_t id) {
    g_lastStart = g_log.size();
    head(kTagRelease, id, 0, 0, 0, 0);
}

void bind(uint32_t id, const std::string& handle) {
    g_lastStart = g_log.size();
    head(kTagBind, id, 0, 0, 0, static_cast<uint32_t>(handle.size()));
    payload(reinterpret_cast<const uint8_t*>(handle.data()), handle.size());
}

void adopt(uint32_t id, const std::string& text) {
    g_lastStart = g_log.size();
    head(kTagAdopt, id, 0, 0, 0, static_cast<uint32_t>(text.size()));
    payload(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

void append(const uint8_t* bytes, size_t length) {
    g_lastStart = g_log.size();
    g_log.insert(g_log.end(), bytes, bytes + length);
}

const std::vector<uint8_t>& bytes() { return g_log; }
const uint8_t* lastData() { return g_log.data() + g_lastStart; }
size_t lastSize() { return g_log.size() - g_lastStart; }

void clear() {
    g_log.clear();
    g_lastStart = 0;
}

}  // namespace kapy_facts
