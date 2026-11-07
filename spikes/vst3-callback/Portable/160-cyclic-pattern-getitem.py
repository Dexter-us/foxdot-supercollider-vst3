# Example 160: getitem wraps finite indices, unlike ordinary pattern[index].
Clock.bpm = 102
Scale.default = Scale.minor

riff = P[0, 2, 4, 7]
positions = P[0, 2, 4, 6]
wrapped_riff = riff.getitem(positions)

p1 >> pluck(riff, dur=0.5, oct=4, amp=0.18, pan=-0.2)
p2 >> triangle(wrapped_riff, dur=0.5, oct=3, amp=0.14, pan=0.2)
