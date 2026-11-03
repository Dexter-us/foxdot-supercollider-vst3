#pragma once

#include <algorithm>
#include <cmath>

namespace dexter {

// These bounded, dependency-free shapers provide the FoxDot `shape` and
// `drive` controls in the VST's local player insert. They match the control
// names and useful range, but are not bit-for-bit replacements for FoxDot's
// SuperCollider SynthDef implementations.
inline float smoothPlayerEffectControl(float current, float target,
                                       float coefficient) noexcept {
    if (!std::isfinite(current))
        current = 0.0f;
    if (!std::isfinite(target))
        target = current;
    if (!std::isfinite(coefficient))
        coefficient = 0.0f;
    return current + std::clamp(coefficient, 0.0f, 1.0f) * (target - current);
}

inline float processFoxDotShape(float input, float amount) noexcept {
    if (!std::isfinite(input))
        return 0.0f;
    if (!std::isfinite(amount))
        return input;
    const float level = std::clamp(amount, 0.0f, 4.0f);
    if (level <= 0.0f)
        return input;
    const float blend = level / (1.0f + level);
    const float shaped = std::tanh(input * (1.0f + 7.0f * level));
    return input + blend * (shaped - input);
}

inline float processFoxDotDrive(float input, float amount) noexcept {
    if (!std::isfinite(input))
        return 0.0f;
    if (!std::isfinite(amount))
        return input;
    const float level = std::clamp(amount, 0.0f, 4.0f);
    if (level <= 0.0f)
        return input;
    constexpr float halfPi = 1.57079632679489661923f;
    const float blend = level / (1.0f + level);
    const float driven = std::atan(input * (1.0f + 10.0f * level)) /
                         halfPi;
    return input + blend * (driven - input);
}

} // namespace dexter
