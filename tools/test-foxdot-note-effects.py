#!/usr/bin/env python3
"""Test FoxDot per-note amplitude, gating, spin, and formant effect curves."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
plugin = root / "spikes/vst3-callback"
source = r"""
#include "foxdot_note_effects.h"
#include <cmath>

static bool near(float actual, float expected) {
    return std::abs(actual - expected) < 1.0e-5f;
}

int main() {
    using dexter::foxDotNoteEffectGain;
    using dexter::foxDotChopGain;
    using dexter::foxDotCoarseGain;
    using dexter::foxDotFormantSample;
    using dexter::foxDotSpinGains;
    if (!near(foxDotNoteEffectGain(0.0, 0.5f, 0.0f, 0.25f, 0.0f, 0.0f), 1.0f) ||
        !near(foxDotNoteEffectGain(0.25, 0.5f, 0.0f, 0.25f, 0.0f, 0.0f), 1.0f) ||
        !near(foxDotNoteEffectGain(0.625, 0.5f, 0.0f, 0.25f, 0.0f, 0.0f), 1.25f) ||
        !near(foxDotNoteEffectGain(1.0, 0.5f, 0.0f, 0.25f, 0.0f, 0.0f), 1.5f))
        return 1;
    if (!near(foxDotNoteEffectGain(0.0, 0.0f, 0.5f, 0.25f, 0.0f, 0.0f), 1.5f) ||
        !near(foxDotNoteEffectGain(0.25, 0.0f, 0.5f, 0.25f, 0.0f, 0.0f), 1.5f) ||
        !near(foxDotNoteEffectGain(0.625, 0.0f, 0.5f, 0.25f, 0.0f, 0.0f), 1.25f) ||
        !near(foxDotNoteEffectGain(1.0, 0.0f, 0.5f, 0.25f, 0.0f, 0.0f), 1.0f))
        return 2;
    if (!near(foxDotNoteEffectGain(0.2, 0.0f, 0.0f, 0.0f, 0.5f, 0.2f), 1.0f) ||
        !near(foxDotNoteEffectGain(0.6, 0.0f, 0.0f, 0.0f, 0.5f, 0.2f), 1.5f) ||
        !near(foxDotNoteEffectGain(1.0, 0.0f, 0.0f, 0.0f, 0.5f, 0.2f), 1.0f))
        return 3;
    if (!near(foxDotNoteEffectGain(0.5, 0.5f, 0.25f, 0.2f, 0.4f, 0.1f),
              (1.0 + 0.5 * 0.375) * (1.0 + 0.25 * 0.625) *
                  (1.0 + 0.4 * 0.8888888888888888)))
        return 4;
    if (!near(foxDotNoteEffectGain(1.0, 0.5f, 0.0f, 1.0f, 0.5f, 1.0f),
              1.0f) ||
        !near(foxDotNoteEffectGain(0.4, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f),
              0.6f) ||
        foxDotNoteEffectGain(NAN, 0.5f, 0.5f, 0.0f, 0.5f, 0.0f) != 1.0f)
        return 5;
    if (!near(foxDotChopGain(0.0, 4.0f), 1.01f) ||
        !near(foxDotChopGain(0.124, 4.0f), 1.01f) ||
        !near(foxDotChopGain(0.125, 4.0f), 0.01f) ||
        !near(foxDotChopGain(0.25, 4.0f), 1.01f) ||
        !near(foxDotChopGain(0.5, 4.0f), 1.01f) ||
        !near(foxDotChopGain(0.75, 4.0f), 1.01f) ||
        !near(foxDotChopGain(0.5, 0.0f), 1.0f) ||
        foxDotChopGain(NAN, 4.0f) != 1.0f)
        return 6;
    if (!near(foxDotCoarseGain(0.0, 4.0f), 1.0f) ||
        !near(foxDotCoarseGain(0.124, 4.0f), 1.0f) ||
        !near(foxDotCoarseGain(0.125, 4.0f), 0.0f) ||
        !near(foxDotCoarseGain(0.25, 4.0f), 1.0f) ||
        !near(foxDotCoarseGain(0.5, 0.0f), 1.0f) ||
        foxDotCoarseGain(NAN, 4.0f) != 1.0f)
        return 8;
    for (float formant : {0.0f, 3.5f, 7.0f}) {
        dexter::FoxDotFormantState state;
        double energy = 0.0;
        for (int sample = 0; sample < 4096; ++sample) {
            const float input = 0.25f *
                static_cast<float>(std::sin(sample * 0.13));
            const float output = foxDotFormantSample(
                state, input, formant, 48000.0);
            if (!std::isfinite(output) || std::abs(output) > 1.0f)
                return 9;
            energy += std::abs(output);
        }
        if (energy < 1.0)
            return 10;
    }
    dexter::FoxDotFormantState bypass;
    if (foxDotFormantSample(bypass, 0.25f, -1.0f, 48000.0) != 0.25f ||
        foxDotFormantSample(bypass, 0.25f, 2.0f, 4000.0) != 0.25f)
        return 11;
    const auto spinStart = foxDotSpinGains(0.0, 4.0f);
    const auto spinQuarter = foxDotSpinGains(0.25, 4.0f);
    if (!near(spinStart.left,
              static_cast<float>(0.5 + 0.5 * std::sin(1.0))) ||
        !near(spinStart.right,
              static_cast<float>(0.5 + 0.5 * std::sin(3.0))) ||
        !near(spinQuarter.left,
              static_cast<float>(0.5 + 0.5 * std::sin(3.14159265358979323846 + 1.0))) ||
        !near(spinQuarter.right,
              static_cast<float>(0.5 + 0.5 * std::sin(3.14159265358979323846 + 3.0))) ||
        foxDotSpinGains(0.0, 0.0f).left != 1.0f ||
        foxDotSpinGains(0.0, 0.0f).right != 1.0f ||
        foxDotSpinGains(NAN, 4.0f).left != 1.0f)
        return 7;
    return 0;
}
"""
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    source_file = directory / "note-effects.cpp"
    binary = directory / "note-effects"
    source_file.write_text(source)
    subprocess.run([
        "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-I", str(plugin), str(source_file), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)

print("PASS: FoxDot slide/bend envelopes, chop/coarse gates, spin, and Formlet-style formant curves")
