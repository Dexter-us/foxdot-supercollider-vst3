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
u16(6)  # DexterTone plus five embedded FoxDot patches
string("DexterTone")
constants = (0.0, 0.1, 0.0, 0.5, 1.0, 0.2, 0.005)
u16(len(constants))
for value in constants:
    f32(value)
parameters = (0.0, 440.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 20000.0)
u16(len(parameters))
for value in parameters:
    f32(value)
parameter_names = (("out", 0), ("freq", 1), ("sine", 2),
                   ("triangle", 3), ("saw", 4), ("pulse", 5),
                   ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8))
u16(len(parameter_names))
for name, index in parameter_names:
    string(name)
    u16(index)

u16(20)  # Controls, oscillators, selectors, Mouse UI, and reverb send
ugen("Control", 1, [], [1, 1, 1, 1, 1, 1, 1, 1, 1])
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
# Mouse UI UGens write normalized cursor positions to auxiliary audio buses.
ugen("MouseX", 1, [(-1, 0), (-1, 4), (-1, 2), (-1, 5)], [1])
ugen("MouseY", 1, [(-1, 0), (-1, 4), (-1, 2), (-1, 5)], [1])
ugen("Out", 2, [(0, 0), (12, 0), (13, 0), (14, 0)], [])
# A separately enveloped, low-pass-filtered send feeds the persistent player reverb graph.
ugen("LPF", 2, [(12, 0), (0, 8)], [2])
ugen("Lag", 2, [(0, 7), (-1, 6)], [2])
ugen("BinaryOpUGen", 2, [(16, 0), (17, 0)], [2], 2)
ugen("Out", 2, [(0, 6), (18, 0), (18, 0)], [])
u16(0)  # no variants

parameter_names = (("out", 0), ("freq", 1), ("sine", 2),
                   ("triangle", 3), ("saw", 4), ("pulse", 5),
                   ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8))
parameters = (0.0, 440.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 20000.0)
constants = (0.0, 0.005, 0.1, 0.5, 1.0, 0.2, 0.005, 1.005, 0.0025, 0.004,
             0.25, 0.8, 0.85, 2.0)

def begin_patch(name, patch_parameter_names=parameter_names):
    string(name)
    u16(len(constants))
    for value in constants:
        f32(value)
    u16(len(parameters))
    for value in parameters:
        f32(value)
    u16(len(patch_parameter_names))
    for parameter_name, index in patch_parameter_names:
        string(parameter_name)
        u16(index)

def finish_patch():
    u16(0)  # no variants

def mouse_and_reverb(audio_ugen):
    # These auxiliary buses preserve the plugin's MouseX/MouseY controls and
    # the per-player reverb send used by DexterTone.
    ugen("MouseX", 1, [(-1, 0), (-1, 4), (-1, 2), (-1, 5)], [1])
    ugen("MouseY", 1, [(-1, 0), (-1, 4), (-1, 2), (-1, 5)], [1])
    mouse_x = audio_ugen + 1
    mouse_y = audio_ugen + 2
    out = audio_ugen + 3
    ugen("Out", 2, [(0, 0), (audio_ugen, 0), (mouse_x, 0), (mouse_y, 0)], [])
    lowpass = out + 1
    ugen("LPF", 2, [(audio_ugen, 0), (0, 8)], [2])
    lag = out + 2
    ugen("Lag", 2, [(0, 7), (-1, 6)], [2])
    send = out + 3
    ugen("BinaryOpUGen", 2, [(lowpass, 0), (lag, 0)], [2], 2)
    ugen("Out", 2, [(0, 6), (send, 0), (send, 0)], [])

begin_patch("DexterBass", (("out", 0), ("freq", 1), ("rate", 2),
                           ("unused3", 3), ("unused4", 4), ("unused5", 5),
                           ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8)))
u16(17)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), (-1, 10)], [2], 2)  # FoxDot bass frequency / 4
ugen("BinaryOpUGen", 2, [(0, 2), (-1, 2)], [2], 2)   # FoxDot rate / 10
ugen("LFTri", 2, [(1, 0), (-1, 0)], [2])
ugen("VarSaw", 2, [(1, 0), (-1, 0), (2, 0)], [2])
ugen("SinOscFB", 2, [(1, 0), (-1, 0)], [2])
ugen("BinaryOpUGen", 2, [(3, 0), (4, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(5, 0), (-1, 3)], [2], 2)   # SinOscFB * 0.5
ugen("BinaryOpUGen", 2, [(6, 0), (7, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(8, 0), (-1, 11)], [2], 2)  # FoxDot bass amp * 0.8
mouse_and_reverb(9)
finish_patch()

begin_patch("DexterVarsaw", (("out", 0), ("freq", 1), ("rate", 2),
                             ("unused3", 3), ("unused4", 4), ("unused5", 5),
                             ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8)))
u16(13)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), (-1, 7)], [2], 2)  # slight detune
ugen("VarSaw", 2, [(0, 1), (-1, 0), (0, 2)], [2])
ugen("VarSaw", 2, [(1, 0), (-1, 0), (0, 2)], [2])
ugen("BinaryOpUGen", 2, [(2, 0), (3, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(4, 0), (-1, 10)], [2], 2)  # two detuned voices at amp / 4
mouse_and_reverb(5)
finish_patch()

begin_patch("DexterDirt", (("out", 0), ("freq", 1), ("rate", 2),
                           ("unused3", 3), ("unused4", 4), ("unused5", 5),
                           ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8)))
u16(18)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), (-1, 10)], [2], 2)  # FoxDot dirt frequency / 4
ugen("BinaryOpUGen", 2, [(1, 0), (-1, 4)], [2], 0)   # freq + 1
ugen("BinaryOpUGen", 2, [(1, 0), (-1, 4)], [2], 1)   # freq - 1
ugen("LFSaw", 2, [(1, 0)], [2])
ugen("VarSaw", 2, [(2, 0), (-1, 0), (-1, 12)], [2])  # fixed width 0.85
ugen("SinOscFB", 2, [(3, 0), (-1, 0)], [2])
ugen("BinaryOpUGen", 2, [(4, 0), (5, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(6, 0), (-1, 3)], [2], 2)   # SinOscFB * 0.5
ugen("BinaryOpUGen", 2, [(7, 0), (8, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(9, 0), (-1, 3)], [2], 2)   # FoxDot dirt amp / 2
mouse_and_reverb(10)
finish_patch()

begin_patch("DexterJBass", (("out", 0), ("freq", 1), ("rate", 2),
                            ("unused3", 3), ("unused4", 4), ("unused5", 5),
                            ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8)))
u16(11)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), (-1, 10)], [2], 2)  # FoxDot jbass frequency / 4
ugen("LFTri", 2, [(1, 0), (-1, 0)], [2])
ugen("BinaryOpUGen", 2, [(2, 0), (-1, 11)], [2], 2)  # FoxDot jbass amp * 0.8
mouse_and_reverb(3)
finish_patch()

begin_patch("DexterNoise", (("out", 0), ("freq", 1), ("rate", 2),
                            ("unused3", 3), ("unused4", 4), ("unused5", 5),
                            ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8)))
u16(10)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), (-1, 13)], [2], 2)  # FoxDot noise frequency * 2
ugen("LFNoise0", 2, [(1, 0), (-1, 0)], [2])
mouse_and_reverb(2)
finish_patch()

out_path.parent.mkdir(parents=True, exist_ok=True)
if out_path.suffix.lower() == ".h":
    rows = []
    for offset in range(0, len(data), 12):
        values = ", ".join(f"0x{value:02x}" for value in data[offset:offset + 12])
        rows.append("    " + values + ",")
    out_path.write_text("static constexpr unsigned char kToneSynthDef[] = {\n" +
                        "\n".join(rows) + "\n};\n", encoding="utf-8")
else:
    out_path.write_bytes(data)
print(f"wrote {out_path} ({len(data)} bytes)")
