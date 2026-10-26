# FoxDot-style finite Pattern arithmetic: element-wise transposition and lacing.
Clock.bpm = 108
Scale.default.set("dorian")

pitches = P[0, 1, 2, 3, 4]
harmony = pitches + 2
bass = pitches - 1

p1 >> saw(harmony, dur=0.5, oct=5, sus=0.38, amp=0.6, pan=-0.2)
p2 >> pulse(bass, dur=1, oct=4, sus=0.72, amp=0.3, pan=0.25)
