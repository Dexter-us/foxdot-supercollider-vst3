# Example 185: flatten one nested Pattern level into a single chord.
Clock.bpm = 96
Scale.default = Scale.minor

wide_chord = P[0, PGroup(2, 4), 7].flatten()
upper_chord = P[2, PGroup(4, 7), 11].flatten()

# Each flattened PGroup plays simultaneously as one chord event.
p1 >> pluck(wide_chord, dur=2, sus=1.8, oct=4, amp=0.22, pan=-0.2)
p2 >> triangle(upper_chord, dur=2, sus=1.8, oct=3, amp=0.12, pan=0.25)
