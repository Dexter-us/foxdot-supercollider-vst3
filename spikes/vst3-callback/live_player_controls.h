#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

enum class EditorPlayerControl : std::uint8_t {
    Amplitude,
    Pan,
    Cutoff,
    EffectMix
};

inline float clampEditorPlayerControl(EditorPlayerControl control, float value) {
    switch (control) {
    case EditorPlayerControl::Amplitude:
    case EditorPlayerControl::EffectMix:
        return std::clamp(value, 0.0f, 1.0f);
    case EditorPlayerControl::Pan:
        return std::clamp(value, -1.0f, 1.0f);
    case EditorPlayerControl::Cutoff:
        return std::clamp(value, 20.0f, 20000.0f);
    }
    return 0.0f;
}

inline float smoothEditorPlayerControl(float current, float target,
                                       float smoothing) {
    if (!std::isfinite(current) || !std::isfinite(target) ||
        !std::isfinite(smoothing))
        return 0.0f;
    const float alpha = std::clamp(smoothing, 0.0f, 1.0f);
    return current + alpha * (target - current);
}
