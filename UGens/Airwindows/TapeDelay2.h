/* Airwindows TapeDelay2 state adapted to SuperCollider's Unit ABI.
 * DSP processing is compiled unchanged from the pinned upstream Proc.cpp.
 * Copyright (c) Airwindows. MIT License; see LICENSE-Airwindows.txt.
 */
#ifndef __TapeDelay2_H
#define __TapeDelay2_H

#include "AirwindowsCompat.h"

#include <cstdint>

struct TapeDelay2State {
    double dL[88211];
    double prevSampleL;
    double delayL;
    double sweepL;
    double regenFilterL[9];
    double outFilterL[9];
    double lastRefL[10];

    double dR[88211];
    double prevSampleR;
    double delayR;
    double sweepR;
    double regenFilterR[9];
    double outFilterR[9];
    double lastRefR[10];

    int cycle;
    uint32_t fpdL;
    uint32_t fpdR;

    float A;
    float B;
    float C;
    float D;
    float E;
    float F;
};

struct TapeDelay2 : Unit, TapeDelay2State {
    double getSampleRate() const { return mRate->mSampleRate; }
    void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
    void processDoubleReplacing(double** inputs, double** outputs, VstInt32 sampleFrames);
};

#endif
