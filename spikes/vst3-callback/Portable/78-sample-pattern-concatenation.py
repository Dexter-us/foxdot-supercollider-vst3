# Example 78: FoxDot's | operator concatenates patterns left to right.
# Keep sample symbols and space-rests in P patterns; rotate() still works after joining.
Clock.bpm = 96

beat = P["x", " ", "x", " "] | P["o", "-", " ", "-"]
p1 >> play(beat.rotate(2), dur=0.5, sample=P[0, 1], amp=0.30)
