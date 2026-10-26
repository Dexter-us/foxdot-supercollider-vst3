# Finite FoxDot-style P[...] literals for notes, rests, and chords.
Clock.bpm = 104
Scale.default.set("minor")

melody = P[0, 2, None, (3, 5), 7, 5, 3, None]
bass = P[0, None, 3, None]

p1 >> saw(melody, dur=0.5, oct=5, sus=0.38, amp=0.65, pan=-0.2)
p2 >> pulse(bass, dur=1, oct=4, sus=0.72, amp=0.28, pan=0.25)
