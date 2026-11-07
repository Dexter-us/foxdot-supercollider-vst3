# Example 135: PSquare generates 0, 1, 4, 9...; modulo keeps pitches in range.
Clock.bpm = 92
Scale.default = Scale.minor
Root.default.set("C")

lead = PSquare(length=32) % 8
bassline = (PSquare(length=16) % 5) - 3

p1 >> pluck(lead, dur=0.5, sus=0.42, oct=4, amp=0.2)
p2 >> triangle(bassline, dur=1, sus=0.85, oct=3, amp=0.12, pan=-0.2)
p3 >> play("x---", dur=1, sample=0, amp=0.16)
