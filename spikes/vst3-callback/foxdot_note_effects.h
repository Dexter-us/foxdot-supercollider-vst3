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
