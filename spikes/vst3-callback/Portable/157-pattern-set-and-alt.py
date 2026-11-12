# Example 157: set updates a shared phrase; alt creates a separate phrase.
Clock.bpm = 108
Scale.default = Scale.dorian

riff = P[0, 2, 4, 7]
shared_riff = riff
riff.set(1, 3)

# alt() returns a new Pattern and leaves riff/shared_riff unchanged.
answer = riff.alt(P[7, 9, 11, 14])

p1 >> pluck(riff, dur=0.5, oct=4, amp=0.18, pan=-0.25)
p2 >> triangle(shared_riff, dur=0.5, oct=3, amp=0.14, pan=0.25)
p3 >> saw(answer, dur=0.5, oct=2, amp=0.10)
