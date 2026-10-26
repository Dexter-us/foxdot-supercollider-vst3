#!/usr/bin/env python3
"""Generate the embedded DexterTone SynthDef with SuperCollider oscillator modes."""
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
constants = (0.0, 0.1, 0.0, 0.5)
u16(len(constants))
for value in constants:
    f32(value)
parameters = (0.0, 440.0, 1.0, 0.0, 0.0, 0.0)
u16(len(parameters))
for value in parameters:
    f32(value)
parameter_names = (("out", 0), ("freq", 1), ("sine", 2),
                   ("triangle", 3), ("saw", 4), ("pulse", 5))
u16(len(parameter_names))
for name, index in parameter_names:
    string(name)
    u16(index)

u16(14)  # Control, four oscillators, four gates, three sums, output gain, Out
ugen("Control", 1, [], [1, 1, 1, 1, 1, 1])
ugen("SinOsc", 2, [(0, 1), (-1, 2)], [2])
ugen("LFTri", 2, [(0, 1), (-1, 2)], [2])
ugen("Saw", 2, [(0, 1)], [2])
ugen("Pulse", 2, [(0, 1), (-1, 3)], [2])
ugen("BinaryOpUGen", 2, [(1, 0), (0, 2)], [2], 2)
ugen("BinaryOpUGen", 2, [(2, 0), (0, 3)], [2], 2)
ugen("BinaryOpUGen", 2, [(3, 0), (0, 4)], [2], 2)
ugen("BinaryOpUGen", 2, [(4, 0), (0, 5)], [2], 2)
ugen("BinaryOpUGen", 2, [(5, 0), (6, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(9, 0), (7, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(10, 0), (8, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(11, 0), (-1, 1)], [2], 2)
ugen("Out", 2, [(0, 0), (12, 0)], [])
u16(0)  # no variants
out_path.parent.mkdir(parents=True, exist_ok=True)
out_path.write_bytes(data)
print(f"wrote {out_path} ({len(data)} bytes)")
