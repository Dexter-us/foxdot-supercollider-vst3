#include "user_synth_routing.h"

#include <cassert>
#include <cmath>

int main() {
    using dexter::stereoBalanceGains;
    using dexter::mergeSynthOutputChannels;
    using dexter::synthOutputChannelsFromOutInputCount;
    using dexter::userSynthOutputBus;

    assert(synthOutputChannelsFromOutInputCount(1) == 0);
    assert(synthOutputChannelsFromOutInputCount(2) == 1);
    assert(synthOutputChannelsFromOutInputCount(3) == 2);
    assert(synthOutputChannelsFromOutInputCount(4) == 0);
    assert(mergeSynthOutputChannels(0, 2) == 1);
    assert(mergeSynthOutputChannels(1, 2) == 1);
    assert(mergeSynthOutputChannels(0, 3) == 2);
    assert(mergeSynthOutputChannels(2, 3) == 2);
    assert(mergeSynthOutputChannels(1, 3) == 0); // reject mixed mono/stereo Outs
    assert(mergeSynthOutputChannels(2, 2) == 0);
    assert(mergeSynthOutputChannels(0, 1) == 0);

    constexpr int monoBase = 2;
    constexpr int voiceStride = 3;
    constexpr int stereoBase = monoBase + 3 * 8;
    assert(userSynthOutputBus(1, 0, monoBase, voiceStride, stereoBase) == 2);
    assert(userSynthOutputBus(1, 3, monoBase, voiceStride, stereoBase) == 11);
    assert(userSynthOutputBus(2, 0, monoBase, voiceStride, stereoBase) == 26);
    assert(userSynthOutputBus(2, 3, monoBase, voiceStride, stereoBase) == 32);

    const auto center = stereoBalanceGains(0.0f);
    assert(center[0] == 1.0f && center[1] == 1.0f);
    const auto left = stereoBalanceGains(-1.0f);
    assert(left[0] == 1.0f && left[1] == 0.0f);
    const auto right = stereoBalanceGains(1.0f);
    assert(right[0] == 0.0f && right[1] == 1.0f);
    const auto partial = stereoBalanceGains(0.5f);
    assert(std::abs(partial[0] - std::sqrt(0.5f)) < 1e-6f);
    assert(partial[1] == 1.0f);
    const auto clamped = stereoBalanceGains(4.0f);
    assert(clamped[0] == 0.0f && clamped[1] == 1.0f);
    return 0;
}
