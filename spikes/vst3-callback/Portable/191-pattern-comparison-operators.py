# Example 191: FoxDot-style pattern comparisons create reusable 0/1 masks.
Clock.clear()
Clock.bpm = 104
Scale.default = Scale.major

degrees = P[0, 2, 4, 7, 9, 11]
lower_mask = degrees <= 4
upper_mask = degrees >= 7
same_notes = degrees == P[0, 2, 4, 7, 9, 11]
changed_notes = degrees != P[0, 2, 4, 8, 9, 11]

# select() cycles the mask and keeps the notes whose comparison result is 1.
lower_notes = degrees.select(lower_mask)
upper_notes = degrees.select(upper_mask)

# Unary ~ mirrors both the pattern and any nested group order.
p1 >> pluck(lower_notes, dur=0.5, oct=4, amp=0.20, pan=-0.2)
p2 >> triangle(~upper_notes, dur=0.5, oct=5, amp=0.10, pan=0.25)

# All six relational operators return numeric masks that can be composed.
ascending_mask = degrees < 7
p3 >> sine(degrees.select(ascending_mask), dur=1, oct=3, amp=0.08)
