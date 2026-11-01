# Example 87: FoxDot allows notes to overlap when sus is longer than dur.
Clock.bpm = 100
Scale.default = Scale.minor

# Repeated pitches ring over one another; the engine keeps a bounded
# eight-voice safety limit per player.
p1 >> sine(P[0, 0, 2, 2], dur=0.5, sus=1.0, oct=4, amp=0.16)

# Sustained chords cross the pattern loop boundary and remain independent voices.
p2 >> triangle(
    P[(0, 2, 4), (2, 4, 6)],
    dur=1,
    sus=1.5,
    oct=5,
    amp=0.12,
    pan=0.2,
)
