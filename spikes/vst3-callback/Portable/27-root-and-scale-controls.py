# Example 27: FoxDot-style global key, scale assignment, and per-player roots.
Clock.bpm = 104
Scale.default = Scale.dorian
Root.default.set("D")

# Uses the default D Dorian scale and key.
p1 >> triangle(P[0, 2, 4, 6], dur=0.5, oct=5, amp=0.35)

# This player uses its own scale and the pitch-class root G.
p2 >> saw(P[(0, 2, 4), None, (1, 3, 5), None], dur=1, oct=4,
          scale=Scale.harmonicMinor, root=7, amp=0.25, pan=-0.35)

# Note names are also accepted as per-player roots.
p3 >> pulse(P[0, 2, 4, 6], dur=0.5, oct=5, scale=Scale.major,
            root="Eb", amp=0.2, pan=0.35)
