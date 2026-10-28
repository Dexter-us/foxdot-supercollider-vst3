# Combine deterministic FoxDot-style pattern transformations.
Clock.bpm = 110
Scale.default.set("dorian")

melody = interleave(palindrome([0, 2, 4, 6]), [None, 7, None])
rhythm = stutter([0, None, 3], 2)

p1 >> saw(melody, dur=0.5, oct=5, sus=0.35, amp=0.65, pan=-0.25)
p2 >> pulse(rhythm, dur=0.25, oct=4, sus=0.18, amp=0.25, pan=0.3)
