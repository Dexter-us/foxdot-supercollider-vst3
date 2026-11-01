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
u16(28)  # DexterTone plus twenty-seven embedded FoxDot patches
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
             0.25, 0.8, 0.85, 2.0, 1.95, 32.0, 4.0, 3.0, 20.0, 15.0,
             1.5, 0.02, 0.04, 0.125, 12.0, 0.0625, 0.001, 0.002, 0.008,
             0.01, 0.03, 0.05, 0.07, 0.14, 0.7, 0.9, 1.2, 2.002, 2.49,
             2.571, 3.05, 6.242, 9.6, 11.0, 12.49, 13.0, 16.0, 24.0, 0.8,
             2.5, 8.0, 10.0, 0.015625, 0.000001, 0.3, 0.09, 1.002,
              440.0, 4000.0, 0.007, 0.501, 0.004, 0.02, 0.04, 0.5,
              4.0, 9.0, 50.0, 1.01, 1.0001, 0.16)

def begin_patch(name, patch_parameter_names=parameter_names,
                patch_parameters=parameters):
    string(name)
    u16(len(constants))
    for value in constants:
        f32(value)
    u16(len(patch_parameters))
    for value in patch_parameters:
        f32(value)
    u16(len(patch_parameter_names))
    for parameter_name, index in patch_parameter_names:
        string(parameter_name)
        u16(index)

def finish_patch():
    u16(0)  # no variants

def mouse_and_reverb(audio_ugen):
    # These auxiliary buses preserve the plugin's MouseX/MouseY controls and
    # the per-player reverb send used by DexterTone. FoxDot's SynthDef base
    # class applies Mix(osc) * 0.5 before its output.
    mixed = audio_ugen + 1
    ugen("BinaryOpUGen", 2, [(audio_ugen, 0), (-1, 3)], [2], 2)
    ugen("MouseX", 1, [(-1, 0), (-1, 4), (-1, 2), (-1, 5)], [1])
    ugen("MouseY", 1, [(-1, 0), (-1, 4), (-1, 2), (-1, 5)], [1])
    mouse_x = audio_ugen + 2
    mouse_y = audio_ugen + 3
    out = audio_ugen + 4
    ugen("Out", 2, [(0, 0), (mixed, 0), (mouse_x, 0), (mouse_y, 0)], [])
    lowpass = out + 1
    ugen("LPF", 2, [(mixed, 0), (0, 8)], [2])
    lag = out + 2
    ugen("Lag", 2, [(0, 7), (-1, 6)], [2])
    send = out + 3
    ugen("BinaryOpUGen", 2, [(lowpass, 0), (lag, 0)], [2], 2)
    ugen("Out", 2, [(0, 6), (send, 0), (send, 0)], [])

def const_input(value):
    return (-1, constants.index(float(value)))

def begin_trigger_patch(name):
    trigger_parameters = (("out", 0), ("freq", 1), ("rate", 2),
                          ("sus", 3), ("unused4", 4), ("unused5", 5),
                          ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8),
                          ("trigger", 9))
    begin_patch(name, trigger_parameters, parameters + (0.0,))

def klank_inputs(excitation, freqscale, freqoffset, decayscale, partials):
    inputs = [(excitation, 0), freqscale, freqoffset, decayscale]
    for ratio, level, ring_time in partials:
        inputs.extend((const_input(ratio), const_input(level), const_input(ring_time)))
    return inputs

begin_patch("DexterBass", (("out", 0), ("freq", 1), ("rate", 2),
                           ("unused3", 3), ("unused4", 4), ("unused5", 5),
                           ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8)))
u16(18)
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
u16(14)
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
u16(19)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), (-1, 10)], [2], 2)  # FoxDot dirt frequency / 4
ugen("BinaryOpUGen", 2, [(1, 0), (-1, 4)], [2], 0)   # freq + 1
ugen("BinaryOpUGen", 2, [(1, 0), (-1, 4)], [2], 1)   # freq - 1
ugen("LFSaw", 2, [(1, 0), (-1, 0)], [2])
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
u16(12)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), (-1, 10)], [2], 2)  # FoxDot jbass frequency / 4
ugen("LFTri", 2, [(1, 0), (-1, 0)], [2])
ugen("BinaryOpUGen", 2, [(2, 0), (-1, 11)], [2], 2)  # FoxDot jbass amp * 0.8
mouse_and_reverb(3)
finish_patch()

begin_patch("DexterNoise", (("out", 0), ("freq", 1), ("rate", 2),
                            ("unused3", 3), ("unused4", 4), ("unused5", 5),
                            ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8)))
u16(11)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), (-1, 13)], [2], 2)  # FoxDot noise frequency * 2
ugen("LFNoise0", 2, [(1, 0)], [2])
mouse_and_reverb(2)
finish_patch()

begin_patch("DexterDub")
u16(14)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), (-1, 13)], [2], 3)  # FoxDot dub frequency / 4
ugen("LFTri", 2, [(1, 0), (-1, 0)], [2])
ugen("SinOscFB", 2, [(1, 0), (-1, 0)], [2])
ugen("BinaryOpUGen", 2, [(2, 0), (3, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(4, 0), (-1, 13)], [2], 2)  # FoxDot dub amp * 2
mouse_and_reverb(5)
finish_patch()

begin_patch("DexterGrowl", (("out", 0), ("freq", 1), ("rate", 2),
                            ("sus", 3), ("unused4", 4), ("unused5", 5),
                            ("wetbus", 6), ("wetgain", 7), ("wetcutoff", 8)))
u16(19)
ugen("Control", 1, [], [1] * 9)
ugen("SinOsc", 1, [(-1, 3), (-1, 0)], [1])  # fixed 0.5 Hz modulator
ugen("BinaryOpUGen", 1, [(1, 0), (-1, 13)], [1], 2)  # * 2
ugen("BinaryOpUGen", 1, [(2, 0), (-1, 4)], [1], 0)   # + 1
ugen("BinaryOpUGen", 1, [(0, 1), (3, 0)], [1], 0)    # note freq + modulator
ugen("SinOsc", 2, [(4, 0), (-1, 0)], [2])
ugen("BinaryOpUGen", 1, [(0, 3), (-1, 20)], [1], 2)  # FoxDot sus * 1.5
ugen("BinaryOpUGen", 1, [(6, 0), (-1, 20)], [1], 3)  # / 1.5
ugen("BinaryOpUGen", 1, [(7, 0), (-1, 15)], [1], 2)   # * 32
ugen("Saw", 2, [(8, 0)], [2])
ugen("BinaryOpUGen", 2, [(5, 0), (9, 0)], [2], 2)
mouse_and_reverb(10)
finish_patch()

begin_patch("DexterSoprano")
u16(17)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 1, [(0, 2), (-1, 16)], [1], 0)  # FoxDot rate + 4
ugen("Vibrato", 1, [(0, 1), (1, 0), (-1, 21), (-1, 0), (-1, 0),
                    (-1, 22), (-1, 2), (-1, 0), (-1, 0)], [1])
ugen("BinaryOpUGen", 1, [(2, 0), (-1, 17)], [1], 2)  # vibrato frequency * 3
ugen("SinOsc", 2, [(3, 0), (-1, 0)], [2])
ugen("SinOscFB", 2, [(3, 0), (-1, 0)], [2])
ugen("BinaryOpUGen", 2, [(4, 0), (-1, 3)], [2], 2)   # amp / 2
ugen("BinaryOpUGen", 2, [(5, 0), (-1, 10)], [2], 2)  # amp / 4
ugen("BinaryOpUGen", 2, [(6, 0), (7, 0)], [2], 0)
mouse_and_reverb(8)
finish_patch()

begin_patch("DexterCrunch")
u16(14)
ugen("Control", 1, [], [1] * 9)
ugen("Crackle", 1, [(-1, 14)], [1])
ugen("BinaryOpUGen", 1, [(0, 1), (-1, 19)], [1], 2)  # freq * 15
ugen("BinaryOpUGen", 1, [(1, 0), (2, 0)], [1], 2)
ugen("LFNoise0", 2, [(3, 0)], [2])
ugen("BinaryOpUGen", 2, [(4, 0), (-1, 25)], [2], 2)  # osc amp * Env level amp/4
mouse_and_reverb(5)
finish_patch()

begin_patch("DexterLazer")
u16(27)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 1, [(0, 1), (-1, 7)], [1], 2)   # detuned frequency
ugen("BinaryOpUGen", 1, [(0, 2), (-1, 4)], [1], 1)   # rate - 1
ugen("BinaryOpUGen", 1, [(2, 0), (-1, 16)], [1], 3)  # width = (rate-1)/4
ugen("VarSaw", 2, [(0, 1), (-1, 0), (3, 0)], [2])
ugen("VarSaw", 2, [(1, 0), (-1, 0), (3, 0)], [2])
ugen("BinaryOpUGen", 1, [(0, 2), (-1, 13)], [1], 1)  # rate - 2
ugen("BinaryOpUGen", 1, [(6, 0), (-1, 2)], [1], 0)   # + 0.1
ugen("Pulse", 2, [(7, 0), (-1, 3)], [2])            # default width 0.5
ugen("BinaryOpUGen", 2, [(8, 0), (-1, 4)], [2], 0)  # Pulse add=1
ugen("BinaryOpUGen", 2, [(0, 1), (9, 0)], [2], 2)   # note freq * Pulse+1
ugen("BinaryOpUGen", 1, [(0, 2), (-1, 18)], [1], 2) # rate * 20
ugen("LFNoise0", 2, [(11, 0)], [2])
ugen("BinaryOpUGen", 2, [(12, 0), (-1, 3)], [2], 2) # LFNoise mul=0.5
ugen("BinaryOpUGen", 2, [(13, 0), (10, 0)], [2], 0) # LFNoise add=freq*Pulse+1
ugen("LFSaw", 2, [(14, 0), (-1, 0)], [2])
ugen("BinaryOpUGen", 2, [(4, 0), (5, 0)], [2], 0)   # Mix two detuned VarSaws
ugen("BinaryOpUGen", 2, [(15, 0), (-1, 13)], [2], 2) # mono LFSaw broadcasts twice
ugen("BinaryOpUGen", 2, [(16, 0), (17, 0)], [2], 0)
mouse_and_reverb(18)
finish_patch()

begin_patch("DexterRave")
u16(13)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 1, [(0, 2), (-1, 4)], [1], 1)  # Gendy amp distribution = rate - 1
ugen("BinaryOpUGen", 1, [(0, 1), (-1, 13)], [1], 2) # Gendy maxfreq = freq * 2
ugen("Gendy1", 2, [(1, 0), (-1, 4), (-1, 4), (-1, 4),
                   (0, 1), (2, 0), (-1, 3), (-1, 3),
                   (-1, 24), (-1, 24)], [2])
ugen("BinaryOpUGen", 2, [(3, 0), (-1, 3)], [2], 2)  # Gendy mul=amp/2
mouse_and_reverb(4)
finish_patch()

begin_trigger_patch("DexterBell")
bell_partials = list(zip(
    (0.501, 1.0, 0.7, 2.002, 3.0, 9.6, 2.49, 11.0, 2.571, 3.05,
     6.242, 12.49, 13.0, 16.0, 24.0),
    (0.002, 0.1, 0.001, 0.008, 0.02, 0.004, 0.02, 0.04, 0.02, 0.005,
     0.05, 0.05, 0.02, 0.03, 0.04),
    (1.2, 0.9, 0.25, 0.14, 0.07) * 3))
u16(13)
ugen("Control", 1, [], [1] * 10)
ugen("Trig1", 2, [(0, 9), const_input(0.001)], [2])
ugen("BinaryOpUGen", 2, [(0, 2), const_input(4.0)], [2], 2)
ugen("Klank", 2, klank_inputs(1, (0, 1), const_input(0.0), (2, 0),
                               bell_partials), [2])
ugen("BinaryOpUGen", 2, [(3, 0), const_input(4.0)], [2], 2)
mouse_and_reverb(4)
finish_patch()

begin_trigger_patch("DexterGong")
gong_partials = list(zip(
    (0.501, 1.0, 0.8, 2.002, 3.0, 9.6, 2.49, 11.0, 2.571, 3.05,
     6.242, 12.49, 13.0, 16.0, 24.0),
    (0.002, 0.02, 0.001, 0.008, 0.02, 0.004, 0.02, 0.04, 0.02, 0.005,
     0.05, 0.05, 0.02, 0.03, 0.04),
    (1.2, 1.2, 1.2, 0.9, 0.9, 0.9, 0.25, 0.25, 0.25, 0.14,
     0.14, 0.14, 0.07, 0.07, 0.07)))
u16(17)
ugen("Control", 1, [], [1] * 10)
ugen("Trig1", 2, [(0, 9), const_input(0.001)], [2])
ugen("SinOscFB", 2, [const_input(20.0), const_input(0.0)], [2])
ugen("BinaryOpUGen", 2, [(2, 0), (1, 0)], [2], 2)
ugen("BinaryOpUGen", 2, [(0, 1), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(4, 0), (0, 2)], [2], 2)
ugen("Klank", 2, klank_inputs(3, (4, 0), (5, 0), const_input(4.0),
                               gong_partials), [2])
ugen("HPF", 2, [(6, 0), const_input(440.0)], [2])
ugen("BinaryOpUGen", 2, [(7, 0), const_input(2.5)], [2], 2)
mouse_and_reverb(8)
finish_patch()

begin_trigger_patch("DexterMarimba")
marimba_partials = ((0.5, 0.5, 1.0), (1.0, 1.0, 1.0),
                    (4.0, 1.0, 1.0), (9.0, 1.0, 1.0))
u16(14)
ugen("Control", 1, [], [1] * 10)
ugen("Trig1", 2, [(0, 9), const_input(0.001)], [2])
ugen("PinkNoise", 2, [], [2])
ugen("BinaryOpUGen", 2, [(2, 0), (1, 0)], [2], 2)
ugen("Klank", 2, klank_inputs(3, (0, 1), const_input(0.0),
                               const_input(1.0), marimba_partials), [2])
ugen("BinaryOpUGen", 2, [(4, 0), const_input(0.007)], [2], 2)
mouse_and_reverb(5)
finish_patch()

begin_trigger_patch("DexterPluck")
u16(19)
ugen("Control", 1, [], [1] * 10)
ugen("Trig1", 2, [(0, 9), const_input(0.001)], [2])
ugen("BinaryOpUGen", 1, [(0, 3), const_input(4.0)], [1], 2)
ugen("Decay", 2, [(1, 0), (2, 0)], [2])
ugen("VarSaw", 2, [(0, 1), const_input(0.0), const_input(0.2)], [2])
ugen("BinaryOpUGen", 2, [(0, 1), const_input(1.002)], [2], 2)
ugen("SinOsc", 2, [(5, 0), (4, 0)], [2])
ugen("SinOsc", 2, [(0, 1), (4, 0)], [2])
ugen("BinaryOpUGen", 2, [(6, 0), (7, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(8, 0), const_input(0.09)], [2], 2)
ugen("BinaryOpUGen", 2, [(9, 0), (3, 0)], [2], 2)
mouse_and_reverb(10)
finish_patch()

begin_trigger_patch("DexterTwang")
u16(14)
ugen("Control", 1, [], [1] * 10)
ugen("Trig1", 2, [(0, 9), const_input(0.001)], [2])
ugen("BinaryOpUGen", 2, [(0, 1), const_input(8.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(0, 2), (2, 0)], [2], 4)
ugen("LPF", 2, [(1, 0), const_input(4000.0)], [2])
ugen("CombL", 2, [(4, 0), const_input(0.25), (3, 0), (0, 3)], [2])
mouse_and_reverb(5)
finish_patch()

begin_trigger_patch("DexterDonk")
u16(16)
ugen("Control", 1, [], [1] * 10)
ugen("Trig1", 2, [(0, 9), const_input(0.001)], [2])
ugen("BinaryOpUGen", 2, [(0, 1), const_input(2.0)], [2], 3)
ugen("BinaryOpUGen", 2, [(2, 0), const_input(2.0)], [2], 0)
ugen("Ringz", 2, [(1, 0), (2, 0), (0, 3)], [2])
ugen("Ringz", 2, [(1, 0), (3, 0), (0, 3)], [2])
ugen("BinaryOpUGen", 2, [(4, 0), (5, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(6, 0), const_input(0.8)], [2], 2)
mouse_and_reverb(7)
finish_patch()

begin_trigger_patch("DexterSquish")
u16(17)
ugen("Control", 1, [], [1] * 10)
ugen("Trig1", 2, [(0, 9), const_input(0.001)], [2])
ugen("BinaryOpUGen", 2, [(0, 2), const_input(4.0)], [2], 2)
ugen("Pulse", 2, [(2, 0), const_input(0.5)], [2])
ugen("BinaryOpUGen", 2, [(0, 1), const_input(0.25)], [2], 2)
ugen("Ringz", 2, [(3, 0), (4, 0), (0, 3)], [2])
ugen("Decay", 2, [(1, 0), (0, 3)], [2])
ugen("BinaryOpUGen", 2, [(5, 0), (6, 0)], [2], 2)
ugen("BinaryOpUGen", 2, [(7, 0), const_input(4.0)], [2], 2)
mouse_and_reverb(8)
finish_patch()

foxdot_patch_parameters = (
    ("out", 0), ("freq", 1), ("rate", 2), ("sus", 3),
    ("unused4", 4), ("unused5", 5), ("wetbus", 6),
    ("wetgain", 7), ("wetcutoff", 8))
foxdot_patch_defaults = (0.0, 440.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 20000.0)

begin_patch("DexterCharm", foxdot_patch_parameters, foxdot_patch_defaults)
u16(39)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), const_input(2.0)], [2], 0)
ugen("BinaryOpUGen", 2, [(1, 0), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(0, 1), const_input(8.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(2, 0), const_input(8.0)], [2], 2)
ugen("SinOsc", 2, [(0, 1), const_input(0.0)], [2])
ugen("SinOsc", 2, [(2, 0), const_input(0.0)], [2])
ugen("VarSaw", 2, [(3, 0), const_input(10.0), const_input(0.5)], [2])
ugen("VarSaw", 2, [(4, 0), const_input(10.0), const_input(0.5)], [2])
ugen("BinaryOpUGen", 2, [(8, 0), const_input(0.125)], [2], 2)
ugen("BinaryOpUGen", 2, [(7, 0), const_input(0.125)], [2], 2)
ugen("BinaryOpUGen", 2, [(5, 0), const_input(0.25)], [2], 2)
ugen("BinaryOpUGen", 2, [(6, 0), const_input(0.25)], [2], 2)
ugen("BinaryOpUGen", 2, [(11, 0), (10, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(12, 0), (9, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(0, 2), const_input(4.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(0, 3), const_input(0.125)], [2], 2)
ugen("Line", 2, [const_input(1.0), (15, 0), (16, 0),
                 const_input(0.0)], [2])
ugen("BinaryOpUGen", 2, [(0, 1), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(18, 0), const_input(10.0)], [2], 0)
ugen("SinOsc", 2, [(17, 0), const_input(0.0)], [2])
ugen("BinaryOpUGen", 2, [(20, 0), (18, 0)], [2], 2)
ugen("BinaryOpUGen", 2, [(21, 0), (19, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(2, 0), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(22, 0), const_input(10.0)], [2], 0)
ugen("SinOsc", 2, [(17, 0), const_input(0.0)], [2])
ugen("BinaryOpUGen", 2, [(25, 0), (23, 0)], [2], 2)
ugen("BinaryOpUGen", 2, [(26, 0), (24, 0)], [2], 0)
ugen("LPF", 2, [(13, 0), (22, 0)], [2])
ugen("LPF", 2, [(14, 0), (27, 0)], [2])
ugen("BinaryOpUGen", 2, [(27, 0), (28, 0)], [2], 0)
mouse_and_reverb(30)
finish_patch()

begin_patch("DexterSpark", foxdot_patch_parameters, foxdot_patch_defaults)
u16(18)
ugen("Control", 1, [], [1] * 9)
ugen("Saw", 2, [const_input(0.1)], [2])
ugen("BinaryOpUGen", 2, [(0, 1), const_input(1.002)], [2], 2)
ugen("LFSaw", 2, [(2, 0), (1, 0)], [2])
ugen("LFNoise2", 2, [const_input(50.0)], [2])
ugen("BinaryOpUGen", 2, [(4, 0), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(0, 1), (5, 0)], [2], 0)
ugen("LFSaw", 2, [(6, 0), (1, 0)], [2])
ugen("BinaryOpUGen", 2, [(3, 0), (7, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(8, 0), const_input(0.3)], [2], 2)
mouse_and_reverb(9)
finish_patch()

begin_patch("DexterBlip", foxdot_patch_parameters, foxdot_patch_defaults)
u16(32)
ugen("Control", 1, [], [1] * 9)
ugen("LFNoise2", 2, [const_input(50.0)], [2])
ugen("BinaryOpUGen", 2, [(1, 0), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(0, 1), (2, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(0, 1), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(3, 0), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(4, 0), const_input(1.002)], [2], 2)
ugen("BinaryOpUGen", 2, [(5, 0), const_input(1.002)], [2], 2)
ugen("LFCub", 2, [(6, 0), const_input(1.5)], [2])
ugen("LFCub", 2, [(7, 0), const_input(1.5)], [2])
ugen("Line", 2, [const_input(2.0), const_input(0.0), (0, 3),
                 const_input(0.0)], [2])
ugen("LFTri", 2, [(4, 0), (10, 0)], [2])
ugen("LFTri", 2, [(5, 0), (10, 0)], [2])
ugen("BinaryOpUGen", 2, [(11, 0), const_input(0.3)], [2], 2)
ugen("BinaryOpUGen", 2, [(12, 0), const_input(0.3)], [2], 2)
ugen("BinaryOpUGen", 2, [(8, 0), (13, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(9, 0), (14, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(4, 0), const_input(2.0)], [2], 3)
ugen("BinaryOpUGen", 2, [(5, 0), const_input(2.0)], [2], 3)
ugen("Blip", 2, [(17, 0), (0, 2)], [2])
ugen("Blip", 2, [(18, 0), (0, 2)], [2])
ugen("BinaryOpUGen", 2, [(15, 0), (19, 0)], [2], 2)
ugen("BinaryOpUGen", 2, [(16, 0), (20, 0)], [2], 2)
ugen("BinaryOpUGen", 2, [(21, 0), (22, 0)], [2], 0)
mouse_and_reverb(23)
finish_patch()

begin_patch("DexterScatter", foxdot_patch_parameters, foxdot_patch_defaults)
u16(18)
ugen("Control", 1, [], [1] * 9)
ugen("Saw", 2, [(0, 1)], [2])
ugen("BinaryOpUGen", 2, [(0, 1), const_input(2.0)], [2], 0)
ugen("VarSaw", 2, [(2, 0), const_input(0.0), const_input(0.5)], [2])
ugen("BinaryOpUGen", 2, [(0, 1), const_input(1.0)], [2], 0)
ugen("VarSaw", 2, [(4, 0), const_input(0.0), const_input(0.5)], [2])
ugen("BinaryOpUGen", 2, [(3, 0), (5, 0)], [2], 0)
ugen("LFNoise0", 2, [(0, 2)], [2])
ugen("BinaryOpUGen", 2, [(6, 0), (7, 0)], [2], 2)
ugen("BinaryOpUGen", 2, [(1, 0), (8, 0)], [2], 0)
mouse_and_reverb(9)
finish_patch()

begin_patch("DexterOrient", foxdot_patch_parameters, foxdot_patch_defaults)
u16(12)
ugen("Control", 1, [], [1] * 9)
ugen("LFPulse", 2, [(0, 1), const_input(0.5), const_input(0.25),
                    const_input(0.25), const_input(0.0)], [2])
ugen("LFPulse", 2, [(0, 1), const_input(1.0), const_input(0.1),
                    const_input(0.25), const_input(0.0)], [2])
ugen("BinaryOpUGen", 2, [(1, 0), (2, 0)], [2], 0)
mouse_and_reverb(3)
finish_patch()

begin_patch("DexterZap", foxdot_patch_parameters, foxdot_patch_defaults)
u16(22)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), const_input(1.01)], [2], 2)
ugen("LFNoise2", 2, [const_input(50.0)], [2])
ugen("BinaryOpUGen", 2, [(2, 0), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(1, 0), (3, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(0, 1), (3, 0)], [2], 0)
ugen("Saw", 2, [(4, 0)], [2])
ugen("Saw", 2, [(5, 0)], [2])
ugen("LFNoise2", 2, [const_input(50.0)], [2])
ugen("BinaryOpUGen", 2, [(8, 0), const_input(2.0)], [2], 2)
ugen("BinaryOpUGen", 2, [(0, 1), (9, 0)], [2], 0)
ugen("VarSaw", 2, [(10, 0), const_input(1.0), const_input(0.5)], [2])
ugen("BinaryOpUGen", 2, [(6, 0), (7, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(12, 0), (11, 0)], [2], 0)
mouse_and_reverb(13)
finish_patch()

begin_patch("DexterFuzz", foxdot_patch_parameters, foxdot_patch_defaults)
u16(12)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), const_input(0.5)], [2], 2)
ugen("LFSaw", 1, [(1, 0), const_input(0.0), (1, 0),
                  (0, 1)], [1])
ugen("LFSaw", 2, [(2, 0)], [2])
mouse_and_reverb(3)
finish_patch()

begin_patch("DexterBug", foxdot_patch_parameters, foxdot_patch_defaults)
u16(18)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), const_input(1.0001)], [2], 2)
ugen("Pulse", 2, [(0, 1), const_input(0.09)], [2])
ugen("Pulse", 2, [(1, 0), const_input(0.16)], [2])
ugen("Pulse", 2, [(0, 1), const_input(0.25)], [2])
ugen("BinaryOpUGen", 2, [(0, 2), const_input(4.0)], [2], 2)
ugen("SinOsc", 2, [(5, 0), const_input(0.0)], [2])
ugen("BinaryOpUGen", 2, [(2, 0), (3, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(7, 0), (4, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(8, 0), (6, 0)], [2], 2)
mouse_and_reverb(9)
finish_patch()

begin_patch("DexterRipple", foxdot_patch_parameters, foxdot_patch_defaults)
u16(17)
ugen("Control", 1, [], [1] * 9)
ugen("BinaryOpUGen", 2, [(0, 1), const_input(0.25)], [2], 3)
ugen("Pulse", 2, [(1, 0), const_input(0.2), const_input(0.25)], [2])
ugen("BinaryOpUGen", 2, [(0, 1), const_input(1.0)], [2], 0)
ugen("Pulse", 2, [(3, 0), const_input(0.5), const_input(0.5)], [2])
ugen("BinaryOpUGen", 2, [(2, 0), (4, 0)], [2], 0)
ugen("BinaryOpUGen", 2, [(0, 2), (0, 3)], [2], 4)
ugen("SinOsc", 2, [(6, 0), const_input(0.0), const_input(0.5),
                    const_input(1.0)], [2])
ugen("BinaryOpUGen", 2, [(5, 0), (7, 0)], [2], 2)
mouse_and_reverb(8)
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
