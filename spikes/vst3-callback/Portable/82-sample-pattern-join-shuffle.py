# Example 82: assemble two motifs with PJoin, then shuffle the complete phrase.
Clock.bpm = 104

motifs = [P["x", " ", "o", " "], P["x", "-", " "]]
riff = PShuf(PJoin(motifs))
p1 >> play(riff, dur=0.5, sample=0, amp=0.25)
