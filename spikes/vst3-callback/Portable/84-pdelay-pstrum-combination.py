# Example 84: PDelay adds a three-hit delayed group; PStrum supplies the rhythm.
Clock.bpm = 96

p1 >> triangle(
    P[0, 4, 7, 11, 9, 5, 2, 6, 10, 8, 3, 1],
    dur=PStrum(4),
    delay=PDelay(3, 8),
    sus=0.2,
    oct=4,
    amp=0.22,
    pan=-0.12,
)
