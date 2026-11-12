#!/usr/bin/env python3
"""Validate the compiled stock-UGen player delay definitions."""

import argparse
import struct
from pathlib import Path


EXPECTED = {
    "DexterMultiTapDelayFX": {
        "out": 0, "inbus": 1, "fx_mix": 2, "fx_mode": 3,
        "fx_time": 4, "fx_taps": 5, "fx_feedback": 6,
        "fx_probability": 7, "fx_bpm": 8,
    },
    "DexterFeedbackDelayFX": {
        "out": 0, "inbus": 1, "fx_mix": 2, "fx_time": 3,
        "fx_feedback": 4, "fx_mode": 5, "fx_pitch": 6,
        "fx_interval": 7, "fx_probability": 8,
    },
}
REQUIRED_UGENS = {
    "DexterMultiTapDelayFX": {"In", "Out", "CombC", "Amplitude", "HPZ1", "TRand"},
    "DexterFeedbackDelayFX": {
        "In", "Out", "LocalIn", "LocalOut", "LPF", "PitchShift",
        "DelayC", "Impulse", "TRand",
    },
}


def parse_synthdef(data):
    offset = 0

    def read(fmt):
        nonlocal offset
        value = struct.unpack_from(fmt, data, offset)
        offset += struct.calcsize(fmt)
        return value[0] if len(value) == 1 else value

    def read_string():
        nonlocal offset
        size = read(">B")
        text = data[offset:offset + size].decode("utf-8")
        offset += size
        return text

    if data[:4] != b"SCgf":
        raise ValueError("invalid SynthDef magic")
    offset = 4
    version = read(">i")
    if version not in (1, 2):
        raise ValueError(f"unsupported SynthDef format version {version}")
    count_fmt = ">i" if version == 2 else ">H"
    index_fmt = ">i" if version == 2 else ">h"
    result = {}
    for _ in range(read(">H")):
        name = read_string()
        constants = read(count_fmt)
        offset += constants * 4
        offset += read(count_fmt) * 4
        controls = {}
        for _ in range(read(count_fmt)):
            control_name = read_string()
            control_index = read(count_fmt)
            controls[control_name] = control_index
        ugens = []
        for _ in range(read(count_fmt)):
            ugen = read_string()
            rate = read(">B")
            input_count = read(count_fmt)
            output_count = read(count_fmt)
            read(">h")
            for _ in range(input_count):
                read(index_fmt)
                read(index_fmt)
            for _ in range(output_count):
                read(">B")
            ugens.append((ugen, input_count, rate))
        if read(">H") != 0:
            raise ValueError(f"{name} contains unexpected variants")
        result[name] = (controls, ugens)
    if offset != len(data):
        raise ValueError("trailing data after SynthDef graphs")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    for name, expected_controls in EXPECTED.items():
        path = args.directory / (name + ".scsyndef")
        if not path.is_file():
            raise SystemExit(f"missing compiled SynthDef: {path}")
        graphs = parse_synthdef(path.read_bytes())
        if set(graphs) != {name}:
            raise SystemExit(f"{path.name} contains unexpected graphs: {sorted(graphs)}")
        controls, ugens = graphs[name]
        if controls != expected_controls:
            raise SystemExit(f"{name} control ABI mismatch: {controls}")
        names = {ugen for ugen, _, _ in ugens}
        if not REQUIRED_UGENS[name].issubset(names):
            raise SystemExit(f"{name} lacks required stock UGens: {REQUIRED_UGENS[name] - names}")
        outs = [input_count for ugen, input_count, _ in ugens if ugen == "Out"]
        if outs != [3]:
            raise SystemExit(f"{name} must write one stereo Out, got {outs}")
        print(f"PASS {name}: 9-control ABI, stereo output, required stock UGens")


if __name__ == "__main__":
    main()
