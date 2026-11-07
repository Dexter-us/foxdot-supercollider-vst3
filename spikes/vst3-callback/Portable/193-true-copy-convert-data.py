# Example 193: shallow pattern copies and recursive type conversion.
Clock.clear()
Clock.bpm = 96
Scale.default = Scale.dorian

nested = P[2.8, 4.2]
source = P[0.5, nested, PGroup(5.7, 7.1)]
snapshot = source.true_copy()

# The top-level list is separate; nested patterns remain shared references.
snapshot[0] = 1.5
whole_notes = snapshot.convert_data(int)
floating_notes = source.convert_data()

p1 >> pluck(whole_notes, dur=1, oct=4, amp=0.18, pan=-0.2)
p2 >> triangle(floating_notes, dur=0.5, oct=5, amp=0.09, pan=0.25)
