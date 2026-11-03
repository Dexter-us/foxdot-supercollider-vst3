# Example 138: Pvar phrases with FoxDot-style var/linvar note durations.
Clock.bpm = 105
Scale.default = Scale.minor

# Each two-beat phrase aligns to the two-beat var cycle.
p1 >> pluck(Pvar([P[0, 3, 5], P[7, 5, 3]], [2, 2]),
            dur=var([0.5, 1.0], [1, 1]), sus=0.4, oct=4, amp=0.28)

# linvar is also supported when its complete cycle aligns with the phrase loop.
# Each one-beat phrase contains a 0.25-beat step then a 0.75-beat step.
p2 >> play(Pvar(["x-", "-o"], [1, 1]),
           dur=linvar([0.25, 1.25], [0.5, 0.5]),
           sample=0, amp=0.2, pan=0.25)
