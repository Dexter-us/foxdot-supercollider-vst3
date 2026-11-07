# Example 212: explicit seconds, beats, and bar units on existing P/var helpers.
Clock.bpm = 120
Scale.default = Scale.minor

# dur is still measured in beats; seconds() converts each seeded random value.
p1 >> pluck(PRand([0, 2, 4, 7], length=16, seed=21),
           dur=seconds(PWhite(0.25, 0.5, length=16, seed=22)),
           sus=seconds(linvar([0.18, 0.3], [4, 4])),
           amp=0.22)

# bars() uses the embedded fixed 4/4 meter; beats() makes an existing beat unit explicit.
p2 >> play("x-o-", dur=bars(P[0.25, 0.5]),
           sus=seconds(0.18), sample=0,
           pan=beats(var([-0.25, 0.25], [4, 4])))
