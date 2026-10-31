#pragma once

#include <cmath>

// FoxDot's Clock.swing uses a [0, nudge] TimeVar that alternates every
// half-beat. A positive clock nudge advances the scheduled onset, so expose
// the equivalent beat offset for the plugin's sample-accurate scheduler.
inline double foxDotClockSwingOffsetBeats(double swingSeconds, double bpm,
                                          double beat) noexcept {
    if (!std::isfinite(swingSeconds) || !std::isfinite(bpm) ||
        !std::isfinite(beat) || swingSeconds == 0.0 || bpm <= 0.0 || beat < 0.0)
        return 0.0;
    const double halfBeatIndex = std::floor(beat * 2.0);
    return std::fmod(halfBeatIndex, 2.0) >= 1.0
        ? swingSeconds * bpm / 60.0 : 0.0;
}
