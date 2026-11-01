#pragma once

#include <algorithm>
#include <cmath>

constexpr int kDexterMasterGainMaximumPercent = 250;
constexpr float kDexterMasterGainMaximum = 2.5f;

constexpr float dexterMasterGainFromPercent(int percent) noexcept {
    return static_cast<float>(
               std::clamp(percent, 0, kDexterMasterGainMaximumPercent)) /
           100.0f;
}

static_assert(dexterMasterGainFromPercent(-1) == 0.0f);
static_assert(dexterMasterGainFromPercent(100) == 1.0f);
static_assert(dexterMasterGainFromPercent(250) == 2.5f);
static_assert(dexterMasterGainFromPercent(251) == 2.5f);

inline float dexterMasterGainStep(float current, float target,
                                  double sampleRate) noexcept {
    if (!std::isfinite(current))
        current = 1.0f;
    if (!std::isfinite(target))
        target = 1.0f;
    target = std::clamp(target, 0.0f, kDexterMasterGainMaximum);
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0)
        return target;
    const double coefficient = -std::expm1(-1.0 / (0.01 * sampleRate));
    return current + (target - current) * static_cast<float>(coefficient);
}
