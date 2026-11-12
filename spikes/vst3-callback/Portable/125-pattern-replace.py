# Example 125: replace selected note values without reshaping the phrase.
Clock.bpm = 100
Scale.default = Scale.major
Root.default.set("C")

# Only matching degrees change; their positions and repeated rhythm remain.
p1 >> pluck(P[0, 2, 4, 2, 7, 2].replace(2, 3),
            dur=0.5, oct=4, amp=0.18)

# replace() also works on nested chord groups.
p2 >> triangle(P[PGroup(0, 4, 7), PGroup(2, 5, 9)].replace(4, 5),
               dur=1, oct=3, amp=0.14, pan=-0.25)

# Sample symbols can be remapped without expanding or flattening the pattern.
p3 >> play(P["x", "-", "o", "x"].replace("x", "o"),
           dur=0.5, sample=0, amp=0.12)
