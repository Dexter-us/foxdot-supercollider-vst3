# Example 114: cycle offsets and independent schedule identifiers.
Clock.bpm = 96
p1 >> pluck(P[0, 2, 4, 6], dur=0.5, sus=0.38, oct=5,
            amp=0.22, pan=-0.2)

# The shuffle runs on beat 1 of each four-beat cycle.
p1.every(1, "shuffle", cycle=4, ident="phrase")
# A separate named schedule stutters the beat-3 note in that cycle.
p1.every(3, "stutter", 3, cycle=4, ident="fill")
