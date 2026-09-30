// services/occt/build/embind/bindings/capiMemo.cpp
//
// The memo behind capiMemo.hxx: a map from key to handle with a recency list,
// the same policy as the TypeScript op memo (512 entries, oldest out, a hit
// refreshes, a dead handle is a miss).
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: what is memoised, or how a handle is minted.

#include "capiMemo.hxx"

#include <iterator>
#include <list>
#include <unordered_map>

#include "capiStore.hxx"

namespace kapy_capi {

namespace {
constexpr size_t MEMO_LIMIT = 512;

struct Slot {
    uint32_t handle;
    std::list<std::string>::iterator order;
};

std::unordered_map<std::string, Slot> g_memo;
std::list<std::string> g_order;
}  // namespace

uint32_t memoFind(const std::string& key) {
    auto it = g_memo.find(key);
    if (it == g_memo.end()) return 0;
    if (!find(it->second.handle)) {
        g_order.erase(it->second.order);
        g_memo.erase(it);
        return 0;
    }
    g_order.splice(g_order.end(), g_order, it->second.order);
    return it->second.handle;
}

void memoStore(const std::string& key, uint32_t handle) {
    auto it = g_memo.find(key);
    if (it != g_memo.end()) {
        g_order.erase(it->second.order);
        g_memo.erase(it);
    }
    g_order.push_back(key);
    g_memo[key] = Slot{handle, std::prev(g_order.end())};
    while (g_memo.size() > MEMO_LIMIT) {
        g_memo.erase(g_order.front());
        g_order.pop_front();
    }
}

void memoClear() {
    g_memo.clear();
    g_order.clear();
}

}  // namespace kapy_capi
