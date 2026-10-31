# Example 85: PRhythm turns Euclidean tuples into grouped note delays;
# PQuicken creates a rapidly tightening burst of echoes.
Clock.bpm = 96
Scale.default = Scale.major

# The odd hits expand into the three pulses of a 3-in-8 Euclidean group.
p1 >> play(
    "xoxoxoxo",
    dur=PRhythm([1, (3, 8)]),
    sample=0,
    amp=0.22,
)

# PQuicken returns four bounded beat offsets for each note.
p2 >> sine(
    P[0, 2, 4, 7],
    dur=1,
    sus=0.02,
    delay=PQuicken(0.35, stepsize=2, steps=4),
    oct=4,
    amp=0.16,
    pan=0.2,
)
