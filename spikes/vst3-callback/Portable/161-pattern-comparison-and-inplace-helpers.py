# Example 161: comparison masks, cyclic slices, and alias-visible edits.
Clock.bpm = 100
Scale.default = Scale.minor

riff = P[0, 2, 4, 7]
mask = riff.eq(P[0, 2, 5, 7])
selected = riff.select(mask)
ending = riff.getslice(1, 4)

edited = riff.copy()
edited.i_rotate(1)
edited.i_reverse()
edited.i_sort()
edited.i_shuf()
edited.setitem(0, 5)

p1 >> pluck(riff, dur=0.5, oct=4, amp=0.16, pan=-0.25)
p2 >> triangle(selected, dur=1, oct=3, amp=0.12, pan=0.25)
p3 >> sine(ending, dur=0.5, oct=4, amp=0.1)
p4 >> varsaw(edited, dur=0.25, oct=3, amp=0.08, pan=0.4)
