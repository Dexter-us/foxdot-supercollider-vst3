#!/usr/bin/env python3
"""Test the per-note FoxDot slide/slidefrom/bend envelope curves."""
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

print("PASS: FoxDot slide/slidefrom/bend per-note envelope curves")
