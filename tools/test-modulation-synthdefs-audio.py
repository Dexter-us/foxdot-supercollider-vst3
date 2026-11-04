#!/usr/bin/env python3
"""Run the precompiled modulation SynthDefs through native scsynth NRT."""
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

def main():
    scsynth = shutil.which("scsynth")
    if not scsynth:
        raise SystemExit("scsynth is required for this optional native audio test")
    scsynth = Path(scsynth).resolve()
    stock = scsynth.parent.parent / "lib/SuperCollider/plugins"
    plugin_path = str(stock)
    defs = ROOT / "spikes/vst3-callback/Portable/SynthDefs"
    with tempfile.TemporaryDirectory(prefix="dexter-modulation-nrt-") as folder:
        folder = Path(folder)
        source_file = folder / "ModulationTestInput.scsyndef"
        source_script = folder / "build-input.scd"
        source_script.write_text(
            'SynthDef(\\ModulationTestInput, { |out=16| '
            'var env=EnvGen.ar(Env.perc(0.003,0.12), doneAction:2); '
            'var signal=(SinOsc.ar(440)*env*0.2)!2; '
            'Out.ar(out, signal); Out.ar(2, signal) '
            '}).writeDefFile("' + str(folder).replace("\\", "/") +
            '"); 0.exit;\n')
        result = subprocess.run(["sclang", str(source_script)],
            capture_output=True, text=True, timeout=40,
            env=dict(os.environ, QT_QPA_PLATFORM="minimal",
                     QTWEBENGINE_DISABLE_SANDBOX="1"))
        assert result.returncode == 0 and "ERROR:" not in result.stdout, (
            result.stdout + result.stderr)
        source = source_file.read_bytes()
        for name in ("Chorus", "Flanger", "Phaser", "Tremolo"):
            effect = (defs / (name + ".scsyndef")).read_bytes()
            # All inserts have the same positional layout:
            # out, inbus, mix, rate, depth, delay, spread, feedback, tone.
            base = (
                [0, 0.0, 1, 16.0, 2, 1.0, 3, 0.8, 4, 0.75, 5, 0.018,
                 6, 0.9, 7, 0.6, 8, 10000.0] if name == "Chorus" else
                [0, 0.0, 1, 16.0, 2, 1.0, 3, 0.5, 4, 0.8, 5, 0.0035,
                 6, 0.9, 7, 0.95, 8, 12000.0] if name == "Flanger" else
                [0, 0.0, 1, 16.0, 2, 1.0, 3, 0.22, 4, 0.7, 5, 650.0,
                 6, 0.8, 7, 0.35, 8, 11000.0] if name == "Phaser" else
                [0, 0.0, 1, 16.0, 2, 1.0, 3, 5.0, 4, 0.7, 5, 0.65,
                 6, 0.9, 7, 0.1, 8, 11000.0])
            dry_controls = base.copy()
            dry_controls[5] = 0.0  # control 2, normalized mix=0
            score = bundle(0, message("/d_recv", source),
                           message("/d_recv", effect))
            score += bundle(.01, message("/s_new", name, 100, 1, 0, *base))
            score += bundle(.02, message("/s_new", "ModulationTestInput", 101, 0, 0))
            score += bundle(1.0, message("/n_set", 100, *dry_controls))
            # A repeated burst exercises continuing, bounded delay feedback.
            score += bundle(1.05, message("/s_new", "ModulationTestInput", 102, 0, 0))
            score += bundle(2.5, message("/c_set", 0, 0.0))
            score_file, wav_file = folder / "score.osc", folder / (name + ".wav")
            score_file.write_bytes(score)
            rendered = subprocess.run(
                [str(scsynth), "-N", str(score_file), "_", str(wav_file),
                 "48000", "WAV", "float", "-o", "4", "-i", "0",
                 "-z", "64", "-U", plugin_path],
                capture_output=True, text=True, timeout=60)
            log = rendered.stdout + rendered.stderr
            assert rendered.returncode == 0 and "FAILURE" not in log, log
            samples, channels, rate = read_wav(wav_file)
            assert channels == 4 and all(math.isfinite(value) for value in samples)
            peak = max(abs(value) for value in samples)
            assert 1e-5 < peak < 2.0, (name, "unbounded/inaudible peak", peak)
            dry_error = max(abs(samples[i] - samples[i + 2])
                            for i in range(int(1.05 * rate) * 4,
                                           int(1.25 * rate) * 4, 4))
            assert dry_error < 1e-6, (name, "dry bypass mismatch", dry_error)
            left_right_difference = max(
                abs(samples[i] - samples[i + 1])
                for i in range(int(.1 * rate) * 4, int(.9 * rate) * 4, 4))
            assert left_right_difference > 1e-7, (
                name, "stereo modulation did not produce a changing image")
            print(f"PASS {name}: finite bounded render, stereo modulation, "
                  f"exact dry bypass, peak={peak:.6f}")

        # Exercise ordered bus routing with real stock SC UGens, not a mock DSP.
        # The native renderer runs these nodes in source -> insert1 -> insert2
        # order; the VST calculates its paused insert nodes in the same order.
        phaser = [2, 0.65, 3, 0.4, 4, 0.7, 5, 650.0,
                  6, 0.8, 7, 0.25, 8, 11000.0]
        tremolo = [2, 1.0, 3, 5.0, 4, 0.9, 5, 0.4,
                   6, 0.8, 7, 0.1, 8, 11000.0]

        def render_chain(label, inserts):
            score = bundle(0, message("/d_recv", source))
            for name in sorted({entry[0] for entry in inserts}):
                score += bundle(0, message(
                    "/d_recv", (defs / (name + ".scsyndef")).read_bytes()))
            for slot, (name, controls) in enumerate(inserts):
                input_bus = 16 + slot * 2
                output_bus = 0 if slot == len(inserts) - 1 else input_bus + 2
                score += bundle(.01, message(
                    "/s_new", name, 100 + slot, 1, 0,
                    0, float(output_bus), 1, float(input_bus), *controls))
            for index, beat in enumerate((.02, .32, .62, .92)):
                score += bundle(beat, message(
                    "/s_new", "ModulationTestInput", 200 + index, 0, 0,
                    0, 16.0 if inserts else 0.0))
            score += bundle(2.5, message("/c_set", 0, 0.0))
            score_file = folder / (label + ".osc")
            wav_file = folder / (label + ".wav")
            score_file.write_bytes(score)
            rendered = subprocess.run(
                [str(scsynth), "-N", str(score_file), "_", str(wav_file),
                 "48000", "WAV", "float", "-o", "4", "-i", "0",
                 "-z", "64", "-U", plugin_path],
                capture_output=True, text=True, timeout=60)
            log = rendered.stdout + rendered.stderr
            assert rendered.returncode == 0 and "FAILURE" not in log, log
            samples, channels, rate = read_wav(wav_file)
            assert channels == 4 and all(math.isfinite(v) for v in samples)
            assert 1e-5 < max(abs(v) for v in samples) < 2.0
            return samples

        bypass = tremolo.copy()
        bypass[1] = 0.0
        single = render_chain("single-phaser", [("Phaser", phaser)])
        bypassed = render_chain("phaser-bypassed-tremolo",
                                [("Phaser", phaser), ("Tremolo", bypass)])
        chained = render_chain("phaser-tremolo",
                               [("Phaser", phaser), ("Tremolo", tremolo)])
        reversed_chain = render_chain("tremolo-phaser",
                                      [("Tremolo", tremolo), ("Phaser", phaser)])
        assert len(single) == len(bypassed) == len(chained) == len(reversed_chain)
        error = max(abs(a - b) for a, b in zip(single, bypassed))
        assert error < 1e-6, ("second-insert bypass did not preserve first", error)
        changed = max(abs(a - b) for a, b in zip(single, chained))
        order_difference = max(abs(a - b) for a, b in zip(chained, reversed_chain))
        assert changed > 1e-4, ("second insert was inaudible", changed)
        assert order_difference > 1e-6, ("insert order had no effect", order_difference)
        four = render_chain("four-bypassed-inserts",
                            [("Tremolo", bypass)] * 4)
        dry_error = max(abs(four[i] - four[i + 2])
                        for i in range(0, len(four), 4))
        assert dry_error < 1e-6, ("four-insert cascade lost dry input", dry_error)
        print("PASS ordered two/four-insert native audio: second-stage bypass, "
              f"audible modulation, order difference={order_difference:.7f}, "
              f"four-stage dry error={dry_error:.9f}")

if __name__ == "__main__":
    main()
