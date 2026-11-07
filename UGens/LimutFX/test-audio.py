#!/usr/bin/env python3
"""Render the actual LimutFX module through scsynth NRT and inspect audio."""
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


def render(module_dir, synth_name, synth_source, effect_name, effect_controls,
           duration, synthdef_directory=None):
    scsynth = Path(shutil.which("scsynth")).resolve()
    stock = scsynth.parent.parent / "lib/SuperCollider/plugins"
    plugin_path = os.pathsep.join((str(module_dir.resolve()), str(stock)))
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        source_path = temp / "input.scd"
        source_path.write_text(
            synth_source.replace("{directory}", str(temp)) + "\n0.exit;\n",
            encoding="utf-8")
        compiled = subprocess.run(
            ["sclang", str(source_path)], capture_output=True, text=True,
            timeout=40, env=dict(os.environ, QT_QPA_PLATFORM="minimal",
                                 QTWEBENGINE_DISABLE_SANDBOX="1"))
        assert compiled.returncode == 0 and "ERROR:" not in compiled.stdout, (
            compiled.stdout + compiled.stderr)
        input_def = (temp / (synth_name + ".scsyndef")).read_bytes()
        synthdef_directory = synthdef_directory or HERE / "Portable/SynthDefs"
        effect_def = (synthdef_directory / (effect_name + ".scsyndef")).read_bytes()
        score = bundle(0, message("/d_recv", input_def),
                      message("/d_recv", effect_def))
        score += bundle(.05, message("/s_new", effect_name, 100, 1, 0,
                                     *effect_controls))
        score += bundle(.10, message("/s_new", synth_name, 101, 0, 0))
        score += bundle(duration, message("/c_set", 0, 0.0))
        score_path, wav_path = temp / "score.osc", temp / (effect_name + ".wav")
        score_path.write_bytes(score)
        process = subprocess.run(
            [str(scsynth), "-N", str(score_path), "_", str(wav_path),
             "48000", "WAV", "float", "-o", "4", "-i", "0", "-z", "64",
             "-m", "262144", "-U", plugin_path],
            capture_output=True, text=True, timeout=60)
        log = process.stdout + process.stderr
        if os.environ.get("LIMUTFX_DEBUG"):
            print(log)
        assert process.returncode == 0 and "FAILURE" not in log, log
        assert "exception" not in log.lower() and "not installed" not in log, log
        samples, channels, rate = read_wav(wav_path)
        assert channels == 4 and all(math.isfinite(value) for value in samples)
        return samples, rate


def rms(samples, rate, channel, start, end):
    stride = 4
    first = int(start * rate) * stride + channel
    last = int(end * rate) * stride + channel
    values = samples[first:last:stride]
    assert values
    return math.sqrt(sum(value * value for value in values) / len(values))


def crossing_frequency(samples, rate, channel, start, end):
    first = int(start * rate) * 4 + channel
    last = int(end * rate) * 4 + channel
    values = samples[first:last:4]
    crossings = sum(
        previous <= 0.0 < current
        for previous, current in zip(values, values[1:]))
    return crossings / max(1e-9, end - start)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("module_directory", type=Path)
    args = parser.parse_args()

    ping_source = r"""
SynthDef(\LimutPingPongInput, { |out=16|
    var env = EnvGen.ar(Env.perc(0.001,0.04), doneAction:2);
    var signal = [Impulse.ar(2) * env * 0.5, DC.ar(0)];
    Out.ar(out, signal);
    Out.ar(2, signal);
}).writeDefFile("{directory}");
"""
    ping, rate = render(
        args.module_directory, "LimutPingPongInput", ping_source,
        "LimutPingPong",
        [0, 0.0, 1, 16.0, 2, 1.0, 3, 0.25, 4, 0.58], 1.2)
    first_left = rms(ping, rate, 0, .28, .48)
    first_right = rms(ping, rate, 1, .28, .48)
    second_left = rms(ping, rate, 0, .53, .73)
    dry_left = rms(ping, rate, 2, .10, .20)
    assert first_right > 1e-5 and second_left > 1e-5, (
        first_left, first_right, second_left)
    assert first_right > first_left * 1.5, (
        "expected the first ping-pong repeat on the opposite side",
        first_left, first_right)
    assert dry_left > 1e-5
    print(f"PASS LimutPingPong stereo cross-delay; {rate} Hz")

    duck_source = r"""
SynthDef(\LimutDuckInput, { |out=16|
    var env = EnvGen.ar(Env([1,1],[1.2]), doneAction:2);
    var signal = (SinOsc.ar(220) * env * 0.3) ! 2;
    Out.ar(out, signal);
    Out.ar(2, signal);
}).writeDefFile("{directory}");
"""
    duck, rate = render(
        args.module_directory, "LimutDuckInput", duck_source,
        "LimutDuck",
        [0, 0.0, 1, 16.0, 2, 1.0, 3, 0.5, 4, 0.72,
         5, 0.08, 6, 0.18], 1.2)
    early = rms(duck, rate, 0, .12, .20)
    recovered = rms(duck, rate, 0, .30, .42)
    duck_dry = rms(duck, rate, 2, .12, .20)
    assert 1e-4 < early < duck_dry * 0.65, (early, duck_dry)
    assert recovered > early * 1.3, (early, recovered)
    assert duck_dry > 1e-4
    print(f"PASS LimutDuck audible pump and recovery; {rate} Hz")

    disperser_source = r"""
SynthDef(\LimutDisperserInput, { |out=16|
    var env = EnvGen.ar(Env([1,1],[1.2]), doneAction:2);
    var signal = (SinOsc.ar(440) * env * 0.3) ! 2;
    Out.ar(out, signal);
    Out.ar(2, signal);
}).writeDefFile("{directory}");
"""
    disperser, rate = render(
        args.module_directory, "LimutDisperserInput", disperser_source,
        "LimutDisperser",
        [0, 0.0, 1, 16.0, 2, 1.0, 3, 1250.0, 4, 4.0, 5, 18.0], 1.2)
    disperser_wet = rms(disperser, rate, 0, .15, .9)
    disperser_dry = rms(disperser, rate, 2, .15, .9)
    assert disperser_wet > 1e-3 and disperser_dry > 1e-3
    assert abs(disperser_wet - disperser_dry) < disperser_dry * 0.35, (
        disperser_wet, disperser_dry)
    assert max(abs(value) for value in disperser) < 1.0
    print(f"PASS LimutDisperser stable all-pass output; {rate} Hz")

    fuzz_source = r"""
SynthDef(\LimutFuzzInput, { |out=16|
    var env = EnvGen.ar(Env([1,1],[1.2]), doneAction:2);
    var signal = (SinOsc.ar(220) * env * 0.3) ! 2;
    Out.ar(out, signal);
    Out.ar(2, signal);
}).writeDefFile("{directory}");
"""
    fuzz, rate = render(
        args.module_directory, "LimutFuzzInput", fuzz_source, "LimutFuzz",
        [0, 0.0, 1, 16.0, 2, 1.0, 3, 30.0, 4, 5000.0], 1.2)
    fuzz_wet = rms(fuzz, rate, 0, .15, .9)
    fuzz_dry = rms(fuzz, rate, 2, .15, .9)
    assert fuzz_wet > 1e-3 and fuzz_dry > 1e-3
    assert abs(fuzz_wet - fuzz_dry) > fuzz_dry * 0.25, (
        fuzz_wet, fuzz_dry)
    assert max(abs(value) for value in fuzz) < 1.0
    print(f"PASS LimutFuzz audible saturation; {rate} Hz")

    shifter_source = r"""
SynthDef(\LimutShifterInput, { |out=16|
    var signal = (SinOsc.ar(220) * 0.25) ! 2;
    Out.ar(out, signal);
    Out.ar(2, signal);
}).writeDefFile("{directory}");
"""
    shifter, rate = render(
        args.module_directory, "LimutShifterInput", shifter_source,
        "LimutShifter",
        [0, 0.0, 1, 16.0, 2, 1.0, 3, 2.0, 4, 0.12], 1.2)
    shifted_hz = crossing_frequency(shifter, rate, 0, .35, .85)
    shifter_dry = rms(shifter, rate, 2, .35, .85)
    assert 380.0 < shifted_hz < 500.0, shifted_hz
    assert shifter_dry > 0.1
    assert max(abs(value) for value in shifter) < 1.0
    print(f"PASS LimutShifter octave-up pitch; measured {shifted_hz:.1f} Hz")

    shimmer_source = r"""
SynthDef(\LimutShimmerInput, { |out=16|
    var trigger = Impulse.kr(2);
    var env = EnvGen.kr(Env.perc(0.002, 0.08), trigger);
    var signal = (SinOsc.ar(220) * env * 0.4) ! 2;
    Out.ar(out, signal);
    Out.ar(2, signal);
}).writeDefFile("{directory}");
"""
    shimmer, rate = render(
        args.module_directory, "LimutShimmerInput", shimmer_source,
        "LimutShimmer",
        [0, 0.0, 1, 16.0, 2, 0.65, 3, 2.0, 4, 0.12, 5, 0.8, 6, 0.35],
        1.8)
    shimmer_body = rms(shimmer, rate, 0, .12, .55)
    shimmer_tail = rms(shimmer, rate, 0, .65, 1.3)
    assert shimmer_body > 1e-4, shimmer_body
    assert shimmer_tail > 1e-5, shimmer_tail
    assert max(abs(value) for value in shimmer) < 1.0
    print(f"PASS LimutShimmer audible pitch/reverb tail; {rate} Hz")

    gated_source = r"""
SynthDef(\LimutGatedVerbInput, { |out=16|
    var trigger = Impulse.kr(0.5);
    var env = EnvGen.kr(Env.perc(0.002, 0.08), trigger);
    var signal = (SinOsc.ar(330) * env * 0.4) ! 2;
    Out.ar(out, signal);
    Out.ar(2, signal);
}).writeDefFile("{directory}");
"""
    gated, rate = render(
        args.module_directory, "LimutGatedVerbInput", gated_source,
        "LimutGatedVerb",
        [0, 0.0, 1, 16.0, 2, 1.0, 3, 0.75, 4, 0.5, 5, 0.02,
         6, 0.12, 7, 0.01, 8, 0.12], 1.5)
    gated_open = rms(gated, rate, 0, .12, .42)
    gated_closed = rms(gated, rate, 0, .9, 1.3)
    assert gated_open > 1e-4, gated_open
    assert gated_closed < gated_open * 0.2, (gated_open, gated_closed)
    assert max(abs(value) for value in gated) < 1.0
    print(f"PASS LimutGatedVerb gated tail; {rate} Hz")

    compressor_source = r"""
SynthDef(\LimutCompressorInput, { |out=16|
    var signal = (SinOsc.ar(220) * 0.8) ! 2;
    Out.ar(out, signal);
    Out.ar(2, signal);
}).writeDefFile("{directory}");
"""
    compressed, rate = render(
        args.module_directory, "LimutCompressorInput", compressor_source,
        "LimutCompressor",
        [0, 0.0, 1, 16.0, 2, 1.0, 3, -18.0, 4, 12.0,
         5, 0.005, 6, 0.12, 7, 4.0], 1.2)
    compressor_wet = rms(compressed, rate, 0, .15, .9)
    compressor_dry = rms(compressed, rate, 2, .15, .9)
    assert compressor_dry > 0.1
    assert 1e-3 < compressor_wet < compressor_dry * 0.8, (
        compressor_wet, compressor_dry)
    assert max(abs(value) for value in compressed) < 1.0
    print(f"PASS LimutCompressor audible linked gain reduction; {rate} Hz")

    vocoder_source = r"""
SynthDef(\LimutVocoderInput, { |out=16, modbus=18|
    var carrier = (Saw.ar(110) * 0.3) ! 2;
    var syllable = LFPulse.kr(4, 0, 0.42);
    var modulator = (Formant.ar(120, 500 + (syllable * 900), 80) *
        syllable * 0.35) ! 2;
    Out.ar(out, carrier);
    Out.ar(modbus, modulator);
    Out.ar(2, carrier);
}).writeDefFile("{directory}");
"""
    vocoded, rate = render(
        args.module_directory, "LimutVocoderInput", vocoder_source,
        "LimutVocoder",
        [0, 0.0, 1, 16.0, 2, 18.0, 3, 1.0, 4, 120.0,
         5, 9000.0, 6, 0.005, 7, 0.08, 8, 0.0, 9, 1.8],
        1.2, HERE / "Portable/Sidechain/SynthDefs")
    vocoder_wet = rms(vocoded, rate, 0, .15, .9)
    vocoder_dry = rms(vocoded, rate, 2, .15, .9)
    assert vocoder_dry > 1e-4 and vocoder_wet > 1e-5, (
        vocoder_wet, vocoder_dry)
    assert abs(vocoder_wet - vocoder_dry) > vocoder_dry * 0.05, (
        vocoder_wet, vocoder_dry)
    assert max(abs(value) for value in vocoded) < 1.0
    print(f"PASS LimutVocoder two-bus carrier/modulator output; {rate} Hz")


if __name__ == "__main__":
    main()
