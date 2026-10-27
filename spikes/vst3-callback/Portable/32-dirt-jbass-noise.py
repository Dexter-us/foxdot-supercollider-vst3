# 32: Three more self-contained FoxDot-style synth patches.
Clock.bpm = 84
Scale.default.set("minor")

p1 >> dirt(P[0, 2, 4, 6], dur=1, sus=0.82, oct=4,
          amp=0.12, pan=-0.2)
p2 >> jbass(P[0, None, 3, 5], dur=0.5, sus=0.44, oct=2,
            amp=0.28, pan=0.15)
# LFNoise0 is intentionally noisy; keep it tucked behind the pitched voices.
p3 >> noise(P[0, 2, 4, 6], dur=0.25, sus=0.2, oct=5,
            amp=0.012, pan=0.35)
