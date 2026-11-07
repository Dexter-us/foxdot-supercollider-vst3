#!/usr/bin/env python3
"""Optional native SC NRT test: actual upstream modules, wet-only tails.

Uses installed scsynth/sclang as developer test tools, not runtime dependencies.
Pass the directory containing freshly compiled JPverb / Greyhole modules
(.so on Linux, .scx on Windows), matching the installed scsynth plugin API.
"""
import argparse
import math
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
helpers = runpy.run_path(str(ROOT / "tools/test-core-synth-audio.py"))
bundle, message, read_wav = (
    helpers["bundle"], helpers["message"], helpers["read_float_wav"])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("module_directory", type=Path)
    args = parser.parse_args()
    scsynth = Path(shutil.which("scsynth")).resolve()
    stock = scsynth.parent.parent / "lib/SuperCollider/plugins"
    plugin_path = os.pathsep.join((str(args.module_directory.resolve()), str(stock)))
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        script = temp / "input.scd"
        script.write_text(
            'SynthDef(\\SpaceReverbInput, { |out=16| '
            'var env=EnvGen.ar(Env.perc(0.001,0.04), doneAction:2); '
            'var signal=(SinOsc.ar(440)*env*0.2)!2; '
            'Out.ar(out, signal); Out.ar(2, signal) '
            '}).writeDefFile(' + repr(str(temp)).replace("'", '"') +
            '); 0.exit;\n')
        result = subprocess.run(["sclang", str(script)], capture_output=True,
            text=True, timeout=40, env=dict(os.environ, QT_QPA_PLATFORM="minimal",
                QTWEBENGINE_DISABLE_SANDBOX="1"))
        assert result.returncode == 0 and "ERROR:" not in result.stdout, result.stdout
        source = (temp / "SpaceReverbInput.scsyndef").read_bytes()
        for name in ("JPverb", "Greyhole"):
            effect = (HERE / "Portable/SynthDefs" / (name + ".scsyndef")).read_bytes()
            # Numeric parameter indexes reproduce the plugin's positional ABI.
            # Source is ahead of the insert in execution order; output is wet-only.
            controls = ([0, 0.0, 1, 16.0, 2, 1.0, 3, 3.0] if name == "JPverb"
                        else [0, 0.0, 1, 16.0, 2, 1.0, 3, .35, 7, .7])
            score = bundle(0, message("/d_recv", source), message("/d_recv", effect))
            score += bundle(.05, message("/s_new", name, 100, 1, 0, *controls))
            score += bundle(.1, message("/s_new", "SpaceReverbInput", 101, 0, 0))
            score += bundle(1.2, message("/n_set", 100, 2, .8))
            score += bundle(1.5, message("/s_new", "SpaceReverbInput", 102, 0, 0))
            score += bundle(3.1, message("/n_set", 100, 2, 0.0))
            score += bundle(3.15, message("/s_new", "SpaceReverbInput", 103, 0, 0))
            score += bundle(4.5, message("/c_set", 0, 0.0))
            score_file, wav_file = temp / "score.osc", temp / (name + ".wav")
            score_file.write_bytes(score)
            result = subprocess.run([str(scsynth), "-N", str(score_file), "_",
                str(wav_file), "48000", "WAV", "float", "-o", "4", "-i", "0",
                "-z", "64", "-m", "262144", "-U", plugin_path],
                capture_output=True, text=True, timeout=60)
            log = result.stdout + result.stderr
            assert result.returncode == 0 and "FAILURE" not in log, log
            assert "exception" not in log.lower() and "not installed" not in log, log
            samples, channels, rate = read_wav(wav_file)
            assert channels == 4 and all(math.isfinite(v) for v in samples)
            peak = max(abs(v) for v in samples)
            assert 1e-5 < peak < 2, (name, peak)
            for begin, end in ((.3, 1.1), (1.8, 3.0)):
                tail = samples[int(begin * rate) * 4:int(end * rate) * 4:4]
                rms = math.sqrt(sum(v*v for v in tail) / len(tail))
                assert rms > 1e-7, (name, "silent wet tail", rms)
            start, end = int(3.15 * rate) * 4, int(3.3 * rate) * 4
            dry_error = max(abs(samples[i] - samples[i + 2])
                            for i in range(start, end, 4))
            assert dry_error < 1e-6, (name, "dry bypass mismatch", dry_error)
            assert any(abs(samples[i] - samples[i + 1]) > 1e-7
                       for i in range(int(.3 * rate) * 4, int(1.1 * rate) * 4, 4))
            print(f"PASS {name}: stereo finite tails, repeated input, exact dry bypass, peak={peak:.6f}")

if __name__ == "__main__":
    main()
