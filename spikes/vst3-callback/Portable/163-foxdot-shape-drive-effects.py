# Example 163: Native per-player FoxDot-style shape and drive inserts.
# These controls are smoothly modulated and work on synths and WAV players.
Clock.bpm = 104
Scale.default.set("minor")

p1 >> saw(P[0, 3, 5, 7], dur=0.5, oct=4,
          shape=var([0.0, 0.45, 0.9], [2, 2, 2]), amp=0.24)

p2 >> bass(P[0, -2, 3, -4], dur=1, oct=5, sus=0.8,
           drive=linvar([0.1, 0.8], [4, 4]), amp=0.28, pan=-0.2)

p3 >> play("x---x-o-", dur=0.5, sample=0,
           shape=0.25, drive=var([0.0, 0.35], [4, 4]),
           amp=0.14, pan=0.25)
