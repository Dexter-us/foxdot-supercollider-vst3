# Example 159: inspect a Pattern by index and build a reordered phrase.
Clock.bpm = 104
Scale.default = Scale.minor

riff = P[0, 2, 4, 7]
steps = riff.items()
answer = P[steps[0][1], steps[2][1], steps[1][1], steps[3][1]]

p1 >> pluck(riff, dur=0.5, oct=4, amp=0.18, pan=-0.2)
p2 >> triangle(answer, dur=0.5, oct=3, amp=0.14, pan=0.2)
