#!/usr/bin/env python3
"""Test the master-output gain range and click-free per-sample smoothing."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
plugin = root / "spikes/vst3-callback"
source = r"""
#include "master_output_gain.h"
#include <cmath>
int main() {
    if (dexterMasterGainFromPercent(-1) != 0.0f ||
        dexterMasterGainFromPercent(0) != 0.0f ||
        dexterMasterGainFromPercent(100) != 1.0f ||
        dexterMasterGainFromPercent(250) != 2.5f ||
        dexterMasterGainFromPercent(251) != 2.5f)
        return 1;
    float value = 1.0f;
    value = dexterMasterGainStep(value, 0.0f, 48000.0);
    if (!(value < 1.0f && value > 0.0f))
        return 2;
    for (int i = 0; i < 4800; ++i)
        value = dexterMasterGainStep(value, 0.0f, 48000.0);
    if (value > 0.001f || value < 0.0f)
        return 3;
    value = dexterMasterGainStep(value, 2.5f, 48000.0);
    if (!(value > 0.0f && value < 2.5f))
        return 4;
    if (dexterMasterGainStep(0.3f, 1.5f, 0.0) != 1.5f)
        return 5;
    if (dexterMasterGainStep(0.3f, 3.0f, 0.0) != 2.5f)
        return 6;
    return 0;
}
"""
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    source_file = directory / "gain-test.cpp"
    binary = directory / "gain-test"
    source_file.write_text(source)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(plugin), str(source_file), "-o", str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)
editor = (plugin / "win32_code_editor.cpp").read_text()
processor = (plugin / "processor.cpp").read_text()
assert "kDexterMasterGainMaximumPercent" in editor
assert "kDexterMasterGainMaximum" in processor
print("PASS: master output gain range and 10 ms click-free smoothing")
