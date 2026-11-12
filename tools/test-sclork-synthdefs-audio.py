#!/usr/bin/env python3
"""Optional native scsynth NRT smoke test for the SCLOrk insert SynthDefs."""
import math
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "spikes/vst3-callback"
DEFS = PLUGIN / "Portable/SynthDefs"
helpers = runpy.run_path(str(ROOT / "tools/test-core-synth-audio.py"))
bundle, message, read_wav = (
    helpers["bundle"], helpers["message"], helpers["read_float_wav"])

CONTROLS = {
    "SCLOrkPhazerFX": [
        2, 0.7, 3, 0.28, 4, 0.75, 5, 620.0, 6, 0.9, 7, 0.22, 8, 11000.0],
    "SCLOrkDecimatorFX": [
        2, 0.75, 3, 3200.0, 4, 5.0, 5, 1.5, 6, 7500.0],
    "SCLOrkCrossoverDistortionFX": [
        2, 0.65, 3, 2.6, 4, 0.8, 5, 8500.0],
}


def main():
    sclang = shutil.which("sclang")
    scsynth = shutil.which("scsynth")
    if not sclang or not scsynth:
        raise SystemExit("sclang and scsynth are required for this optional NRT test")
    stock_plugins = str(
        Path(scsynth).resolve().parent.parent / "lib/SuperCollider/plugins")
    with tempfile.TemporaryDirectory(prefix="sclork-insert-nrt-") as tmp:
        tmp = Path(tmp)
        source_file = tmp / "TestToneBus.scsyndef"
        source_script = tmp / "build-source.scd"
        source_script.write_text(
            'SynthDef(\\TestToneBus, { |out=16| '
            'Out.ar(out, SinOsc.ar([220, 331]) * 0.15) '
            '}).writeDefFile("' + str(tmp).replace("\\", "/") +
            '"); 0.exit;\n',
            encoding="utf-8")
        compiled = subprocess.run(
            [sclang, str(source_script)], capture_output=True, text=True,
            timeout=50,
            env=dict(os.environ, QT_QPA_PLATFORM="minimal",
                     QTWEBENGINE_DISABLE_SANDBOX="1"))
        assert compiled.returncode == 0 and "ERROR:" not in compiled.stdout, (
            compiled.stdout + compiled.stderr)
        source = source_file.read_bytes()

        def render(name, mix):
            controls = CONTROLS[name].copy()
            controls[1] = mix
            score = bundle(
                0, message("/d_recv", source),
                message("/d_recv", (DEFS / (name + ".scsyndef")).read_bytes()))
            score += bundle(
                0.01, message("/s_new", "TestToneBus", 100, 0, 0, "out", 16))
            score += bundle(
                0.01, message("/s_new", name, 101, 1, 0,
                              "out", 0, "inbus", 16, *controls))
            score += bundle(0.5, message("/n_free", 100, 101))
            score += bundle(0.6, message("/c_set", 0, 0.0))
            score_file = tmp / (name + f"-{mix}.osc")
            wav_file = tmp / (name + f"-{mix}.wav")
            score_file.write_bytes(score)
            result = subprocess.run(
                [scsynth, "-N", str(score_file), "_", str(wav_file),
                 "48000", "WAV", "float", "-o", "2", "-i", "0",
                 "-z", "64", "-U", stock_plugins],
                capture_output=True, text=True, timeout=60)
            log = result.stdout + result.stderr
            assert result.returncode == 0 and "FAILURE" not in log, log
            samples, channels, _ = read_wav(wav_file)
            assert channels == 2 and all(math.isfinite(value) for value in samples)
            peak = max(abs(value) for value in samples)
            assert 1e-4 < peak < 1.0, (name, "unsafe/silent peak", peak)
            return samples, peak

        for name in CONTROLS:
            dry, _ = render(name, 0.0)
            wet, peak = render(name, 0.7)
            difference = max(abs(a - b) for a, b in zip(dry, wet))
            assert difference > 1e-4, (name, "wet insert made no change")
            print(f"PASS {name}: stereo finite render, peak={peak:.4f}, "
                  f"wet/dry difference={difference:.5f}")


if __name__ == "__main__":
    main()
