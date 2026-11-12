# Example 151: turn a finite phrase into one simultaneous chord.
Clock.bpm = 96
Scale.default = Scale.major

major_stack = P[0, 2, 4, 6].asGroup()
open_stack = P[0, 3, 5].group()

p1 >> triangle(major_stack, dur=2, sus=1.8, oct=4, amp=0.22)
p2 >> pluck(open_stack, dur=2, sus=1.7, oct=3, amp=0.16, pan=0.3)
