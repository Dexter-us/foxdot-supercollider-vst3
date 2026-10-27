# Example 26: FoxDot-style scales, custom semitone intervals, and note groups.
Clock.bpm = 104

# Scale.default.set accepts a FoxDot scale name or a custom interval list.
Scale.default.set(Scale.dorian)
p1 >> triangle(P[0, 2, 4, 6, 4, 2, 1, 0], dur=0.5, oct=5, amp=0.45)

# Chords are note groups; each degree is resolved through the player's scale.
p2 >> saw(P[(0, 2, 4), None, (1, 3, 5), None], dur=1, oct=4,
          scale=Scale.melodicMinor, amp=0.3, pan=-0.35)

# Custom scales use ascending semitone intervals beginning at zero.
p3 >> pulse(P[0, 1, 2, 3, 4, 5, 6], dur=0.5, oct=5,
            scale=[0, 2, 3, 5, 7, 8, 10], amp=0.2, pan=0.35)
