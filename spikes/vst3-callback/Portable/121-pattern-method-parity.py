# Example 121: FoxDot method forms for stretching, looping, and shuffling.
Clock.bpm = 104
Scale.default = Scale.dorian
Root.default.set("D")

# stretch(size) repeats the source until it reaches the requested item count.
p1 >> pluck(P[0, 2, 4, 7].stretch(8), dur=0.5, oct=5, sus=0.38,
            amp=0.18, pan=-0.35)

# loop(n) appends the complete phrase n times.
p2 >> triangle(P[0, 3, 5].loop(2), dur=0.5, oct=4, sus=0.32,
               amp=0.11, pan=0.35)

# shuffle(n) emits independent shuffled copies; chord groups stay intact.
p3 >> pluck(P[(0, 4), (2, 5), (4, 7)].shuffle(2), dur=0.5, oct=5,
            sus=0.4, strum=0.018, amp=0.08)
