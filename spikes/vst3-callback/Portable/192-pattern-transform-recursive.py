# Example 192: transform applies one safe function to every nested note leaf.
Clock.clear()
Clock.bpm = 100
Scale.default = Scale.minor

source = P[0, PGroup(-2, 4), P[6, 8]]
positive = source.transform(abs)
raised = positive.transform(lambda note: note + 1)

# The nested PGroup stays a simultaneous chord; the nested P stays nested.
p1 >> pluck(raised, dur=1, oct=4, amp=0.18, pan=-0.2)
p2 >> triangle(P[0, -3, PGroup(5, 7)].transform(abs),
               dur=0.5, oct=5, amp=0.10, pan=0.25)
