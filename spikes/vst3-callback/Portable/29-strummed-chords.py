# Example 29: FoxDot-style strum staggers notes within each chord.
Clock.bpm = 96
Scale.default = Scale.major
Root.default.set("C")

# At this tempo, 0.06 beats spaces each chord tone by about 38 ms.
p1 >> triangle(P[I, V7, IV, I7], dur=2, sus=1.8, oct=4, strum=0.06, amp=0.32)

# Each chord can use a different strum value as the time-var cycles.
p2 >> saw(P[V7, I7, IV7, V7], dur=2, sus=1.8, oct=3,
          strum=var([0.02, 0.05], 2), amp=0.18, pan=0.25)
