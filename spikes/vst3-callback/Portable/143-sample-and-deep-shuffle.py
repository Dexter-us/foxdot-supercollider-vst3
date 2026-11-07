# Example 143: sample unique positions and make seeded shuffled phrase copies.
Clock.bpm = 96
Scale.default = Scale.major

notes = P[0, 2, 4, 5, 7, 9, 11, 12]

# sample(n) picks n source positions without replacement.
# Supplying a seed makes the same selection reproducible.
p1 >> pluck(notes.sample(4, seed=143), dur=0.5, oct=4, amp=0.22)

# deep_shuffle(n) concatenates n independently shuffled copies.
# Each copy keeps all eight notes; this seed makes the sequence repeatable.
p2 >> triangle(notes.deep_shuffle(2, seed=42),
               dur=0.25, oct=3, amp=0.14)
