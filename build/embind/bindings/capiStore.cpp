// engine/kernels/occt/build/embind/bindings/capiStore.cpp
//
// The shape table behind the handles (see capiStore.hxx for the contract).
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the C entry points and the embind registrations.

#include "capiStore.hxx"

#include "capiImportCache.hxx"
#include "capiMemo.hxx"
#include "factsLog.hxx"

#include <algorithm>
#include <unordered_map>

#include <TopExp.hxx>
#include <TopAbs_ShapeEnum.hxx>

namespace kapy_capi {

namespace {
constexpr uint32_t SERIAL_BITS = 24;
constexpr uint32_t SERIAL_MASK = (1u << SERIAL_BITS) - 1;

std::unordered_map<uint32_t, Entry> g_entries;
std::vector<uint32_t> g_dropped;
std::vector<uint32_t> g_minted;
uint32_t g_table = 1;
uint32_t g_epoch = 0;
uint32_t g_serial = 0;
}  // namespace

uint32_t put(const TopoDS_Shape& shape) {
    g_serial = (g_serial + 1) & SERIAL_MASK;
    // 16.7 million shapes in one epoch would wrap the serial onto a live
    // handle; skipping zero and live ones keeps "never aliases" true.
    while (g_serial == 0 || g_entries.count((g_epoch << SERIAL_BITS) | g_serial)) {
        g_serial = (g_serial + 1) & SERIAL_MASK;
    }
    const uint32_t handle = (g_epoch << SERIAL_BITS) | g_serial;
    Entry& entry = g_entries[handle];
    entry.shape = shape;
    return handle;
}

uint32_t putNative(const TopoDS_Shape& shape, uint32_t tableId) {
    const uint32_t handle = put(shape);
    g_entries[handle].tableId = tableId;
    g_minted.push_back(handle);
    return handle;
}

std::vector<uint32_t> takeMinted() {
    std::vector<uint32_t> out;
    out.swap(g_minted);
    return out;
}

void seedTable(uint32_t next) { g_table = next; }
uint32_t nextTable() { return g_table; }
uint32_t allocTable() { return g_table++; }

Entry* find(uint32_t handle) {
    auto it = g_entries.find(handle);
    return it == g_entries.end() ? nullptr : &it->second;
}

uint32_t retain(uint32_t handle) {
    Entry* entry = find(handle);
    if (!entry) return 0;
    return ++entry->refs;
}

int32_t release(uint32_t handle, bool queueDropped) {
    auto it = g_entries.find(handle);
    if (it == g_entries.end()) return -1;
    if (--it->second.refs > 0) return static_cast<int32_t>(it->second.refs);
    // The naming table goes with its last owner: the core learns it from the
    // facts log, so nobody else has to remember to free it.
    if (it->second.tableId != 0) kapy_facts::release(it->second.tableId);
    g_entries.erase(it);
    if (queueDropped) g_dropped.push_back(handle);
    return 0;
}

void ensureMaps(Entry& entry) {
    if (entry.mapped) return;
    TopExp::MapShapes(entry.shape, TopAbs_FACE, entry.faces);
    TopExp::MapShapes(entry.shape, TopAbs_EDGE, entry.edges);
    TopExp::MapShapes(entry.shape, TopAbs_VERTEX, entry.vertices);
    entry.mapped = true;
}

void reset(bool bumpEpoch) {
    // Every table of an entry that dies here is released, in id order.
    std::vector<uint32_t> tables;
    for (const auto& kv : g_entries) {
        if (kv.second.tableId != 0) tables.push_back(kv.second.tableId);
    }
    std::sort(tables.begin(), tables.end());
    for (uint32_t id : tables) kapy_facts::release(id);
    g_entries.clear();
    g_dropped.clear();
    g_minted.clear();
    memoClear();
    clearImportCaches();
    g_serial = 0;
    if (bumpEpoch) g_epoch = (g_epoch + 1) & 0xff;
}

std::vector<uint32_t> takeDropped() {
    std::vector<uint32_t> out;
    out.swap(g_dropped);
    return out;
}

uint32_t live() { return static_cast<uint32_t>(g_entries.size()); }
uint32_t epoch() { return g_epoch; }

}  // namespace kapy_capi
