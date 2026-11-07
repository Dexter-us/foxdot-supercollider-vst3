#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

enum class EditorPlayerControl : std::uint8_t {
    Amplitude,
    Pan,
    Cutoff,
    EffectMix,
    BitDepth,
    CrushRate,
    PShift
};

inline constexpr bool editorPlayerControlAvailable(
    EditorPlayerControl control, bool effectMixAvailable,
    bool bitcrusherEnabled) noexcept {
    switch (control) {
    case EditorPlayerControl::EffectMix:
        return effectMixAvailable;
    case EditorPlayerControl::BitDepth:
    case EditorPlayerControl::CrushRate:
        return bitcrusherEnabled;
    case EditorPlayerControl::PShift:
        return true;
    case EditorPlayerControl::Amplitude:
    case EditorPlayerControl::Pan:
    case EditorPlayerControl::Cutoff:
        return true;
    }
    return false;
}

inline constexpr bool editorPlayerControlVisible(
    EditorPlayerControl control, bool playersTab, bool hasSelectedPlayer,
    bool bitcrusherEnabled) noexcept {
    if (!playersTab)
        return false;
    if (!hasSelectedPlayer)
        return true;
    if (control == EditorPlayerControl::BitDepth ||
        control == EditorPlayerControl::CrushRate)
        return bitcrusherEnabled;
    return true;
}

inline float clampEditorPlayerControl(EditorPlayerControl control, float value) {
    switch (control) {
    case EditorPlayerControl::Amplitude:
    case EditorPlayerControl::EffectMix:
        return std::clamp(value, 0.0f, 1.0f);
    case EditorPlayerControl::BitDepth:
        return std::clamp(value, 0.0f, 24.0f);
    case EditorPlayerControl::CrushRate:
        return std::clamp(value, 1.0f, 128.0f);
    case EditorPlayerControl::Pan:
        return std::clamp(value, -1.0f, 1.0f);
    case EditorPlayerControl::Cutoff:
        return std::clamp(value, 20.0f, 20000.0f);
    case EditorPlayerControl::PShift:
        return std::clamp(value, -24.0f, 24.0f);
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
