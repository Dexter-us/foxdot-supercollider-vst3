#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace dexter {

// An SC Out unit has one bus input followed by one input per output channel.
// Reject zero, three-or-more channels, and malformed widths instead of silently
// treating a multichannel SynthDef as mono.
constexpr int synthOutputChannelsFromOutInputCount(std::size_t inputCount) noexcept {
    return inputCount == 2 ? 1 : inputCount == 3 ? 2 : 0;
}

constexpr int mergeSynthOutputChannels(int currentChannels,
                                       std::size_t outInputCount) noexcept {
    const int nextChannels =
        synthOutputChannelsFromOutInputCount(outInputCount);
    if (nextChannels == 0 ||
        (currentChannels != 0 && currentChannels != nextChannels))
        return 0;
    return nextChannels;
}

inline std::array<float, 2> stereoBalanceGains(float pan) noexcept {
    pan = std::clamp(pan, -1.0f, 1.0f);
    const float left = pan > 0.0f ? std::sqrt(1.0f - pan) : 1.0f;
    const float right = pan < 0.0f ? std::sqrt(1.0f + pan) : 1.0f;
    return {left, right};
}

constexpr int userSynthOutputBus(int outputChannels, std::size_t voiceIndex,
                                 int monoBusBase, int voiceBusStride,
                                 int stereoBusBase) noexcept {
    if (outputChannels == 2)
        return stereoBusBase + static_cast<int>(voiceIndex * 2);
    return monoBusBase + voiceBusStride * static_cast<int>(voiceIndex);
}

} // namespace dexter
