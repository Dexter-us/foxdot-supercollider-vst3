# Example 145: duplicate() and iter() repeat finite phrases without flattening chords.
Clock.bpm = 96
Scale.default = Scale.minor

phrase = P[0, PGroup(2, 4), 5, PGroup(7, 9)]
p1 >> pluck(phrase.duplicate(2), dur=0.5, sus=0.45,
            oct=4, amp=0.22)

riff = P[-2, PGroup(0, 3), 4]
p2 >> triangle(riff.iter(3), dur=0.5, sus=0.45,
               oct=3, amp=0.14, pan=0.25)
