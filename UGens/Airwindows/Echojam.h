/* Airwindows Echojam state adapted to SuperCollider's Unit ABI.
 * DSP processing is compiled unchanged from the pinned upstream Proc.cpp.
 * Copyright (c) Airwindows. MIT License; see LICENSE-Airwindows.txt.
 */
#ifndef __Echojam_H
#define __Echojam_H

#include "AirwindowsCompat.h"

#include <cstdint>

constexpr int d4A = 6654; constexpr int d4B = 7450; constexpr int d4C = 6998; constexpr int d4D = 6650;
constexpr int d4E = 7136; constexpr int d4F = 7198; constexpr int d4G = 7253; constexpr int d4H = 7221;
constexpr int d4I = 7355; constexpr int d4J = 4838; constexpr int d4K = 7069; constexpr int d4L = 7175;
constexpr int d4M = 7307; constexpr int d4N = 6819; constexpr int d4O = 7104; constexpr int d4P = 7443;

struct EchojamState {
    float A;
    float B;
    float C;
    float D;

    double lA[d4A + 5]; double lB[d4B + 5]; double lC[d4C + 5]; double lD[d4D + 5];
    double lE[d4E + 5]; double lF[d4F + 5]; double lG[d4G + 5]; double lH[d4H + 5];
    double lI[d4I + 5]; double lJ[d4J + 5]; double lK[d4K + 5]; double lL[d4L + 5];
    double lM[d4M + 5]; double lN[d4N + 5]; double lO[d4O + 5]; double lP[d4P + 5];

    double rA[d4A + 5]; double rB[d4B + 5]; double rC[d4C + 5]; double rD[d4D + 5];
    double rE[d4E + 5]; double rF[d4F + 5]; double rG[d4G + 5]; double rH[d4H + 5];
    double rI[d4I + 5]; double rJ[d4J + 5]; double rK[d4K + 5]; double rL[d4L + 5];
    double rM[d4M + 5]; double rN[d4N + 5]; double rO[d4O + 5]; double rP[d4P + 5];

    int cLA, cLB, cLC, cLD, cLE, cLF, cLG, cLH;
    int cLI, cLJ, cLK, cLL, cLM, cLN, cLO, cLP;
    int cRA, cRB, cRC, cRD, cRE, cRF, cRG, cRH;
    int cRI, cRJ, cRK, cRL, cRM, cRN, cRO, cRP;

    double fAL, fBL, fCL, fDL;
    double hAL, hBL, hCL, hDL, hEL;
    double fDR, fHR, fLR, fPR;
    double hAR, hBR, hCR, hDR, hER;

    enum {
        bez_AL, bez_AR, bez_BL, bez_BR, bez_CL, bez_CR,
        bez_SampL, bez_SampR, bez_cycle, bez_total
    };
    double bez[bez_total];

    int shortA, shortB, shortC, shortD, shortE, shortF, shortG, shortH;
    int shortI, shortJ, shortK, shortL, shortM, shortN, shortO, shortP;

    int prevclearcoat;
    uint32_t fpdL;
    uint32_t fpdR;
};

struct Echojam : Unit, EchojamState {
    double getSampleRate() const { return mRate->mSampleRate; }
    void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
    void processDoubleReplacing(double** inputs, double** outputs, VstInt32 sampleFrames);
};

#endif
