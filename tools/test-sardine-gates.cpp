#include "../spikes/vst3-callback/sardine_pattern_gates.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

int main() {
    using dexter::sardineEventPasses;

    // No gate and "always" pass; "never" always skips.
    assert(sardineEventPasses(1.0, 0, 0, 0, 0, 0.0, 0, 0));
    assert(!sardineEventPasses(0.0, 0, 0, 0, 0, 0.0, 0, 0));

    // A 4-bar loaf with on=(1, 3) is 1-based at the API and a 0b0101 mask.
    assert(sardineEventPasses(1.0, 0, 0, 0, 0, 0.0, 4, 0b0101));
    assert(!sardineEventPasses(1.0, 0, 0, 0, 0, 4.0, 4, 0b0101));
    assert(sardineEventPasses(1.0, 0, 0, 0, 0, 8.0, 4, 0b0101));
    assert(!sardineEventPasses(1.0, 0, 0, 0, 0, 12.0, 4, 0b0101));
    assert(sardineEventPasses(1.0, 0, 0, 1, 0, 16.0, 4, 0b0101));

    // Chance is stable for a given seed/event, differs across events, and
    // averages near its requested probability over a bounded test sequence.
    std::size_t passed = 0;
    for (std::size_t event = 0; event < 256; ++event) {
        const bool first = sardineEventPasses(
            0.5, 77, 2, static_cast<std::int64_t>(event / 8), event,
            static_cast<double>(event) * 0.5, 0, 0);
        const bool repeat = sardineEventPasses(
            0.5, 77, 2, static_cast<std::int64_t>(event / 8), event,
            static_cast<double>(event) * 0.5, 0, 0);
        assert(first == repeat);
        passed += first ? 1u : 0u;
    }
    assert(passed > 80 && passed < 176);
}
