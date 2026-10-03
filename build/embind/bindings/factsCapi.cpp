// engine/kernels/occt/build/embind/bindings/factsCapi.cpp
//
// The facts log as the Rust core reads it over the C transport: one call
// drains the log into the result arena, in the D-N4 layout
// (`topo/store/wire.rs`), and leaves it empty. An empty log answers an empty
// arena. Nothing is ever lost on the way: the bytes are copied into the arena
// before the log is cleared.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: writing the log (factsLog.cpp), collecting.

#include "capiState.hxx"
#include "factsLog.hxx"
#include "kapy_capi.h"

KAPY_API int32_t kapy_facts_take() noexcept {
    kapy_capi::begin();
    const std::vector<uint8_t>& log = kapy_facts::bytes();
    const int32_t code = log.empty() ? kapy_capi::answerNothing()
                                     : kapy_capi::answer(log.data(), log.size());
    if (code == KAPY_OK) kapy_facts::clear();
    return code;
}
