// services/occt/build/embind/bindings/capiCore.cpp
//
// The parts of the C API that are not an operation: the ABI version, memory
// the host writes into, the result arena, the last error and the self-test.
// The self-test is the boot check of the kernel: it builds a box inside the
// kernel and walks the store and the measurements over it, so a link that is
// broken in any of them is known at boot and not at the first regen, and then
// asks for the boolean history of edges and vertices (capiSelfTestHistory.cpp),
// whose words ride in the last error's message when it is incomplete.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the operations (capiOps.cpp) and the embind
// registrations (capiEmbind.cpp).

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <BRepPrimAPI_MakeBox.hxx>
#include <Standard_Failure.hxx>

#include "capiSelfTest.hxx"
#include "capiState.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

namespace kapy_capi {

namespace {
std::vector<uint8_t> g_arena;
int32_t g_code = 0;
int32_t g_arg = 0;
std::string g_message;
int g_perturb = 0;
}  // namespace

void begin() {
    g_code = 0;
    g_arg = 0;
    g_message.clear();
}

int32_t fail(int32_t code, const char* message, int32_t arg) {
    g_code = code;
    g_arg = arg;
    g_message = message;
    g_arena.clear();
    return code;
}

int32_t answer(const void* bytes, size_t length) {
    g_arena.assign(static_cast<const uint8_t*>(bytes),
                   static_cast<const uint8_t*>(bytes) + length);
    return KAPY_OK;
}

int32_t answerNothing() {
    g_arena.clear();
    return KAPY_OK;
}

int perturbation() { return g_perturb; }
void setPerturbation(int mode) { g_perturb = mode; }

}  // namespace kapy_capi

using namespace kapy_capi;

KAPY_API int32_t kapy_abi_version() noexcept { return KAPY_ABI_VERSION; }

KAPY_API uint32_t kapy_alloc(uint32_t bytes) noexcept {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(std::malloc(bytes ? bytes : 1)));
}

KAPY_API void kapy_free(uint32_t ptr) noexcept {
    std::free(reinterpret_cast<void*>(static_cast<uintptr_t>(ptr)));
}

KAPY_API uint32_t kapy_result_ptr() noexcept {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_arena.data()));
}

KAPY_API uint32_t kapy_result_len() noexcept { return static_cast<uint32_t>(g_arena.size()); }

KAPY_API int32_t kapy_error() noexcept { return g_code; }

KAPY_API int32_t kapy_error_arg() noexcept { return g_arg; }

KAPY_API int32_t kapy_error_message() noexcept {
    const std::string text = g_message;
    const int32_t code = g_code;
    answer(text.data(), text.size());
    g_code = code;
    g_message = text;
    return static_cast<int32_t>(text.size());
}

// Returns 0 when every check passed, else the number of the first that did
// not (12: the boolean history is incomplete, said in `kapy_error_message`).
// The perturbation is held off while it runs: the self-test certifies the
// link, not the red control.
KAPY_API int32_t kapy_self_test() noexcept {
    const int held = perturbation();
    setPerturbation(0);
    int32_t verdict = 0;
    std::string words;
    try {
        BRepPrimAPI_MakeBox box(1.0, 2.0, 3.0);
        const uint32_t handle = put(box.Shape());
        Entry* entry = find(handle);
        if (!entry) verdict = 1;
        if (!verdict) {
            ensureMaps(*entry);
            if (entry->faces.Extent() != 6 || entry->edges.Extent() != 12 ||
                entry->vertices.Extent() != 8) {
                verdict = 2;
            }
        }
        if (!verdict && kapy_volume(handle) != KAPY_OK) verdict = 3;
        if (!verdict) {
            double volume = 0;
            std::memcpy(&volume, g_arena.data(), sizeof(double));
            if (std::fabs(volume - 6.0) > 1e-9) verdict = 4;
        }
        if (!verdict && kapy_bounds(handle) != KAPY_OK) verdict = 5;
        if (!verdict) {
            double six[6] = {0, 0, 0, 0, 0, 0};
            if (g_arena.size() != sizeof(six)) verdict = 6;
            else {
                std::memcpy(six, g_arena.data(), sizeof(six));
                if (std::fabs(six[3] - 1.0) > 1e-6 || std::fabs(six[4] - 2.0) > 1e-6 ||
                    std::fabs(six[5] - 3.0) > 1e-6) {
                    verdict = 7;
                }
            }
        }
        if (!verdict && (kapy_retain(handle) != 2 || release(handle, false) != 1)) verdict = 8;
        if (!verdict && release(handle, false) != 0) verdict = 9;
        if (!verdict && (find(handle) != nullptr || kapy_volume(handle) != KAPY_E_UNKNOWN_HANDLE)) {
            verdict = 10;
        }
        if (!verdict) words = historyVerdict();
    } catch (...) {
        verdict = 11;
    }
    setPerturbation(held);
    begin();
    if (!verdict && !words.empty()) {
        fail(KAPY_E_FAILED, words.c_str());
        return 12;
    }
    return verdict;
}
