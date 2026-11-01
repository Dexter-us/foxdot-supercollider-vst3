#pragma once

#include <algorithm>
#include <cmath>

constexpr float dexterMasterGainFromPercent(int percent) noexcept {
    return static_cast<float>(std::clamp(percent, 0, 200)) / 100.0f;
}

static_assert(dexterMasterGainFromPercent(-1) == 0.0f);
static_assert(dexterMasterGainFromPercent(100) == 1.0f);
static_assert(dexterMasterGainFromPercent(201) == 2.0f);

inline float dexterMasterGainStep(float current, float target,
                                  double sampleRate) noexcept {
    if (!std::isfinite(current))
        current = 1.0f;
    if (!std::isfinite(target))
        target = 1.0f;
    target = std::clamp(target, 0.0f, 2.0f);
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0)
        return target;
    const double coefficient = -std::expm1(-1.0 / (0.01 * sampleRate));
    return current + (target - current) * static_cast<float>(coefficient);
}
