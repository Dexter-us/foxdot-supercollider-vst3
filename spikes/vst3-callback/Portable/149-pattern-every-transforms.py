# Example 149: repeat a phrase, then apply one bounded Pattern transform.
Clock.bpm = 104
Scale.default = Scale.minor

descending_turn = P[0, 2, 4, 7].every(3, "reverse")
rotated_turn = P[0, 3, 5].every(2, "rotate", 1)
stuttered_turn = P[0, 2, 5].every(2, "stutter", 2)

p1 >> pluck(descending_turn, dur=0.25, oct=4, amp=0.18)
p2 >> triangle(rotated_turn, dur=0.5, oct=4, amp=0.12, pan=-0.25)
p3 >> bass(stuttered_turn, dur=0.5, oct=5, amp=0.16, pan=0.25)
