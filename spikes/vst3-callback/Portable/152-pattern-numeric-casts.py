# Example 152: convert a numeric phrase without changing its source.
Clock.bpm = 96
Scale.default = Scale.minor

raw_phrase = P[0.8, 2.4, 4.9, 6.2]
whole_notes = raw_phrase.int()
fractional_notes = P[0, 2, 4, 7].float()
rounded_chord = P[0.6, 2.6, 4.6].int().asGroup()

p1 >> triangle(whole_notes, dur=0.5, oct=4, amp=0.2)
p2 >> pluck(fractional_notes, dur=0.5, oct=4, amp=0.16, pan=0.25)
p3 >> saw(rounded_chord, dur=2, sus=1.8, oct=3, amp=0.12, pan=-0.25)
