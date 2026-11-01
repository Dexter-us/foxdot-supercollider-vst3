/* Airwindows DSP is retained in the original processor source files.
 * This file only adapts their stereo processReplacing entry points to SC.
 * Copyright (c) Airwindows. MIT License; see LICENSE-Airwindows.txt.
 */
#include "Echojam.h"
#include "TapeDelay2.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

static InterfaceTable* ft;

static inline float boundedParameter(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

static uint32_t ditherSeed(const Unit* unit, uint32_t salt) {
    auto value = static_cast<uint32_t>(
        reinterpret_cast<std::uintptr_t>(unit) >> 4) ^ salt;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    if (value < 16386u)
        value += 16386u;
    return value;
}

static void Echojam_next(Echojam* unit, int sampleFrames) {
    unit->A = boundedParameter(IN0(2));
    unit->B = boundedParameter(IN0(3));
    unit->C = boundedParameter(IN0(4));
    unit->D = boundedParameter(IN0(5));

    float* inputs[2] = {
        const_cast<float*>(IN(0)),
        const_cast<float*>(IN(1))
    };
    float* outputs[2] = { OUT(0), OUT(1) };
    unit->processReplacing(inputs, outputs, sampleFrames);
}

static void Echojam_Ctor(Echojam* unit) {
    std::memset(static_cast<EchojamState*>(unit), 0, sizeof(EchojamState));
    unit->A = 0.5f;
    unit->B = 0.5f;
    unit->C = 0.5f;
    unit->D = 0.5f;

    unit->cLA = unit->cLB = unit->cLC = unit->cLD =
        unit->cLE = unit->cLF = unit->cLG = unit->cLH = 1;
    unit->cLI = unit->cLJ = unit->cLK = unit->cLL =
        unit->cLM = unit->cLN = unit->cLO = unit->cLP = 1;
    unit->cRA = unit->cRB = unit->cRC = unit->cRD =
        unit->cRE = unit->cRF = unit->cRG = unit->cRH = 1;
    unit->cRI = unit->cRJ = unit->cRK = unit->cRL =
        unit->cRM = unit->cRN = unit->cRO = unit->cRP = 1;

    unit->bez[EchojamState::bez_cycle] = 1.0;
    unit->shortA = 17; unit->shortB = 19; unit->shortC = 23; unit->shortD = 29;
    unit->shortE = 19; unit->shortF = 106; unit->shortG = 41; unit->shortH = 6361;
    unit->shortI = 47; unit->shortJ = 53; unit->shortK = 59; unit->shortL = 61;
    unit->shortM = 67; unit->shortN = 5824; unit->shortO = 6257; unit->shortP = 653;
    unit->prevclearcoat = -1;
    unit->fpdL = ditherSeed(unit, 0x6d2b79f5u);
    unit->fpdR = ditherSeed(unit, 0x1b56c4e9u);
    SETCALC(Echojam_next);
    Echojam_next(unit, 1);
}

static void TapeDelay2_next(TapeDelay2* unit, int sampleFrames) {
    unit->A = boundedParameter(IN0(2));
    unit->B = boundedParameter(IN0(3));
    unit->C = boundedParameter(IN0(4));
    unit->D = boundedParameter(IN0(5));
    unit->E = boundedParameter(IN0(6));
    unit->F = boundedParameter(IN0(7));

    float* inputs[2] = {
        const_cast<float*>(IN(0)),
        const_cast<float*>(IN(1))
    };
    float* outputs[2] = { OUT(0), OUT(1) };
    unit->processReplacing(inputs, outputs, sampleFrames);
}

static void TapeDelay2_Ctor(TapeDelay2* unit) {
    std::memset(static_cast<TapeDelay2State*>(unit), 0, sizeof(TapeDelay2State));
    unit->A = 1.0f;
    unit->B = 0.0f;
    unit->C = 0.5f;
    unit->D = 0.0f;
    unit->E = 0.0f;
    unit->F = 1.0f;
    unit->fpdL = ditherSeed(unit, 0x2c9277b5u);
    unit->fpdR = ditherSeed(unit, 0x4f1bbcdcu);
    SETCALC(TapeDelay2_next);
    TapeDelay2_next(unit, 1);
}

PluginLoad(AirwindowsFX) {
    ft = inTable;
    DefineSimpleCantAliasUnit(TapeDelay2);
    DefineSimpleCantAliasUnit(Echojam);
}
