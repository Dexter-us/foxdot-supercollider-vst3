# Example 29: A gentle two-layer FoxDot-style strum, spaced in beats.
Clock.bpm = 84
Scale.default = Scale.major
Root.default.set("C")

# 0.04 beats is about 29 ms per chord tone at 84 BPM.
p1 >> triangle(P[I, V7, IV, I7], dur=2, sus=1.75, oct=3, strum=0.04, amp=0.28, pan=-0.15)

# The brighter layer varies gently from about 14 to 36 ms between notes.
p2 >> sine(P[V7, I7, IV7, V7], dur=2, sus=1.75, oct=4,
          strum=var([0.02, 0.05], 2), amp=0.16, pan=0.2)
