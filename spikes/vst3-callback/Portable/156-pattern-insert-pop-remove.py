# Example 156: mutate a live phrase while keeping an earlier copy independent.
Clock.bpm = 100
Scale.default = Scale.minor

riff = P[0, 2, 5, 7]
shared_riff = riff
riff.insert(2, 3)
snapshot = riff.copy()
popped_note = riff.pop()
riff.remove(2)

# p1 and p2 share the shortened riff; snapshot keeps the inserted five notes.
p1 >> pluck(riff, dur=0.5, oct=4, amp=0.18, pan=-0.2)
p2 >> triangle(shared_riff, dur=0.5, oct=3, amp=0.14, pan=0.2)
p3 >> saw(snapshot, dur=0.5, oct=2, amp=0.12)
p4 >> sine(P[popped_note], dur=1, oct=5, amp=0.08)
