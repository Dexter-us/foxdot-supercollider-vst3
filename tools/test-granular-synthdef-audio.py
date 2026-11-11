#!/usr/bin/env python3
"""Render all four DexterGranularFX modes with native headless scsynth."""
import math
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import runpy

ROOT = Path(__file__).resolve().parents[1]
helpers = runpy.run_path(str(ROOT / "tools/test-core-synth-audio.py"))
bundle, message, read_wav = (
    helpers["bundle"], helpers["message"], helpers["read_float_wav"])
PLUGIN = Path(shutil.which("scsynth") or "/missing").resolve().parent.parent


def render_mode(folder, name, mode, freeze=False):
    definitions = ROOT / "spikes/vst3-callback/Portable/SynthDefs"
    source = (folder / "GranularTestTone.scsyndef").read_bytes()
    effect = (definitions / "DexterGranularFX.scsyndef").read_bytes()
    controls = [
        0, 0.0,       # out
        1, 16.0,      # input bus
        2, 1.0,       # wet mix, so the test cannot pass on dry signal alone
        3, float(mode),
        4, 16.0,      # grains / second
        5, 0.12,      # grain duration
        6, 1.25,      # pitch ratio
        7, 0.45,      # buffer position
        8, 0.0,       # begin recording before freeze
    ]
    score = bundle(0, message("/d_recv", source),
                   message("/d_recv", effect))
    score += bundle(.01, message(
        "/s_new", "GranularTestTone", 100, 0, 0))
    score += bundle(.02, message(
        "/s_new", "DexterGranularFX", 101, 1, 0, *controls))
    if freeze:
        # Fill the selected buffer region before holding it.
        score += bundle(1.8, message("/n_set", 101, "fx_freeze", 1.0))
    score += bundle(3.0, message("/n_free", 100))
    score += bundle(3.01, message("/n_free", 101))
    score += bundle(3.1, message("/c_set", 0, 0.0))
    score_path = folder / (name + ".osc")
    wav_path = folder / (name + ".wav")
    score_path.write_bytes(score)
    rendered = subprocess.run(
        [str(PLUGIN / "bin/scsynth"), "-N", str(score_path), "_",
         str(wav_path), "48000", "WAV", "float", "-o", "2", "-i", "0",
         "-z", "64", "-U", str(PLUGIN / "lib/SuperCollider/plugins")],
        capture_output=True, text=True, timeout=45)
    log = rendered.stdout + rendered.stderr
    assert rendered.returncode == 0 and "FAILURE" not in log, (
        name, rendered.returncode, log)
    samples, channels, rate = read_wav(wav_path)
    assert channels == 2 and rate == 48000
    assert all(math.isfinite(value) for value in samples)
    peak = max(abs(value) for value in samples)
    assert 1e-5 < peak < 1.5, (name, "silent or runaway output", peak)
    channel_delta = max(
        abs(samples[index] - samples[index + 1])
        for index in range(0, len(samples) - 1, 2))
    assert channel_delta > 1e-7, (name, "no stereo output", channel_delta)
    return samples, peak


def main():
    scsynth = shutil.which("scsynth")
    sclang = shutil.which("sclang")
    if not scsynth or not sclang:
        raise SystemExit("sclang and scsynth are required for this audio test")
    with tempfile.TemporaryDirectory(prefix="dexter-granular-nrt-") as directory:
        folder = Path(directory)
        source_script = folder / "build-tone.scd"
        source_script.write_text(
            'SynthDef(\\GranularTestTone, { |out=16| '
            'var mod = SinOsc.kr(1.7).range(0.15, 0.65); '
            'Out.ar(out, (SinOsc.ar(330) * mod)!2) '
            '}).writeDefFile("' + folder.as_posix() + '"); 0.exit;\n',
            encoding="utf-8")
        built = subprocess.run(
            [sclang, str(source_script)], capture_output=True, text=True,
            timeout=45,
            env=dict(os.environ, QT_QPA_PLATFORM="minimal",
                     QTWEBENGINE_DISABLE_SANDBOX="1"))
        build_log = built.stdout + built.stderr
        assert built.returncode == 0 and "ERROR:" not in build_log, build_log

        results = {}
        for label, mode, freeze in (
                ("cloud", 0, False), ("slicer", 1, False),
                ("freeze", 2, True), ("live", 3, False)):
            results[label] = render_mode(folder, label, mode, freeze)
            print(f"PASS {label}: native stereo render, peak="
                  f"{results[label][1]:.6f}")
        for left, right in (("cloud", "slicer"), ("slicer", "freeze"),
                            ("freeze", "live")):
            difference = max(abs(a - b) for a, b in
                             zip(results[left][0], results[right][0]))
            assert difference > 1e-5, (left, right, "modes sound identical")
        print("PASS all four granular modes render distinct, finite stereo audio")


if __name__ == "__main__":
    main()
