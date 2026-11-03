# Example 137: Pvar changes whole phrases while the lead's note lengths vary.
# Phrase spans must line up with the repeating dur pattern.
Clock.bpm = 108
Scale.default = Scale.minor
Root.default.set("C")

# The eight-note melody switches every six beats and restarts at phrase item 1.
melody_a = P[0, 3, 5, 7, 10, 7, 5, 3]
melody_b = P[0, 5, 7, 10, 12, 10, 7, 5]
p1 >> pluck(Pvar([melody_a, melody_b], [6, 6]),
            dur=P[0.5, 1.0], sus=0.8, oct=4, amp=0.34)

# Spaces are rests; the drum phrase changes on the same six-beat boundary.
p2 >> play(Pvar(["x   o   ", "x x oo o"], [6, 6]),
           dur=0.75, sample=0, amp=0.24, pan=-0.15)
