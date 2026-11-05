// Limut-inspired PingPong, Duck, Disperser, Fuzz, Shifter, Shimmer, and
// GatedVerb SuperCollider UGens.
// Adapted from the effect graph concepts in sdclibbery/limut (CC BY-SA 4.0).
#if defined(_WIN32)
#    include <windows.h>
#endif
#include "LimutFX.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

static InterfaceTable* ft = nullptr;
static constexpr float kMaximumDelaySeconds = 2.0f;

static void LimutPingPong_next(LimutPingPong* unit, int sampleFrames) {
    const float delaySeconds = IN0(2);
    const float feedback = IN0(3);
    const float* inputLeft = IN(0);
    const float* inputRight = IN(1);
    float* outputLeft = OUT(0);
    float* outputRight = OUT(1);

    for (int frame = 0; frame < sampleFrames; ++frame) {
        limutfx::pingPongSample(unit->state, inputLeft[frame],
                                inputRight[frame], delaySeconds, feedback,
                                1.0f, outputLeft[frame], outputRight[frame]);
    }
}

static void LimutPingPong_Ctor(LimutPingPong* unit) {
    const double rate = unit->mWorld->mSampleRate;
    const double boundedRate =
        std::isfinite(rate) && rate > 1000.0
            ? std::clamp(rate, 1000.0, 192000.0)
            : 44100.0;
    unit->state.sampleRate = static_cast<float>(boundedRate);
    unit->state.frames = static_cast<uint32_t>(
        std::ceil(unit->state.sampleRate * kMaximumDelaySeconds)) + 2u;
    const size_t bytes =
        static_cast<size_t>(unit->state.frames) * sizeof(float);
    unit->state.left = static_cast<float*>(RTAlloc(unit->mWorld, bytes));
    unit->state.right = static_cast<float*>(RTAlloc(unit->mWorld, bytes));
    if (unit->state.left)
        std::memset(unit->state.left, 0, bytes);
    if (unit->state.right)
        std::memset(unit->state.right, 0, bytes);
    unit->state.writeIndex = 0;
    SETCALC(LimutPingPong_next);
    LimutPingPong_next(unit, 1);
}

static void LimutPingPong_Dtor(LimutPingPong* unit) {
    if (unit->state.left)
        RTFree(unit->mWorld, unit->state.left);
    if (unit->state.right)
        RTFree(unit->mWorld, unit->state.right);
    unit->state.left = nullptr;
    unit->state.right = nullptr;
    unit->state.frames = 0;
}

static void LimutDuck_next(LimutDuck* unit, int sampleFrames) {
    const float period = IN0(2);
    const float depth = IN0(3);
    const float hold = IN0(4);
    const float attack = IN0(5);
    const float* inputLeft = IN(0);
    const float* inputRight = IN(1);
    float* outputLeft = OUT(0);
    float* outputRight = OUT(1);

    for (int frame = 0; frame < sampleFrames; ++frame) {
        limutfx::duckSample(unit->state, inputLeft[frame], inputRight[frame],
                            period, depth, hold, attack, 1.0f,
                            outputLeft[frame], outputRight[frame]);
    }
}

static void LimutDuck_Ctor(LimutDuck* unit) {
    const double rate = unit->mWorld->mSampleRate;
    unit->state.sampleRate =
        std::isfinite(rate) && rate > 1000.0
            ? static_cast<float>(std::clamp(rate, 1000.0, 192000.0))
            : 44100.0f;
    unit->state.phaseSeconds = 0.0f;
    SETCALC(LimutDuck_next);
    LimutDuck_next(unit, 1);
}

static float sampleRateFor(Unit* unit) {
    const double rate = unit->mWorld->mSampleRate;
    return std::isfinite(rate) && rate > 1000.0
        ? static_cast<float>(std::clamp(rate, 1000.0, 192000.0))
        : 44100.0f;
}

static void LimutDisperser_next(LimutDisperser* unit, int sampleFrames) {
    const float frequency = IN0(2);
    const float q = IN0(3);
    const float stages = IN0(4);
    const float mix = IN0(5);
    const float* inputLeft = IN(0);
    const float* inputRight = IN(1);
    float* outputLeft = OUT(0);
    float* outputRight = OUT(1);

    for (int frame = 0; frame < sampleFrames; ++frame) {
        limutfx::disperserSample(
            unit->state, inputLeft[frame], inputRight[frame], frequency, q,
            stages, mix, outputLeft[frame], outputRight[frame]);
    }
}

static void LimutDisperser_Ctor(LimutDisperser* unit) {
    unit->state.sampleRate = sampleRateFor(unit);
    unit->state.initialized = false;
    for (auto& channel : unit->state.sections)
        for (auto& stage : channel)
            stage = {};
    SETCALC(LimutDisperser_next);
    LimutDisperser_next(unit, 1);
}

static void LimutFuzz_next(LimutFuzz* unit, int sampleFrames) {
    const float driveDb = IN0(2);
    const float toneHz = IN0(3);
    const float mix = IN0(4);
    const float* inputLeft = IN(0);
    const float* inputRight = IN(1);
    float* outputLeft = OUT(0);
    float* outputRight = OUT(1);

    for (int frame = 0; frame < sampleFrames; ++frame) {
        limutfx::fuzzSample(unit->state, inputLeft[frame], inputRight[frame],
                            driveDb, toneHz, mix, outputLeft[frame],
                            outputRight[frame]);
    }
}

static void LimutFuzz_Ctor(LimutFuzz* unit) {
    unit->state.sampleRate = sampleRateFor(unit);
    unit->state.currentToneHz = 9000.0f;
    unit->state.lowpassLeft = 0.0f;
    unit->state.lowpassRight = 0.0f;
    unit->state.initialized = false;
    SETCALC(LimutFuzz_next);
    LimutFuzz_next(unit, 1);
}

static float boundedSampleRate(Unit* unit) {
    const double rate = unit->mWorld->mSampleRate;
    return std::isfinite(rate) && rate > 1000.0
        ? static_cast<float>(std::clamp(rate, 1000.0, 192000.0))
        : 44100.0f;
}

static void allocatePitchBuffer(Unit* unit, limutfx::ShifterState& state) {
    limutfx::preparePitchBuffer(state);
    const auto bytes = static_cast<std::size_t>(state.frames) * sizeof(float);
    state.left = static_cast<float*>(RTAlloc(unit->mWorld, bytes));
    state.right = static_cast<float*>(RTAlloc(unit->mWorld, bytes));
    if (state.left)
        std::memset(state.left, 0, bytes);
    if (state.right)
        std::memset(state.right, 0, bytes);
}

static void releasePitchBuffer(Unit* unit, limutfx::ShifterState& state) {
    if (state.left)
        RTFree(unit->mWorld, state.left);
    if (state.right)
        RTFree(unit->mWorld, state.right);
    state.left = nullptr;
    state.right = nullptr;
    state.frames = 0;
}

static void allocateReverb(Unit* unit, limutfx::ReverbState& state) {
    limutfx::prepareReverb(state);
    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t comb = 0; comb < limutfx::kReverbCombCount; ++comb) {
            const auto bytes = static_cast<std::size_t>(
                state.lengths[channel][comb]) * sizeof(float);
            state.buffers[channel][comb] =
                static_cast<float*>(RTAlloc(unit->mWorld, bytes));
            if (state.buffers[channel][comb])
                std::memset(state.buffers[channel][comb], 0, bytes);
        }
    }
}

static void releaseReverb(Unit* unit, limutfx::ReverbState& state) {
    for (auto& channel : state.buffers) {
        for (auto& buffer : channel) {
            if (buffer)
                RTFree(unit->mWorld, buffer);
            buffer = nullptr;
        }
    }
    limutfx::freeReverb(state);
}

static void LimutShifter_next(LimutShifter* unit, int sampleFrames) {
    const float mix = IN0(2);
    const float ratio = IN0(3);
    const float windowSeconds = IN0(4);
    const float* inputLeft = IN(0);
    const float* inputRight = IN(1);
    float* outputLeft = OUT(0);
    float* outputRight = OUT(1);
    for (int frame = 0; frame < sampleFrames; ++frame)
        limutfx::shifterSample(unit->state, inputLeft[frame],
            inputRight[frame], mix, ratio, windowSeconds,
            outputLeft[frame], outputRight[frame]);
}

static void LimutShifter_Ctor(LimutShifter* unit) {
    unit->state.sampleRate = boundedSampleRate(unit);
    allocatePitchBuffer(unit, unit->state);
    SETCALC(LimutShifter_next);
    LimutShifter_next(unit, 1);
}

static void LimutShifter_Dtor(LimutShifter* unit) {
    releasePitchBuffer(unit, unit->state);
}

static void LimutShimmer_next(LimutShimmer* unit, int sampleFrames) {
    const float mix = IN0(2);
    const float ratio = IN0(3);
    const float windowSeconds = IN0(4);
    const float room = IN0(5);
    const float damp = IN0(6);
    const float* inputLeft = IN(0);
    const float* inputRight = IN(1);
    float* outputLeft = OUT(0);
    float* outputRight = OUT(1);
    for (int frame = 0; frame < sampleFrames; ++frame)
        limutfx::shimmerSample(unit->state, inputLeft[frame],
            inputRight[frame], mix, ratio, windowSeconds, room, damp,
            outputLeft[frame], outputRight[frame]);
}

static void LimutShimmer_Ctor(LimutShimmer* unit) {
    unit->state.sampleRate = boundedSampleRate(unit);
    unit->state.pitch.sampleRate = unit->state.sampleRate;
    unit->state.reverb.sampleRate = unit->state.sampleRate;
    allocatePitchBuffer(unit, unit->state.pitch);
    allocateReverb(unit, unit->state.reverb);
    SETCALC(LimutShimmer_next);
    LimutShimmer_next(unit, 1);
}

static void LimutShimmer_Dtor(LimutShimmer* unit) {
    releasePitchBuffer(unit, unit->state.pitch);
    releaseReverb(unit, unit->state.reverb);
}

static void LimutGatedVerb_next(LimutGatedVerb* unit, int sampleFrames) {
    const float mix = IN0(2);
    const float room = IN0(3);
    const float damp = IN0(4);
    const float threshold = IN0(5);
    const float holdSeconds = IN0(6);
    const float attackSeconds = IN0(7);
    const float releaseSeconds = IN0(8);
    const float* inputLeft = IN(0);
    const float* inputRight = IN(1);
    float* outputLeft = OUT(0);
    float* outputRight = OUT(1);
    for (int frame = 0; frame < sampleFrames; ++frame)
        limutfx::gatedVerbSample(unit->state, inputLeft[frame],
            inputRight[frame], mix, room, damp, threshold, holdSeconds,
            attackSeconds, releaseSeconds, outputLeft[frame],
            outputRight[frame]);
}

static void LimutGatedVerb_Ctor(LimutGatedVerb* unit) {
    unit->state.sampleRate = boundedSampleRate(unit);
    unit->state.reverb.sampleRate = unit->state.sampleRate;
    allocateReverb(unit, unit->state.reverb);
    SETCALC(LimutGatedVerb_next);
    LimutGatedVerb_next(unit, 1);
}

static void LimutGatedVerb_Dtor(LimutGatedVerb* unit) {
    releaseReverb(unit, unit->state.reverb);
}

static void LimutCompressor_next(LimutCompressor* unit, int sampleFrames) {
    const float mix = IN0(2);
    const float thresholdDb = IN0(3);
    const float ratioDb = IN0(4);
    const float attackSeconds = IN0(5);
    const float releaseSeconds = IN0(6);
    const float kneeDb = IN0(7);
    const float* inputLeft = IN(0);
    const float* inputRight = IN(1);
    float* outputLeft = OUT(0);
    float* outputRight = OUT(1);
    for (int frame = 0; frame < sampleFrames; ++frame)
        limutfx::compressorSample(unit->state, inputLeft[frame],
            inputRight[frame], mix, thresholdDb, ratioDb, attackSeconds,
            releaseSeconds, kneeDb, outputLeft[frame], outputRight[frame]);
}

static void LimutCompressor_Ctor(LimutCompressor* unit) {
    unit->state.sampleRate = boundedSampleRate(unit);
    SETCALC(LimutCompressor_next);
    LimutCompressor_next(unit, 1);
}

static void LimutVocoder_next(LimutVocoder* unit, int sampleFrames) {
    const float mix = IN0(4);
    const float lowHz = IN0(5);
    const float highHz = IN0(6);
    const float attackSeconds = IN0(7);
    const float releaseSeconds = IN0(8);
    const float tilt = IN0(9);
    const float boost = IN0(10);
    const float* carrierLeft = IN(0);
    const float* carrierRight = IN(1);
    const float* modulatorLeft = IN(2);
    const float* modulatorRight = IN(3);
    float* outputLeft = OUT(0);
    float* outputRight = OUT(1);
    limutfx::updateVocoderBands(unit->state, lowHz, highHz, tilt);
    for (int frame = 0; frame < sampleFrames; ++frame)
        limutfx::vocoderSample(unit->state, carrierLeft[frame],
            carrierRight[frame], modulatorLeft[frame], modulatorRight[frame],
            mix, attackSeconds, releaseSeconds, boost, outputLeft[frame],
            outputRight[frame]);
}

static void LimutVocoder_Ctor(LimutVocoder* unit) {
    unit->state.sampleRate = boundedSampleRate(unit);
    limutfx::updateVocoderBands(unit->state, 120.0f, 8000.0f, 0.0f);
    SETCALC(LimutVocoder_next);
    LimutVocoder_next(unit, 1);
}

PluginLoad(LimutFX) {
    ft = inTable;
    DefineDtorCantAliasUnit(LimutPingPong);
    DefineSimpleCantAliasUnit(LimutDuck);
    DefineSimpleCantAliasUnit(LimutDisperser);
    DefineSimpleCantAliasUnit(LimutFuzz);
    DefineDtorCantAliasUnit(LimutShifter);
    DefineDtorCantAliasUnit(LimutShimmer);
    DefineDtorCantAliasUnit(LimutGatedVerb);
    DefineSimpleCantAliasUnit(LimutCompressor);
    DefineSimpleCantAliasUnit(LimutVocoder);
}
