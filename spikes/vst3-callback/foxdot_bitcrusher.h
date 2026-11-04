#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace dexter {

struct FoxDotBitcrusherState {
    double phase = 0.0;
    float heldLeft = 0.0f;
    float heldRight = 0.0f;
    float wet = 0.0f;
    bool initialized = false;
};

inline float quantizeFoxDotBits(float sample, float bitDepth) noexcept {
    if (!std::isfinite(sample) || !std::isfinite(bitDepth))
        return 0.0f;
    const int bits = std::clamp(static_cast<int>(std::lround(bitDepth)), 1, 24);
    const double levels = static_cast<double>((std::uint32_t{1} << bits) - 1u);
    const double clipped = std::clamp(static_cast<double>(sample), -1.0, 1.0);
    return static_cast<float>(
        (std::round((clipped + 1.0) * 0.5 * levels) / levels) * 2.0 - 1.0);
}

inline std::array<float, 2> processFoxDotBitcrusher(
    FoxDotBitcrusherState& state, float left, float right, float bitDepth,
    float crush, double sampleRate, float smoothing) noexcept {
    if (!std::isfinite(left)) left = 0.0f;
    if (!std::isfinite(right)) right = 0.0f;
    if (!std::isfinite(bitDepth)) bitDepth = 0.0f;
    if (!std::isfinite(crush)) crush = 1.0f;
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        return {left, right};
    bitDepth = std::clamp(bitDepth, 0.0f, 24.0f);
    crush = std::clamp(crush, 1.0f, 128.0f);
    smoothing = std::clamp(smoothing, 0.0f, 1.0f);

    const float targetWet = bitDepth > 0.0f ? 1.0f : 0.0f;
    state.wet += smoothing * (targetWet - state.wet);
    if (targetWet > 0.0f) {
        const double rate = std::clamp(sampleRate / crush, 1.0, sampleRate);
        state.phase += rate / sampleRate;
        if (!state.initialized || state.phase >= 1.0) {
            state.phase -= std::floor(state.phase);
            state.heldLeft = quantizeFoxDotBits(left, bitDepth);
            state.heldRight = quantizeFoxDotBits(right, bitDepth);
            state.initialized = true;
        }
    } else {
        state.heldLeft = left;
        state.heldRight = right;
        state.initialized = true;
        state.phase = 0.0;
    }

    if (state.wet >= 1.0f)
        return {state.heldLeft, state.heldRight};
    return {left + state.wet * (state.heldLeft - left),
            right + state.wet * (state.heldRight - right)};
}

} // namespace dexter
