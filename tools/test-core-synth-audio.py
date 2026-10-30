#!/usr/bin/env python3
"""Render all new graphs twice with stock scsynth (no audio device).

Optional developer test, not a Windows SC/UGen build. Checks actual signal, finite
samples and a second note per graph. Uses fresh nodes, matching the new bank's
host lifecycle and original FoxDot. Does not claim DAW validation.
"""
import argparse
import array
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
from importlib.util import module_from_spec, spec_from_file_location

spec = spec_from_file_location("bank", Path(__file__).with_name("build-foxdot-synth-bank.py"))
bank = module_from_spec(spec)
spec.loader.exec_module(bank)


def string(value):
    blob = value.encode() + b"\0"
    return blob + b"\0" * (-len(blob) % 4)


def message(address, *arguments):
    types, values = ",", bytearray()
    for argument in arguments:
        if isinstance(argument, str):
            types += "s"
            values.extend(string(argument))
        elif isinstance(argument, bytes):
            types += "b"
            values.extend(struct.pack(">i", len(argument)) + argument)
            values.extend(b"\0" * (-len(argument) % 4))
        elif isinstance(argument, int):
            types += "i"
            values.extend(struct.pack(">i", argument))
        else:
            types += "f"
            values.extend(struct.pack(">f", argument))
    return string(address) + string(types) + values


def bundle(time, *messages):
    seconds = int(time)
    data = b"#bundle\0" + struct.pack(">II", seconds, int((time - seconds) * 2**32))
    for item in messages:
        data += struct.pack(">i", len(item)) + item
    return struct.pack(">i", len(data)) + data


def read_float_wav(path):
    raw = path.read_bytes()
    assert raw[:4] == b"RIFF" and raw[8:12] == b"WAVE"
    pos, samples, channels, rate = 12, None, None, None
    while pos + 8 <= len(raw):
        tag, size = struct.unpack_from("<4sI", raw, pos)
        data = raw[pos + 8:pos + 8 + size]
        if tag == b"fmt ":
            fmt, channels, rate = struct.unpack_from("<HHI", data)
            assert fmt in (3, 65534), fmt
        if tag == b"data":
            samples = array.array("f")
            samples.frombytes(data)
            if sys.byteorder != "little":
                samples.byteswap()
        pos += 8 + size + size % 2
    assert samples and channels and rate
    return samples, channels, rate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--keep", type=Path)
    parser.add_argument("--trace", help="Trace one synth shortly after its second trigger")
    args = parser.parse_args()
    header = (bank.SOURCE.parent / "foxdot_synthdefs.h").read_text()
    blob = bytes(int(v, 16) for v in re.findall(r"0x([0-9a-f]{2})", header))
    with tempfile.TemporaryDirectory() as temporary:
        directory = args.keep or Path(temporary)
        directory.mkdir(parents=True, exist_ok=True)
        score = bundle(0, message("/d_recv", blob))
        for index, name in enumerate(bank.NAMES):
            start = index * 3.0 + 0.1
            params = ["out", 0, "wetbus", 8, "wetgain", 0.0, "freq", 261.6256,
                      "sus", 0.25, "amp", 0.5, "trigger", 1.0]
            if name in ("nylon", "swell"):
                params += ["rate", 1.0]
            if name == "snick":
                params += ["rate", 2.0]
            if name == "ambi":
                params += ["rate", 0.5]
            score += bundle(start, message("/s_new", "Dexter" + name.capitalize(), 100, 0, 0, *params))
            score += bundle(start + 0.003, message("/n_set", 100, "trigger", 0.0))
            next_params = list(params)
            next_params[next_params.index("freq") + 1] = 329.6276
            score += bundle(start + 1.3, message("/n_free", 100),
                            message("/s_new", "Dexter" + name.capitalize(), 100, 0, 0, *next_params))
            score += bundle(start + 1.303, message("/n_set", 100, "trigger", 0.0))
            if name == args.trace:
                score += bundle(start + 1.45, message("/n_trace", 100))
            score += bundle(start + 2.8, message("/n_free", 100))
        score += bundle(len(bank.NAMES) * 3.0, message("/c_set", 0, 0))
        score_path, wav_path = directory / "core.osc", directory / "core.wav"
        score_path.write_bytes(score)
        result = subprocess.run(["scsynth", "-N", str(score_path), "_", str(wav_path),
                                 "48000", "WAV", "float", "-o", "3", "-i", "0",
                                 "-z", "64", "-m", "65536"],
                                capture_output=True, text=True, timeout=90)
        if args.keep:
            (directory / "render.log").write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "FAILURE" not in result.stdout and "exception" not in result.stdout.lower(), result.stdout
        samples, channels, rate = read_float_wav(wav_path)
        # Channels 1/2 carry platform cursor data, not audio. A headless NRT
        # server has no GUI pointer provider; verify the synth signal only.
        assert all(math.isfinite(v) for v in samples[::channels]), "Non-finite audio"
        for index, name in enumerate(bank.NAMES):
            peaks = []
            for offset in (0.11, 1.41):
                start = int((index * 3 + offset) * rate) * channels
                end = start + int(0.6 * rate) * channels
                signal = samples[start:end:channels]
                peaks.append(max(abs(v) for v in signal))
            assert min(peaks) > 1e-7, (name, "silent note", peaks)
            assert max(peaks) < 100, (name, "runaway output", peaks)
            print(f"{name:9s} first={peaks[0]:.6f} repeated={peaks[1]:.6f}")
        print("PASS: 26 graphs, 52 audible/finite notes with source-style fresh-node lifecycle")


if __name__ == "__main__":
    main()
