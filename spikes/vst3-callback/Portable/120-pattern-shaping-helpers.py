# Example 120: three bounded FoxDot pattern-shaping helpers.
Clock.bpm = 100
Scale.default = Scale.dorian
Root.default.set("D")

# Cumulative degrees make a rising phrase; it wraps within this finite pattern.
p1 >> pluck(P[1, 1, 2, 3].accum(), dur=1, oct=5, sus=0.72,
            amp=0.2, pan=-0.2)

# Reverse each two-step block for a compact rhythmic-note rearrangement.
p2 >> triangle(P[0, 2, 4, 6, 1, 3].swap(2), dur=0.5, oct=4, sus=0.38,
               amp=0.13, pan=0.25)

# mirror() reverses the chord order and the note order inside each chord.
p3 >> sine(P[PGroup(0, 2, 4), PGroup(1, 3, 5), PGroup(2, 4, 6)].mirror(),
           dur=2, sus=1.5, oct=5, strum=0.025, amp=0.09, pan=0.4)
