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

constexpr float kPitchBufferSeconds = 1.05f;
constexpr std::size_t kReverbCombCount = 4;

struct ShifterState {
    float* left = nullptr;
    float* right = nullptr;
    uint32_t frames = 0;
    uint32_t writeIndex = 0;
    float sampleRate = 44100.0f;
    float phase = 0.0f;
    float currentMix = 1.0f;
    float currentRatio = 2.0f;
    float currentWindowSeconds = 0.12f;
    bool initialized = false;
};

inline void preparePitchBuffer(ShifterState& state) {
    state.frames = static_cast<uint32_t>(
        std::ceil(state.sampleRate * kPitchBufferSeconds)) + 4u;
    state.writeIndex = 0;
    state.phase = 0.0f;
    state.initialized = false;
}

inline void freePitchBuffer(ShifterState& state) {
    state.left = nullptr;
    state.right = nullptr;
    state.frames = 0;
    state.writeIndex = 0;
}

inline void shifterSample(ShifterState& state, float inputLeft,
                          float inputRight, float mix, float ratio,
                          float windowSeconds, float& outputLeft,
                          float& outputRight) {
    inputLeft = finiteOr(inputLeft);
    inputRight = finiteOr(inputRight);
    mix = clampFinite(mix, 0.0f, 1.0f, 1.0f);
    ratio = clampFinite(ratio, 0.5f, 2.0f, 2.0f);
    windowSeconds = clampFinite(windowSeconds, 0.02f, 0.5f, 0.12f);
    if (!std::isfinite(state.sampleRate) || state.sampleRate < 1000.0f)
        state.sampleRate = 44100.0f;
    if (!state.left || !state.right || state.frames < 128) {
        outputLeft = inputLeft;
        outputRight = inputRight;
        return;
    }

    if (!state.initialized) {
        state.currentMix = mix;
        state.currentRatio = ratio;
        state.currentWindowSeconds = windowSeconds;
        state.initialized = true;
    } else {
        const float smoothing = 1.0f - std::exp(
            -1.0f / (0.02f * state.sampleRate));
        state.currentMix += (mix - state.currentMix) * smoothing;
        state.currentRatio += (ratio - state.currentRatio) * smoothing;
        state.currentWindowSeconds +=
            (windowSeconds - state.currentWindowSeconds) * smoothing;
    }

    state.left[state.writeIndex] = inputLeft;
    state.right[state.writeIndex] = inputRight;
    const float maxWindow = static_cast<float>(state.frames - 8u) * 0.5f;
    const float windowFrames = std::clamp(
        state.currentWindowSeconds * state.sampleRate, 64.0f, maxWindow);
    const float depthFrames = std::clamp(
        std::fabs(1.0f - state.currentRatio) * windowFrames, 0.0f,
        windowFrames - 2.0f);
    const auto delayAtPhase = [&](float phase) {
        phase -= std::floor(phase);
        return state.currentRatio >= 1.0f
            ? 1.0f + (1.0f - phase) * depthFrames
            : 1.0f + phase * depthFrames;
    };
    const float phaseA = state.phase;
    float phaseB = phaseA + 0.5f;
    if (phaseB >= 1.0f)
        phaseB -= 1.0f;
    const float windowA =
        0.5f - 0.5f * std::cos(6.2831853071795864769f * phaseA);
    const float windowB = 1.0f - windowA;
    const float shiftedLeft =
        readLinear(state.left, state.frames, state.writeIndex,
                   delayAtPhase(phaseA)) * windowA +
        readLinear(state.left, state.frames, state.writeIndex,
                   delayAtPhase(phaseB)) * windowB;
    const float shiftedRight =
        readLinear(state.right, state.frames, state.writeIndex,
                   delayAtPhase(phaseA)) * windowA +
        readLinear(state.right, state.frames, state.writeIndex,
                   delayAtPhase(phaseB)) * windowB;
    outputLeft = finiteOr(inputLeft * (1.0f - state.currentMix) +
                          shiftedLeft * state.currentMix);
    outputRight = finiteOr(inputRight * (1.0f - state.currentMix) +
                           shiftedRight * state.currentMix);

    state.phase += 1.0f / windowFrames;
    state.phase -= std::floor(state.phase);
    state.writeIndex = (state.writeIndex + 1u) % state.frames;
}

struct ReverbState {
    std::array<std::array<float*, kReverbCombCount>, 2> buffers{};
    std::array<std::array<uint32_t, kReverbCombCount>, 2> lengths{};
    std::array<std::array<uint32_t, kReverbCombCount>, 2> writeIndices{};
    std::array<std::array<float, kReverbCombCount>, 2> dampingState{};
    float sampleRate = 44100.0f;
    float currentRoom = 0.65f;
    float currentDamp = 0.5f;
    bool initialized = false;
};

inline void prepareReverb(ReverbState& state) {
    static constexpr std::array<float, kReverbCombCount> baseSeconds{
        0.0297f, 0.0371f, 0.0411f, 0.0437f};
    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t comb = 0; comb < kReverbCombCount; ++comb) {
            const float stereoOffset = channel == 0 ? 0.0f : 0.0017f;
            const auto length = static_cast<uint32_t>(std::max(
                64.0f, std::round((baseSeconds[comb] + stereoOffset) *
                                  state.sampleRate)));
            state.lengths[channel][comb] = length;
            state.writeIndices[channel][comb] = 0;
            state.dampingState[channel][comb] = 0.0f;
        }
    }
    state.currentRoom = 0.65f;
    state.currentDamp = 0.5f;
    state.initialized = false;
}

inline void freeReverb(ReverbState& state) {
    for (auto& channel : state.buffers)
        for (auto& buffer : channel)
            buffer = nullptr;
    for (auto& channel : state.lengths)
        channel.fill(0);
    for (auto& channel : state.writeIndices)
        channel.fill(0);
}

inline void reverbSample(ReverbState& state, float inputLeft, float inputRight,
                         float room, float damp, float& outputLeft,
                         float& outputRight) {
    inputLeft = finiteOr(inputLeft);
    inputRight = finiteOr(inputRight);
    room = clampFinite(room, 0.0f, 1.0f, 0.65f);
    damp = clampFinite(damp, 0.0f, 1.0f, 0.5f);
    if (!std::isfinite(state.sampleRate) || state.sampleRate < 1000.0f)
        state.sampleRate = 44100.0f;
    for (std::size_t channel = 0; channel < 2; ++channel)
        for (std::size_t comb = 0; comb < kReverbCombCount; ++comb)
            if (!state.buffers[channel][comb] ||
                state.lengths[channel][comb] < 2) {
                outputLeft = inputLeft;
                outputRight = inputRight;
                return;
            }

    if (!state.initialized) {
        state.currentRoom = room;
        state.currentDamp = damp;
        state.initialized = true;
    } else {
        const float smoothing = 1.0f - std::exp(
            -1.0f / (0.03f * state.sampleRate));
        state.currentRoom += (room - state.currentRoom) * smoothing;
        state.currentDamp += (damp - state.currentDamp) * smoothing;
    }
    const float cutoff = 16000.0f * std::pow(
        800.0f / 16000.0f, state.currentDamp);
    const float dampingCoefficient = 1.0f - std::exp(
        -6.2831853071795864769f * cutoff / state.sampleRate);
    const float feedback = 0.45f + 0.45f * state.currentRoom;
    std::array<std::array<float, kReverbCombCount>, 2> taps{};
    for (std::size_t channel = 0; channel < 2; ++channel)
        for (std::size_t comb = 0; comb < kReverbCombCount; ++comb)
            taps[channel][comb] =
                state.buffers[channel][comb][state.writeIndices[channel][comb]];

    for (std::size_t channel = 0; channel < 2; ++channel) {
        const float input = channel == 0 ? inputLeft : inputRight;
        for (std::size_t comb = 0; comb < kReverbCombCount; ++comb) {
            const auto opposite = 1u - channel;
            auto& damping = state.dampingState[channel][comb];
            damping += dampingCoefficient * (taps[channel][comb] - damping);
            state.buffers[channel][comb][state.writeIndices[channel][comb]] =
                finiteOr(input + (damping * 0.75f +
                                  taps[opposite][comb] * 0.25f) * feedback);
            auto& index = state.writeIndices[channel][comb];
            index = (index + 1u) % state.lengths[channel][comb];
        }
    }
    float wetLeft = 0.0f;
    float wetRight = 0.0f;
    for (std::size_t comb = 0; comb < kReverbCombCount; ++comb) {
        wetLeft += state.dampingState[0][comb];
        wetRight += state.dampingState[1][comb];
    }
    outputLeft = finiteOr(wetLeft * 0.25f);
    outputRight = finiteOr(wetRight * 0.25f);
}

struct ShimmerState {
    ShifterState pitch;
    ReverbState reverb;
    float sampleRate = 44100.0f;
};

inline void shimmerSample(ShimmerState& state, float inputLeft,
                          float inputRight, float mix, float ratio,
                          float windowSeconds, float room, float damp,
                          float& outputLeft, float& outputRight) {
    float shiftedLeft = 0.0f;
    float shiftedRight = 0.0f;
    float wetLeft = 0.0f;
    float wetRight = 0.0f;
    shifterSample(state.pitch, inputLeft, inputRight, 1.0f, ratio,
                  windowSeconds, shiftedLeft, shiftedRight);
    reverbSample(state.reverb, shiftedLeft, shiftedRight, room, damp,
                 wetLeft, wetRight);
    mix = clampFinite(mix, 0.0f, 1.0f, 0.35f);
    outputLeft = finiteOr(inputLeft * (1.0f - mix) + wetLeft * mix);
    outputRight = finiteOr(inputRight * (1.0f - mix) + wetRight * mix);
}

struct GatedVerbState {
    ReverbState reverb;
    float sampleRate = 44100.0f;
    float detectorEnvelope = 0.0f;
    float gateGain = 0.0f;
    float holdRemaining = 0.0f;
};

inline void gatedVerbSample(GatedVerbState& state, float inputLeft,
                            float inputRight, float mix, float room,
                            float damp, float threshold, float holdSeconds,
                            float attackSeconds, float releaseSeconds,
                            float& outputLeft, float& outputRight) {
    inputLeft = finiteOr(inputLeft);
    inputRight = finiteOr(inputRight);
    mix = clampFinite(mix, 0.0f, 1.0f, 0.35f);
    threshold = clampFinite(threshold, 0.001f, 0.25f, 0.02f);
    holdSeconds = clampFinite(holdSeconds, 0.0f, 2.0f, 0.25f);
    attackSeconds = clampFinite(attackSeconds, 0.001f, 0.25f, 0.015f);
    releaseSeconds = clampFinite(releaseSeconds, 0.01f, 1.0f, 0.15f);
    if (!std::isfinite(state.sampleRate) || state.sampleRate < 1000.0f)
        state.sampleRate = 44100.0f;

    const float detector = 0.5f * (std::fabs(inputLeft) +
                                   std::fabs(inputRight));
    const float detectorSeconds = detector > state.detectorEnvelope
        ? 0.003f : 0.05f;
    const float detectorCoefficient = 1.0f - std::exp(
        -1.0f / (detectorSeconds * state.sampleRate));
    state.detectorEnvelope += detectorCoefficient *
        (detector - state.detectorEnvelope);
    if (state.detectorEnvelope >= threshold)
        state.holdRemaining = holdSeconds;
    const bool open = state.holdRemaining > 0.0f;
    if (open)
        state.holdRemaining = std::max(
            0.0f, state.holdRemaining - 1.0f / state.sampleRate);
    const float gateSeconds = open ? attackSeconds : releaseSeconds;
    const float gateCoefficient = 1.0f - std::exp(
        -1.0f / (gateSeconds * state.sampleRate));
    state.gateGain += gateCoefficient *
        ((open ? 1.0f : 0.0f) - state.gateGain);

    float wetLeft = 0.0f;
    float wetRight = 0.0f;
    reverbSample(state.reverb, inputLeft, inputRight, room, damp,
                 wetLeft, wetRight);
    outputLeft = finiteOr(inputLeft * (1.0f - mix) +
                          wetLeft * state.gateGain * mix);
    outputRight = finiteOr(inputRight * (1.0f - mix) +
                           wetRight * state.gateGain * mix);
}

} // namespace limutfx
