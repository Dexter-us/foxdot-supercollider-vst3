#!/usr/bin/env python3
"""Generate embedded SuperCollider FreeVerb, FreeVerb2, and GVerb SynthDefs."""
import struct
import sys
from pathlib import Path

out_path = Path(sys.argv[1] if len(sys.argv) > 1 else "reverb.scsyndef")
data = bytearray()

def u8(value): data.extend(struct.pack(">B", value))
def u16(value): data.extend(struct.pack(">H", value))
def i16(value): data.extend(struct.pack(">h", value))
def i32(value): data.extend(struct.pack(">i", value))
def f32(value): data.extend(struct.pack(">f", value))
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

def synthdef(name, constants, parameters, parameter_names, ugens):
    string(name)
    u16(len(constants))
    for value in constants: f32(value)
    u16(len(parameters))
    for value in parameters: f32(value)
    u16(len(parameter_names))
    for control_name, index in parameter_names:
        string(control_name)
        u16(index)
    u16(len(ugens))
    for name, rate, inputs, outputs, special in ugens:
        ugen(name, rate, inputs, outputs, special)
    u16(0)

def control(name, count, parameters, names, ugens):
    rates = [1] * count
    all_ugens = [("Control", 1, [], rates, 0)] + ugens
    synthdef(name, parameters[0], parameters[1], names, all_ugens)

data.extend(b"SCgf")
i32(1)
u16(3)

# Controls are in/out bus, room, and damping. Mix is fixed fully wet;
# the VST3 applies FoxDot's mix control to the separate wet return.
freeverb_ugens = [
    ("In", 2, [(0, 0)], [2], 0),
    ("FreeVerb", 2, [(1, 0), (-1, 0), (0, 2), (0, 3)], [2], 0),
    ("Out", 2, [(0, 1), (2, 0), (2, 0)], [], 0),
]
synthdef("DexRevFree", (1.0,), (0.0, 0.0, 0.5, 0.5),
         (("inbus", 0), ("outbus", 1), ("room", 2), ("damp", 3)),
         [("Control", 1, [], [1, 1, 1, 1], 0)] + freeverb_ugens)

freeverb2_ugens = [
    ("In", 2, [(0, 0)], [2, 2], 0),
    ("FreeVerb2", 2, [(1, 0), (1, 1), (-1, 0), (0, 2), (0, 3)], [2, 2], 0),
    ("Out", 2, [(0, 1), (2, 0), (2, 1)], [], 0),
]
synthdef("DexRevFree2", (1.0,), (0.0, 0.0, 0.5, 0.5),
         (("inbus", 0), ("outbus", 1), ("room", 2), ("damp", 3)),
         [("Control", 1, [], [1, 1, 1, 1], 0)] + freeverb2_ugens)

gverb_ugens = [
    ("In", 2, [(0, 0)], [2], 0),
    ("GVerb", 2, [(1, 0), (0, 2), (0, 3), (0, 4), (-1, 0), (0, 5), (-1, 1), (-1, 2), (-1, 3), (-1, 4)], [2, 2], 0),
    ("Out", 2, [(0, 1), (2, 0), (2, 1)], [], 0),
]
synthdef("DexRevGverb", (0.5, 0.0, 0.0, 0.0, 100.0), (0.0, 0.0, 50.0, 3.0, 0.5, 15.0),
         (("inbus", 0), ("outbus", 1), ("roomsize", 2), ("revtime", 3),
          ("damping", 4), ("spread", 5)),
         [("Control", 1, [], [1] * 6, 0)] + gverb_ugens)

out_path.parent.mkdir(parents=True, exist_ok=True)
out_path.write_bytes(data)
print(f"wrote {out_path} ({len(data)} bytes)")
