#!/usr/bin/env python3
"""Generate our own tiny CC0 diagnostic WAVs; no downloaded sample library."""
import math
from pathlib import Path
import random
import struct
import wave

root = Path(__file__).resolve().parents[1] / "spikes/vst3-callback/Portable/Samples"

def write(relative, duration, source, channels=1, sample_rate=44100):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = int(duration * sample_rate)
    samples = bytearray()
    for i in range(frames):
        t = i / sample_rate
        fade = min(1.0, i / 80, (frames - 1 - i) / 160)
        values = source(t)
        if channels == 1:
            values = (values,)
        for value in values:
            samples.extend(struct.pack("<h", round(max(-1, min(1, value * fade)) * 32767)))
    with wave.open(str(path), "wb") as f:
        f.setparams((channels, 2, sample_rate, frames, "NONE", "not compressed"))
        f.writeframes(samples)
    print(relative, len(samples), "PCM bytes")

write("x/lower/00-test-kick.wav", .45,
      lambda t: .75 * math.sin(2*math.pi*(48*t + 30*(1-math.exp(-25*t))/25)) * math.exp(-11*t))
write("x/lower/01-test-kick-tight.wav", .3,
      lambda t: .7 * math.sin(2*math.pi*(60*t + 55*(1-math.exp(-35*t))/35)) * math.exp(-17*t))
rng = random.Random(20261030)
write("o/lower/00-test-snare.wav", .25,
      lambda t: (.38*rng.uniform(-1,1) + .18*math.sin(2*math.pi*175*t)) * math.exp(-17*t))
last = [0.0]
def hat(t):
    noise = rng.uniform(-1, 1)
    high = noise - last[0]
    last[0] = noise
    return .18 * high * math.exp(-45*t)
write("_/hyphen/00-test-hat.wav", .13, hat)
write("a/lower/00-test-chirp.wav", .7,
      lambda t: .45*math.sin(2*math.pi*(220*t + 440*t*t)) * math.sin(math.pi*t/.7)**2)
write("a/lower/01-test-stereo-chirp.wav", .7,
      lambda t: (.3*math.sin(2*math.pi*(330*t + 330*t*t)),
                 .3*math.sin(2*math.pi*(440*t + 220*t*t))), channels=2, sample_rate=48000)
