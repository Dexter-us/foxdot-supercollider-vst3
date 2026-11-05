// Limut-inspired stereo effect DSP, separated from the SuperCollider ABI
// wrapper so the bounded signal behavior can be tested without scsynth.
#pragma once

#include <algorithm>
#include <array>
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

constexpr uint32_t kMaximumDisperserStages = 24;

struct AllpassSectionState {
    float x1 = 0.0f;
    float x2 = 0.0f;
    float y1 = 0.0f;
    float y2 = 0.0f;
};

struct DisperserState {
    std::array<std::array<AllpassSectionState, kMaximumDisperserStages>, 2>
        sections{};
    float sampleRate = 44100.0f;
    float currentFrequency = 440.0f;
    float currentQ = 3.0f;
    float currentStages = 24.0f;
    bool initialized = false;
};

struct AllpassCoefficients {
    float b0 = 0.0f;
    float b1 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
};

inline AllpassCoefficients allpassCoefficients(float frequency, float q,
                                                float sampleRate) {
    const float maximumFrequency =
        std::max(30.0f, std::min(18000.0f, sampleRate * 0.45f));
    frequency = clampFinite(frequency, 30.0f, maximumFrequency, 440.0f);
    q = clampFinite(q, 0.1f, 10.0f, 3.0f);
    const float omega = 6.2831853071795864769f * frequency / sampleRate;
    const float cosine = std::cos(omega);
    const float alpha = std::sin(omega) / (2.0f * q);
    const float denominator = 1.0f + alpha;
    AllpassCoefficients result;
    result.b0 = (1.0f - alpha) / denominator;
    result.b1 = -2.0f * cosine / denominator;
    result.a1 = result.b1;
    result.a2 = result.b0;
    return result;
}

inline float allpassSample(AllpassSectionState& state, float input,
                           const AllpassCoefficients& coefficients) {
    const float output = finiteOr(
        coefficients.b0 * input + coefficients.b1 * state.x1 + state.x2 -
        coefficients.a1 * state.y1 - coefficients.a2 * state.y2);
    state.x2 = state.x1;
    state.x1 = input;
    state.y2 = state.y1;
    state.y1 = output;
    return output;
}

inline void disperserSample(DisperserState& state, float inputLeft,
                            float inputRight, float frequency, float q,
                            float stages, float mix, float& outputLeft,
                            float& outputRight) {
    inputLeft = finiteOr(inputLeft);
    inputRight = finiteOr(inputRight);
    mix = clampFinite(mix, 0.0f, 1.0f, 0.35f);
    frequency = clampFinite(frequency, 30.0f,
        std::max(30.0f, std::min(18000.0f, state.sampleRate * 0.45f)),
        440.0f);
    q = clampFinite(q, 0.1f, 10.0f, 3.0f);
    stages = clampFinite(stages, 1.0f,
                         static_cast<float>(kMaximumDisperserStages), 24.0f);
    if (!std::isfinite(state.sampleRate) || state.sampleRate < 1000.0f)
        state.sampleRate = 44100.0f;

    if (!state.initialized) {
        state.currentFrequency = frequency;
        state.currentQ = q;
        state.currentStages = stages;
        state.initialized = true;
    } else {
        const float smoothing = 1.0f - std::exp(
            -1.0f / (0.02f * state.sampleRate));
        state.currentFrequency +=
            (frequency - state.currentFrequency) * smoothing;
        state.currentQ += (q - state.currentQ) * smoothing;
        state.currentStages += (stages - state.currentStages) * smoothing;
    }

    const int lowerCount = std::clamp(
        static_cast<int>(std::floor(state.currentStages)), 1,
        static_cast<int>(kMaximumDisperserStages));
    const int upperCount = std::min(
        static_cast<int>(kMaximumDisperserStages), lowerCount + 1);
    const float countBlend = std::clamp(
        state.currentStages - static_cast<float>(lowerCount), 0.0f, 1.0f);
    const auto coefficients = allpassCoefficients(
        state.currentFrequency, state.currentQ, state.sampleRate);
    const auto processChannel = [&](int channel, float input) {
        float value = input;
        float lower = input;
        float upper = input;
        for (int stage = 0; stage < upperCount; ++stage) {
            value = allpassSample(state.sections[static_cast<std::size_t>(channel)]
                                                  [static_cast<std::size_t>(stage)],
                                  value, coefficients);
            const int completedStages = stage + 1;
            if (completedStages == lowerCount)
                lower = value;
            if (completedStages == upperCount)
                upper = value;
        }
        return lower * (1.0f - countBlend) + upper * countBlend;
    };
    const float wetLeft = processChannel(0, inputLeft);
    const float wetRight = processChannel(1, inputRight);
    outputLeft = finiteOr(inputLeft * (1.0f - mix) + wetLeft * mix);
    outputRight = finiteOr(inputRight * (1.0f - mix) + wetRight * mix);
}

struct FuzzState {
    float sampleRate = 44100.0f;
    float currentToneHz = 12000.0f;
    float currentDriveDb = 12.0f;
    float lowpassLeft = 0.0f;
    float lowpassRight = 0.0f;
    bool initialized = false;
};

inline float fuzzChannel(float input, float drive, float normalizer,
                         float toneHz, float sampleRate, float& lowpass) {
    input = std::clamp(finiteOr(input), -8.0f, 8.0f);
    const float shaped = std::tanh(input * drive) / normalizer;
    const float cutoff = std::min(toneHz, sampleRate * 0.45f);
    const float coefficient = 1.0f - std::exp(
        -6.2831853071795864769f * cutoff / sampleRate);
    lowpass += coefficient * (shaped - lowpass);
    return finiteOr(lowpass);
}

inline void fuzzSample(FuzzState& state, float inputLeft, float inputRight,
                       float driveDb, float toneHz, float mix,
                       float& outputLeft, float& outputRight) {
    inputLeft = finiteOr(inputLeft);
    inputRight = finiteOr(inputRight);
    driveDb = clampFinite(driveDb, 0.0f, 36.0f, 12.0f);
    toneHz = clampFinite(toneHz, 200.0f, 16000.0f, 9000.0f);
    mix = clampFinite(mix, 0.0f, 1.0f, 0.35f);
    if (!std::isfinite(state.sampleRate) || state.sampleRate < 1000.0f)
        state.sampleRate = 44100.0f;
    if (!state.initialized) {
        state.currentToneHz = toneHz;
        state.currentDriveDb = driveDb;
        state.initialized = true;
    } else {
        const float smoothing = 1.0f - std::exp(
            -1.0f / (0.01f * state.sampleRate));
        state.currentToneHz +=
            (toneHz - state.currentToneHz) * smoothing;
        state.currentDriveDb +=
            (driveDb - state.currentDriveDb) * smoothing;
    }
    const float drive = std::pow(10.0f, state.currentDriveDb / 20.0f);
    const float normalizer = std::max(0.001f, std::tanh(drive));
    const float wetLeft = fuzzChannel(
        inputLeft, drive, normalizer, state.currentToneHz, state.sampleRate,
        state.lowpassLeft);
    const float wetRight = fuzzChannel(
        inputRight, drive, normalizer, state.currentToneHz, state.sampleRate,
        state.lowpassRight);
    outputLeft = finiteOr(inputLeft * (1.0f - mix) + wetLeft * mix);
    outputRight = finiteOr(inputRight * (1.0f - mix) + wetRight * mix);
}

} // namespace limutfx
