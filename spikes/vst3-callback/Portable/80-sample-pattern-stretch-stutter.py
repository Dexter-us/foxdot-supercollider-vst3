# Example 80: PStretch sets the phrase length; PStutter varies repeats per item.
Clock.bpm = 96

phrase = PStretch(P["x", "o", " "], 6)
rhythm = PStutter(phrase, P[1, 2])
p1 >> play(rhythm, dur=0.5, sample=0, amp=0.28)
