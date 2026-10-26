# FoxDot-style per-note envelope controls, in seconds.
# The renderer keeps at least 5 ms of release to avoid hard note endings.
Clock.bpm = 96
Scale.default.set("minor")

p1 >> triangle(P[0, 2, 4, 7], dur=0.5, oct=5, sus=0.35, amp=0.3, atk=0.005, rel=0.2)
p2 >> saw(P[0, None, 3, None], dur=0.5, oct=3, sus=0.25, amp=0.18, pan=0.3, atk=0.04, rel=0.1)
