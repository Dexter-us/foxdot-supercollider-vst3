# Example 132: seeded PWalk creates a finite melodic contour.
Clock.bpm = 104
Scale.default = Scale.minor
Root.default.set("C")

lead = PWalk(max=7, step=P[1, 2, 1, 1], start=0,
             length=32, seed=132)
bassline = PWalk(max=3, step=1, start=0, length=16, seed=42)

p1 >> pluck(lead, dur=0.25, sus=0.22, oct=4, amp=0.2)
p2 >> triangle(bassline, dur=1, sus=0.85, oct=3, amp=0.12, pan=-0.2)
p3 >> play("x---", dur=1, sample=0, amp=0.16)
