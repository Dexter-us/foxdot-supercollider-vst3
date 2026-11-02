# Example 134: PDelta accumulates a repeating interval pattern from its start.
Clock.bpm = 96
Scale.default = Scale.minor
Root.default.set("C")

lead = PDelta(P[0, 2, 1, -2, -1], start=0, length=32)
bassline = PDelta(P[0, 0, 3, -1], start=-5, length=16)

p1 >> pluck(lead, dur=0.5, sus=0.42, oct=4, amp=0.2)
p2 >> triangle(bassline, dur=1, sus=0.85, oct=3, amp=0.12, pan=-0.2)
p3 >> play("x---", dur=1, sample=0, amp=0.16)
