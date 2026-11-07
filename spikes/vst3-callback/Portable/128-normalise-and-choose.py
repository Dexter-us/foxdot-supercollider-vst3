# Example 128: normalise() scales a contour; choose() picks one phrase value.
Clock.bpm = 100
Scale.default = Scale.major
Root.default.set("C")

# The relative contour becomes a bounded pitch-degree pattern.
shape = P[2, 4, 1, 5, 3].normalise()
p1 >> pluck(shape, dur=0.5, oct=4, amp=0.16)

# choose() selects one pitch when this script evaluates.
picked = P[0, 2, 4, 7].choose()
p2 >> triangle(P[picked, picked + 2, picked + 4],
               dur=1, oct=3, amp=0.13, pan=-0.25)

p3 >> play("x---o---", dur=0.5, amp=0.11)
