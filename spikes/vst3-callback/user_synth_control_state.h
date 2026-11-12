#pragma once

#include <cstddef>
#include <cstdint>

namespace dexter {

// Keep panel-edited SynthDef values when Run replaces a player's pattern with
// another pattern using the same loaded user SynthDef. Pending slider events
// are folded in too, so a quick Run immediately after dragging cannot lose the
// final value. Switching instruments intentionally starts from the new
// SynthDef's sidecar defaults instead.
template <typename Values, typename PendingValues>
inline void preserveUserSynthControlOverrides(
    int previousSynthIndex, int nextSynthIndex, int firstUserSynthIndex,
    int previousDefinitionIndex, std::uint8_t previousMask,
    const Values& previousValues, std::uint8_t pendingMask,
    const PendingValues& pendingValues, int& nextDefinitionIndex,
    std::uint8_t& nextMask, Values& nextValues) noexcept {
    if (previousSynthIndex < firstUserSynthIndex ||
        nextSynthIndex != previousSynthIndex)
        return;

    const int definitionIndex = nextSynthIndex - firstUserSynthIndex;
    const auto copyMasked = [&](std::uint8_t mask, const auto& values) {
        for (std::size_t control = 0; control < nextValues.size(); ++control) {
            const auto bit = static_cast<std::uint8_t>(1u << control);
            if ((mask & bit) == 0)
                continue;
            nextValues[control] = values[control];
            nextMask |= bit;
        }
    };

    if (previousDefinitionIndex == definitionIndex && previousMask != 0) {
        nextDefinitionIndex = definitionIndex;
        copyMasked(previousMask, previousValues);
    }
    if (pendingMask != 0) {
        nextDefinitionIndex = definitionIndex;
        copyMasked(pendingMask, pendingValues);
    }
}

} // namespace dexter
