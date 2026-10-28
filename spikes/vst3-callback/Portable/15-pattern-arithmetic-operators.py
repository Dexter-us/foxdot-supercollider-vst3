# FoxDot-style finite Pattern arithmetic beyond transposition.
Clock.bpm = 104
Scale.default.set("dorian")

scaled = P[0, 1, 2, 3] * 2
divided = P[8, 9] / P[2, 3]
steps = P[0, 2, 4, 7] % 3
squared = P[0, 1, 2, 3] ** 2
rhythm = P[1, 2] / 2

p1 >> saw(scaled, dur=rhythm, oct=5, sus=0.32, amp=0.34, pan=-0.4)
p2 >> pulse(divided, dur=1, oct=4, sus=0.6, amp=0.22, pan=0.35)
p3 >> triangle(steps, dur=0.5, oct=5, sus=0.22, amp=0.18, pan=0.1)
p4 >> saw(squared, dur=0.5, oct=4, sus=0.3, amp=0.16, pan=0.6)
