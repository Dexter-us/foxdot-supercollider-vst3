# FoxDot-style phrase concatenation and pattern zipping.
Clock.bpm = 102
Scale.default.set("dorian")

joined = P[0, 2, None, 4] | P[7, 5]
chords = P[0, 2, None, 5] & P[4, 7]

p1 >> saw(joined, dur=0.5, oct=5, sus=0.36, amp=0.4, pan=-0.25)
p2 >> pulse(chords, dur=0.5, oct=4, sus=0.32, amp=0.24, pan=0.3)
