#!/usr/bin/env python3
"""Compile and exercise the dependency-free native player effect functions."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
plugin = root / "spikes/vst3-callback"
source = r"""
#include "native_player_effects.h"
#include <cmath>
#include <limits>
int main() {
    using namespace dexter;
    for (float sample : {-8.0f, -1.0f, -0.25f, 0.0f, 0.25f, 1.0f, 8.0f}) {
        if (processFoxDotShape(sample, 0.0f) != sample ||
            processFoxDotDrive(sample, 0.0f) != sample)
            return 1;
        if (!std::isfinite(processFoxDotShape(sample, 1.0f)) ||
            !std::isfinite(processFoxDotDrive(sample, 1.0f)))
            return 2;
    }
    if (processFoxDotShape(0.5f, 1.0f) == 0.5f ||
        processFoxDotDrive(0.5f, 1.0f) == 0.5f ||
        processFoxDotShape(0.5f, 1.0f) ==
            processFoxDotDrive(0.5f, 1.0f))
        return 3;
    if (processFoxDotShape(1.0f, 100.0f) !=
            processFoxDotShape(1.0f, 4.0f) ||
        processFoxDotDrive(1.0f, -1.0f) != 1.0f)
        return 4;
    if (processFoxDotShape(
            std::numeric_limits<float>::infinity(), 1.0f) != 0.0f)
        return 5;

    float current = 0.0f;
    float previous = current;
    for (int index = 0; index < 2000; ++index) {
        current = smoothPlayerEffectControl(current, 1.0f, 0.0025f);
        if (current < previous || current > 1.0f)
            return 6;
        previous = current;
    }
    if (std::abs(current - 1.0f) >= 0.01f)
        return 7;
    return 0;
}
"""
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    source_file = directory / "player-effects-test.cpp"
    binary = directory / "player-effects-test"
    source_file.write_text(source)
    subprocess.run([
        "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-I", str(plugin), str(source_file), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)

print("PASS: native FoxDot shape/drive range, bypass and smoothing")
