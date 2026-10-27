# Event-level var/linvar controls: changes are sampled at each note start.
# dur changes the step grid; oct is rounded to an integer octave per note.
Clock.bpm = 96
Scale.default.set("minor")

p1 >> triangle(P[0, 1, 2, 3, 4, 5, 6, 0, 0, 1, 2, 3, 4, 5, 6, 0], dur=var([0.25, 0.5], [2, 2]), oct=var([4, 5], [2, 2]), sus=var([0.2, 0.4], [2, 2]), velocity=var([60, 110], [2, 2]), amp=0.25)
p2 >> saw(P[0, 2, 4, 6, 0, 2, 4, 6], dur=linvar([0.25, 0.5], [1, 1]), oct=linvar([4, 5], [1, 1]), sus=linvar([0.1, 0.3], [1, 1]), velocity=linvar([55, 115], [1, 1]), amp=0.15)
