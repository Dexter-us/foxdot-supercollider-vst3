# Example 28: FoxDot's scale-degree triads and seventh-chord vocabulary.
Clock.bpm = 96
Scale.default = Scale.major
Root.default.set("C")

# Roman-numeral chord constants can be played as chord progressions.
p1 >> triangle(P[I, IV, V7, I7], dur=1, oct=4, amp=0.35)

# PGroup builds a custom chord; this player uses its own minor scale and key.
p2 >> saw(PGroup(0, 2, 4, 7), dur=2, oct=5,
          scale=Scale.minor, root="A", amp=0.25, pan=0.3)
