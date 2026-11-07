# Example 116: FoxDot-style groups can update pitch and rhythm together.
Clock.bpm = 96
Scale.default = Scale.major
Root.default.set("C")

p1 >> triangle(P[0, 2, 4, 6], dur=1, sus=0.45, oct=4, amp=0.24)
p2 >> pluck(P[0, 2, 4, 6], dur=1, sus=0.45, oct=5, amp=0.16, pan=0.25)

melody = Group(p1, p2)
melody.degree = P[0, 2, 4, 6]
melody.scale = Scale.minor
melody.oct = var([3, 4], [1, 1])
melody.dur = var([0.5, 1], [1, 1])
melody.sus = 0.35

# Sample players can share timing, but not note, scale, or octave controls.
p3 >> play("x---", dur=1, sample=0, amp=0.24)
drums = Group(p3)
drums.dur = 1
drums.sus = 0.3
