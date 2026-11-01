# Example 92: PZip and PZip2 can combine finite sample-symbol patterns.
# Inputs cycle to their least-common-multiple length; each hit is at most 8 symbols.

Clock.bpm = 96

# Three unequal motifs lace across 12 steps. A space silences only its own lane.
p1 >> play(
    PZip(
        P["x", " ", "x"],
        P["o", "o"],
        P["-", " ", "-", "-"],
    ),
    dur=0.5,
    sample=0,
    amp=0.22,
)

# PZip2 filters paired symbols before creating each simultaneous hit.
p2 >> play(
    PZip2(
        P["-", " ", "-", "o"],
        P["o", "x", " ", "-"],
        rule=lambda a, b: a != b,
    ),
    dur=1,
    sample=0,
    amp=0.12,
    pan=0.25,
)
