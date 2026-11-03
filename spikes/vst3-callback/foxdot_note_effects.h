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

} // namespace dexter
