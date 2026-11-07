# Example 117: Group.degree can move through a shared var/linvar melody.
Clock.bpm = 96
Scale.default = Scale.minor
Root.default.set("C")

p1 >> triangle(P[0, 0, 0, 0], dur=0.5, sus=0.4, oct=4,
               amp=0.22)
p2 >> pluck(P[0, 0, 0, 0], dur=0.5, sus=0.4, oct=5,
            amp=0.14, pan=0.25)

melody = Group(p1, p2)
melody.degree = var([0, 2, 4, 6], [0.5, 0.5, 0.5, 0.5])

# A second group follows a linear degree contour, quantized to scale steps.
p3 >> sine(P[0, 0, 0, 0], dur=0.5, sus=0.3, oct=5,
           amp=0.1, pan=-0.3)
answer = Group(p3)
answer.degree = linvar([0, 4], [1, 1])
