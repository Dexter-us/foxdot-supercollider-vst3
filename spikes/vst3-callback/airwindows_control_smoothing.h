#pragma once

#include <algorithm>
#include <cmath>

// Smooth normalized FoxDot var() changes before they reach block-rate
// persistent player-insert controls (Airwindows, reverbs, Chorus, Flanger).
// linvar() targets use the same short smoothing so block
// boundaries do not introduce parameter steps.
inline float smoothAirwindowsFxControl(float previous, float target,
                                       double blockSeconds) noexcept {
    constexpr double kTimeConstantSeconds = 0.015;
    if (!std::isfinite(target))
        return 0.0f;
    target = std::clamp(target, 0.0f, 1.0f);
    if (!std::isfinite(previous) || !std::isfinite(blockSeconds) ||
        blockSeconds <= 0.0)
        return target;
    const double alpha = 1.0 - std::exp(-blockSeconds / kTimeConstantSeconds);
    const float result = previous + static_cast<float>(alpha) * (target - previous);
    return std::clamp(result, 0.0f, 1.0f);
}
