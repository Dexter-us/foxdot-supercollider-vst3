# Example 86: PZip builds chord patterns; PZip2 filters pairs with a rule.
Clock.bpm = 92
Scale.default = Scale.dorian

# Zip two scale-degree phrases into changing two-note chords.
p1 >> saw(
    PZip(P[0, 2, 4, 5], P[4, 5, 7, 9]),
    dur=2,
    sus=1.6,
    oct=4,
    amp=0.18,
)

# Keep only moving intervals, then play the resulting chord sequence.
p2 >> triangle(
    PZip2(P[0, 1, 2, 3], P[0, 2, 4], rule=lambda a, b: a != b),
    dur=0.5,
    sus=0.42,
    oct=5,
    amp=0.12,
    pan=0.25,
)
