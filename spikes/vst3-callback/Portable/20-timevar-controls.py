# FoxDot-style time-varying controls: var steps; linvar ramps continuously.
# amp stays between 0.15 and 0.45; pan and cutoff move gradually over 8 beats.
Clock.bpm = 96
Scale.default.set("minor")

p1 >> triangle(P[0, 2, 4, 7], dur=0.5, oct=5, sus=0.35, amp=var([0.15, 0.45], [4, 4]))
p2 >> saw(P[0, 3, 5, 8], dur=0.5, oct=3, sus=0.25, amp=0.18, pan=linvar([-0.8, 0.8], 8), cutoff=linvar([400, 5000], 8))
