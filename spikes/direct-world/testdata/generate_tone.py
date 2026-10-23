#!/usr/bin/env python3
"""Generate a tiny SuperCollider v1 SynthDef used by the direct-world spike."""
import struct
import sys
from pathlib import Path

out_path = Path(sys.argv[1] if len(sys.argv) > 1 else "tone.scsyndef")
data = bytearray()


def u8(value):
    data.extend(struct.pack(">B", value))


def u16(value):
    data.extend(struct.pack(">H", value))


def i16(value):
    data.extend(struct.pack(">h", value))


def i32(value):
    data.extend(struct.pack(">i", value))


def f32(value):
    data.extend(struct.pack(">f", value))


def string(value):
    encoded = value.encode("utf-8")
    u8(len(encoded))
    data.extend(encoded)


def ugen(name, rate, inputs, output_rates, special_index=0):
    string(name)
    u8(rate)
    u16(len(inputs))
    u16(len(output_rates))
    i16(special_index)
    for source_index, output_index in inputs:
        i16(source_index)
        i16(output_index)
    for output_rate in output_rates:
        u8(output_rate)


data.extend(b"SCgf")
i32(1)  # SynthDef version
u16(1)  # one SynthDef
string("DexterTone")
u16(2)
f32(0.0)  # output bus
f32(0.1)  # amplitude
u16(2)
f32(0.0)
f32(440.0)  # frequency
u16(2)
string("out")
u16(0)
string("freq")
u16(1)
u16(4)  # UGens
ugen("Control", 1, [], [1, 1])
ugen("SinOsc", 2, [(0, 1), (-1, 0)], [2])
ugen("BinaryOpUGen", 2, [(1, 0), (-1, 1)], [2], 2)
ugen("Out", 2, [(0, 0), (2, 0)], [])
u16(0)  # no variants
out_path.parent.mkdir(parents=True, exist_ok=True)
out_path.write_bytes(data)
print(f"wrote {out_path} ({len(data)} bytes)")