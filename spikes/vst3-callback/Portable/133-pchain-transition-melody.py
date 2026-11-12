# Example 133: PChain follows note transitions; repeated options weight a choice.
Clock.bpm = 100
Scale.default = Scale.minor
Root.default.set("C")

lead = PChain({
    0: [2, 4, 4],
    2: [0, 4, 7],
    4: [2, 7],
    7: [2, 4, 9],
    9: [0, 4],
}, length=32, seed=133)
drumLine = PChain({
    "x": ["x", "o"],
    "o": ["x", "o", "-"],
    "-": ["x", "o"],
}, length=16, seed=33)

p1 >> pluck(lead, dur=0.5, sus=0.42, oct=4, amp=0.2)
p2 >> play(drumLine, dur=1, sample=0, amp=0.16)
p3 >> triangle(P[0, 4, 2, 7], dur=2, sus=1.8, oct=3,
               amp=0.1, pan=-0.2)
