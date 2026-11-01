# Example 79: FoxDot PAlt cycles each input to the LCM length, then alternates items.
Clock.bpm = 96

riff = PAlt(P["x", " "], P["o", "-", " "])
p1 >> play(riff, dur=0.5, sample=P[0, 1], amp=0.28)
