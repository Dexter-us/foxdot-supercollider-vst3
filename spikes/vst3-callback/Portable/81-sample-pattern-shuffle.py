# Example 81: FoxDot PShuf makes a new random ordering while preserving every symbol.
Clock.bpm = 104

riff = PShuf(P["x", "o", "x", " ", "-", " ", "o", " "])
p1 >> play(riff, dur=0.5, sample=0, amp=0.25)
