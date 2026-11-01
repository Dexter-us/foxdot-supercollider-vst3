# Example 100: sample start/end are normalized positions within each WAV.
# The first player plays only a short one-shot slice.
Clock.bpm = 88
p1 >> play("x---", dur=1, sample=0, start=0.08, end=0.55,
          amp=0.30, pan=-0.25)

# loop=True repeats only the selected region. It ends at sus= (or dur= if
# sus is omitted), so the loop cannot leave a voice running indefinitely.
p2 >> play("o", dur=4, sus=3.5, sample=0, start=0.12, end=0.42,
          loop=True, amp=0.16, pan=0.25)
