// Limut-inspired stereo effect DSP, separated from the SuperCollider ABI
// wrapper so the bounded signal behavior can be tested without scsynth.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace limutfx {

inline float finiteOr(float value, float fallback = 0.0f) {
    return std::isfinite(value) ? value : fallback;
}

inline float clampFinite(float value, float minimum, float maximum,
                         float fallback) {
    return std::clamp(finiteOr(value, fallback), minimum, maximum);
}

struct PingPongState {
    float* left = nullptr;
    float* right = nullptr;
    uint32_t frames = 0;
    uint32_t writeIndex = 0;
    float sampleRate = 44100.0f;
    float currentDelayFrames = 0.0f;
    bool delayInitialized = false;
};

inline float readLinear(const float* buffer, uint32_t frames,
                        uint32_t writeIndex, float delayFrames) {
    if (!buffer || frames < 2)
        return 0.0f;
    const float boundedDelay =
        std::clamp(delayFrames, 1.0f, static_cast<float>(frames - 1));
    float position = static_cast<float>(writeIndex) - boundedDelay;
    if (position < 0.0f)
        position += static_cast<float>(frames);
    const auto first = static_cast<uint32_t>(position) % frames;
    const auto second = (first + 1u) % frames;
    const float fraction = position - std::floor(position);
    return buffer[first] + (buffer[second] - buffer[first]) * fraction;
}

inline void pingPongSample(PingPongState& state, float inputLeft,
                           float inputRight, float delaySeconds,
                           float feedback, float mix, float& outputLeft,
                           float& outputRight) {
    inputLeft = finiteOr(inputLeft);
    inputRight = finiteOr(inputRight);
    delaySeconds = clampFinite(delaySeconds, 0.01f, 2.0f, 0.32f);
    feedback = clampFinite(feedback, 0.0f, 0.95f, 0.45f);
    mix = clampFinite(mix, 0.0f, 1.0f, 0.25f);

    if (!state.left || !state.right || state.frames < 2 ||
        !std::isfinite(state.sampleRate) || state.sampleRate <= 0.0f) {
        outputLeft = inputLeft;
        outputRight = inputRight;
        return;
    }
    const float targetDelayFrames = std::clamp(
        delaySeconds * state.sampleRate, 1.0f,
        static_cast<float>(state.frames - 1));
    if (!state.delayInitialized) {
        state.currentDelayFrames = targetDelayFrames;
        state.delayInitialized = true;
    } else {
        // Smooth block-rate changes to delay time instead of jumping the read
        // head, which can click. This also makes var/linvar automation safe.
        state.currentDelayFrames +=
            (targetDelayFrames - state.currentDelayFrames) * 0.002f;
    }
    const float delayedLeft = readLinear(
        state.left, state.frames, state.writeIndex, state.currentDelayFrames);
    const float delayedRight = readLinear(
        state.right, state.frames, state.writeIndex, state.currentDelayFrames);
    const float writeLeft = finiteOr(inputLeft + delayedRight * feedback);
    const float writeRight = finiteOr(inputRight + delayedLeft * feedback);
    state.left[state.writeIndex] = writeLeft;
    state.right[state.writeIndex] = writeRight;
    // Cross the delayed channels: a left input returns on the right first,
    // then the feedback tap returns it on the left on the next repeat.
    outputLeft = inputLeft * (1.0f - mix) + delayedRight * mix;
    outputRight = inputRight * (1.0f - mix) + delayedLeft * mix;
    state.writeIndex = (state.writeIndex + 1u) % state.frames;
}

struct DuckState {
    float phaseSeconds = 0.0f;
    float sampleRate = 44100.0f;
    float currentGain = 1.0f;
};

inline float duckGain(float phaseSeconds, float periodSeconds,
                      float depth, float holdSeconds, float attackSeconds) {
    const float period =
        clampFinite(periodSeconds, 0.05f, 8.0f, 0.5f);
    depth = clampFinite(depth, 0.0f, 1.0f, 0.7f);
    holdSeconds = clampFinite(holdSeconds, 0.0f, period, 0.12f);
    attackSeconds = clampFinite(attackSeconds, 0.001f, period, 0.18f);
    attackSeconds = std::min(attackSeconds, std::max(0.001f, period - holdSeconds));
    const float phase = std::fmod(std::max(0.0f, phaseSeconds), period);
    if (phase < holdSeconds)
        return 1.0f - depth;
    if (phase < holdSeconds + attackSeconds) {
        const float progress = (phase - holdSeconds) / attackSeconds;
        return 1.0f - depth * (1.0f - progress);
    }
    return 1.0f;
}

inline void duckSample(DuckState& state, float inputLeft, float inputRight,
                      float periodSeconds, float depth, float holdSeconds,
                      float attackSeconds, float mix, float& outputLeft,
                      float& outputRight) {
    inputLeft = finiteOr(inputLeft);
    inputRight = finiteOr(inputRight);
    mix = clampFinite(mix, 0.0f, 1.0f, 1.0f);
    const float period =
        clampFinite(periodSeconds, 0.05f, 8.0f, 0.5f);
    if (!std::isfinite(state.sampleRate) || state.sampleRate <= 0.0f)
        state.sampleRate = 44100.0f;

    const float targetGain = duckGain(state.phaseSeconds, period, depth,
                                      holdSeconds, attackSeconds);
    const float safeRate = std::max(1000.0f, state.sampleRate);
    // A short one-pole edge prevents the rhythmic gate from creating
    // broadband clicks while preserving the deliberate pump shape.
    const float smoothing = std::exp(-1.0f / (0.005f * safeRate));
    state.currentGain =
        targetGain + (state.currentGain - targetGain) * smoothing;
    const float gain = state.currentGain;
    const float wetLeft = inputLeft * gain;
    const float wetRight = inputRight * gain;
    outputLeft = inputLeft * (1.0f - mix) + wetLeft * mix;
    outputRight = inputRight * (1.0f - mix) + wetRight * mix;

    state.phaseSeconds += 1.0f / state.sampleRate;
    if (state.phaseSeconds >= period)
        state.phaseSeconds = std::fmod(state.phaseSeconds, period);
}

} // namespace limutfx
