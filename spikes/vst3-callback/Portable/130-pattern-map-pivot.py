# Example 130: map() remaps phrase values; pivot() reorients around steps.
Clock.bpm = 88
Scale.default = Scale.minor
Root.default.set("C")

phrase = P[0, 2, 4, 7]
mapped = phrase.map({0: 0, 2: 3, 4: 5, 7: 6})
pivoted = phrase.pivot(P[1, 3])

p1 >> pluck(mapped, dur=0.5, oct=4, amp=0.2)
p2 >> triangle(pivoted, dur=0.5, oct=3, amp=0.13, pan=0.3)
p3 >> play("x---", dur=1, sample=0, amp=0.18)
