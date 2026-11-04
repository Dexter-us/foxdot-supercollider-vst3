#pragma once

#include <algorithm>
#include <cmath>

namespace dexter {

// FoxDot's slideTo, slideFrom, and pitchBend SynthDefs multiply a note's
// signal by these per-note EnvGen curves. The controls therefore shape gain;
// they do not retune the oscillator, despite the shorthand in some docs.
inline float foxDotNoteEffectGain(double progress, float slide,
                                  float slideFrom, float slideDelay,
                                  float bend, float bendDelay) {
    if (!std::isfinite(progress))
        return 1.0f;
    const double t = std::clamp(progress, 0.0, 1.0);
    const auto ramp = [](double position, double start, double end) {
        if (end <= start)
            return 1.0;
        return std::clamp((position - start) / (end - start), 0.0, 1.0);
    };
    double gain = 1.0;
    if (slide != 0.0f && t > slideDelay) {
        gain *= 1.0 + static_cast<double>(slide) *
            ramp(t, slideDelay, 1.0);
    }
    if (slideFrom != 0.0f) {
        gain *= t <= slideDelay
            ? 1.0 + static_cast<double>(slideFrom)
            : 1.0 + static_cast<double>(slideFrom) *
                (1.0 - ramp(t, slideDelay, 1.0));
    }
    if (bend != 0.0f && t > bendDelay && bendDelay < 1.0f) {
        const double midpoint = bendDelay +
            (1.0 - static_cast<double>(bendDelay)) * 0.5;
        const double amount = t <= midpoint
            ? ramp(t, bendDelay, midpoint)
            : 1.0 - ramp(t, midpoint, 1.0);
        gain *= 1.0 + static_cast<double>(bend) * amount;
    }
    return std::isfinite(gain)
        ? static_cast<float>(std::clamp(gain, 0.0, 125.0))
        : 1.0f;
}

// Mirrors FoxDot's chop.scd: LFPulse.kr(chop / sus, add: 0.01).
// progress is normalized over the note sustain, so `chop` is the number
// of half-on/half-off cycles across that sustain.
inline float foxDotChopGain(double progress, float chop) {
    if (!std::isfinite(progress) || !std::isfinite(chop) || chop <= 0.0f)
        return 1.0f;
    const double phase = std::fmod(std::clamp(progress, 0.0, 1.0) * chop, 1.0);
    return phase < 0.5 ? 1.01f : 0.01f;
}

// FoxDot's coarse.scd gates a note with LFPulse.ar(coarse / sus).
// `progress` is normalized to the note sustain, so coarse is the number of
// pulse cycles across that sustain. Zero leaves the note untouched.
inline float foxDotCoarseGain(double progress, float coarse) {
    if (!std::isfinite(progress) || !std::isfinite(coarse) || coarse <= 0.0f)
        return 1.0f;
    const double phase = std::fmod(std::max(0.0, progress) * coarse, 1.0);
    return phase < 0.5 ? 1.0f : 0.0f;
}

struct FoxDotFormantState {
    double decayY1 = 0.0;
    double decayY2 = 0.0;
    double attackY1 = 0.0;
    double attackY2 = 0.0;
    double preparedFormant = -2.0;
    double preparedSampleRate = 0.0;
    double decayInput = 1.0;
    double decayCoefficient1 = 0.0;
    double decayCoefficient2 = 0.0;
    double attackInput = 1.0;
    double attackCoefficient1 = 0.0;
    double attackCoefficient2 = 0.0;
};

// FoxDot's formantFilter.scd maps the control to a Formlet UGen and applies
// tanh. SuperCollider defines Formlet as Ringz(decay) - Ringz(attack); this
// bounded state-variable implementation follows that resonator form.
inline float foxDotFormantSample(FoxDotFormantState& state, float input,
                                 float formant, double sampleRate) {
    if (!std::isfinite(input) || !std::isfinite(formant) ||
        !std::isfinite(sampleRate) || formant < 0.0f || formant > 7.0f ||
        sampleRate < 8000.0 || sampleRate > 384000.0)
        return std::isfinite(input) ? input : 0.0f;
    if (state.preparedFormant != formant ||
        state.preparedSampleRate != sampleRate) {
        const double index = static_cast<double>(formant) + 1.0;
        const double frequency = std::clamp(index * 200.0, 20.0,
                                            sampleRate * 0.45);
        const double attackSeconds = (std::fmod(index, 5.0) + 1.0) / 1000.0;
        const double decaySeconds = index * 1.5 / 600.0;
        const double omega = 2.0 * 3.14159265358979323846 *
                             frequency / sampleRate;
        const double cosine = 2.0 * std::cos(omega);
        const auto prepare = [&](double decay, double& inputCoefficient,
                                 double& coefficient1, double& coefficient2) {
            const double radius = std::exp(-std::log(1000.0) /
                                           (decay * sampleRate));
            inputCoefficient = 1.0 - radius;
            coefficient1 = radius * cosine;
            coefficient2 = radius * radius;
        };
        prepare(decaySeconds, state.decayInput,
                state.decayCoefficient1, state.decayCoefficient2);
        prepare(attackSeconds, state.attackInput,
                state.attackCoefficient1, state.attackCoefficient2);
        state.preparedFormant = formant;
        state.preparedSampleRate = sampleRate;
    }
    const double boundedInput =
        std::clamp(static_cast<double>(input), -64.0, 64.0);
    const double decayOutput = state.decayInput * boundedInput +
        state.decayCoefficient1 * state.decayY1 -
        state.decayCoefficient2 * state.decayY2;
    state.decayY2 = state.decayY1;
    state.decayY1 = std::isfinite(decayOutput)
        ? std::clamp(decayOutput, -10000.0, 10000.0) : 0.0;
    const double attackOutput = state.attackInput * boundedInput +
        state.attackCoefficient1 * state.attackY1 -
        state.attackCoefficient2 * state.attackY2;
    state.attackY2 = state.attackY1;
    state.attackY1 = std::isfinite(attackOutput)
        ? std::clamp(attackOutput, -10000.0, 10000.0) : 0.0;
    const double output = state.decayY1 - state.attackY1;
    return static_cast<float>(std::tanh(std::clamp(output, -64.0, 64.0)));
}

struct FoxDotSpinGains {
    float left = 1.0f;
    float right = 1.0f;
};

// Mirrors FoxDot's spinPan.scd: two FSinOscs at spin/2 Hz, with initial
// phases 1 and 3 radians and outputs scaled from 0 to 1.
inline FoxDotSpinGains foxDotSpinGains(double elapsedSeconds, float spin) {
    if (!std::isfinite(elapsedSeconds) || !std::isfinite(spin) ||
        spin <= 0.0f)
        return {};
    const double phase = 2.0 * 3.14159265358979323846 *
        (static_cast<double>(spin) * 0.5) *
        std::max(0.0, elapsedSeconds);
    return {
        static_cast<float>(0.5 + 0.5 * std::sin(phase + 1.0)),
        static_cast<float>(0.5 + 0.5 * std::sin(phase + 3.0))
    };
}

} // namespace dexter
