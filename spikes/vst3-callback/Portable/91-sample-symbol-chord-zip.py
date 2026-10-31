# Example 91: FoxDot's & zips finite sample patterns into simultaneous hits.
# Inputs cycle to their least-common-multiple length (up to 256 steps).
# A space rests only that layer; each zipped hit is limited to 8 symbols.

Clock.bpm = 96

# The kick and snare patterns overlap on the first hit.
p1 >> play(
    P["x", " ", "x", " "] & P["o", "o", " ", "o"],
    dur=0.5,
    sample=0,
    amp=0.24,
)

# Unequal pattern lengths cycle to 6 steps; the two patterns combine into
# simultaneous kick/hat hits wherever both lanes are active.
p2 >> play(
    P["x", " "] & P["-", "o", " "],
    dur=1,
    sample=P[0, 1],
    amp=0.12,
    pan=0.25,
)
