# Example 90: FoxDot-style PGroup sustains are laced across chord tones.
Clock.bpm = 96
Scale.default = Scale.dorian

# Each chord tone receives the matching sustain; shorter gates release first.
susGroups = P[PGroup(0.25, 0.45), PGroup(0.35, 0.65)]
p1 >> triangle(P[PGroup(0, 2, 4), PGroup(4, 6)],
               dur=1, sus=susGroups, oct=4, amp=0.14)

# Grouped sustain also works with the bundled sample player.
p2 >> play("x o ", dur=1, sus=PGroup(0.25, 0.5), sample=0, amp=0.12)
