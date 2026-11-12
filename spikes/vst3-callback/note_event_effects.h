#pragma once

#include <array>
#include <cstddef>

inline constexpr std::size_t kMaxNoteEventEffectChain = 2;
inline constexpr std::size_t kMaxEffectControlsPerNoteEvent = 7;

using NoteEventEffectControls =
    std::array<double, kMaxEffectControlsPerNoteEvent>;
using NoteEventEffectControlChain =
    std::array<NoteEventEffectControls, kMaxNoteEventEffectChain>;
using NoteEventEffectIndexChain =
    std::array<int, kMaxNoteEventEffectChain>;

