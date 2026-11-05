// Limut-inspired PingPong, Duck, Disperser, and Fuzz SuperCollider UGens.
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

PluginLoad(LimutFX) {
    ft = inTable;
    DefineDtorCantAliasUnit(LimutPingPong);
    DefineSimpleCantAliasUnit(LimutDuck);
    DefineSimpleCantAliasUnit(LimutDisperser);
    DefineSimpleCantAliasUnit(LimutFuzz);
}
