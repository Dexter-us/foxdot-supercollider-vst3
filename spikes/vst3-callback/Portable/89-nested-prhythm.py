# Example 89: nested duration patterns make grouped rhythmic phrases.
# Each inner duration becomes a bounded retrigger offset inside its group.
Clock.bpm = 96
Scale.default = Scale.dorian

nested = PRhythm([0.75, [0.25, 0.25], (3, 8)])
p1 >> triangle(P[0, 2, 4], dur=nested, sus=0.35, oct=4, amp=0.14)
