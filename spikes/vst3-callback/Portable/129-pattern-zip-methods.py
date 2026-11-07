# Example 129: zip() makes paired-note chords; deepzip/deeprzip retain nesting.
Clock.bpm = 92
Scale.default = Scale.minor
Root.default.set("C")

melody = P[0, 2, 4, 7]
harmony = P[2, 4, 5, 3]
p1 >> pluck(melody.zip(harmony), dur=0.5, oct=4, amp=0.2)

# Nested phrases remain nested; deeprzip reverses each paired note.
left = P[P[0, 2], P[4, 6]]
right = P[P[1, 3], P[5, 7]]
paired = left.deepzip(right)
reversed_pairs = left.deeprzip(right)

p2 >> triangle(P[paired[0][0], paired[1][0]],
               dur=1, oct=3, amp=0.12, pan=-0.25)
p3 >> triangle(P[reversed_pairs[0][0], reversed_pairs[1][0]],
               dur=1, oct=4, amp=0.1, pan=0.25)
