# Example 111: often/rarely shortcuts, degree shuffle, and scheduled stutter.
Clock.bpm = 110
Scale.default.set("minor")

# FoxDot's often() chooses PRand(1, 8) / 2 beat intervals (0.5–4 beats).
p1 >> pluck(P[0, 2, 4, 6], dur=0.5, sus=0.42, oct=5,
            amp=0.2, pan=-0.3)
p1.often("shuffle")

# rarely() uses PRand(32, 64) / 2 beat intervals (16–32 beats).
p2 >> triangle(P[0, 4], dur=0.25, sus=0.2, oct=4,
               amp=0.17, pan=0.3)
p2.rarely("reverse")

# stutter repeats the current note inside its remaining step duration.
# The third argument bounds each trigger to three total notes.
p3 >> sine(P[0, 2, 4, 6], dur=0.5, sus=0.4, oct=3,
           amp=0.12, pan=0.05)
p3.every(1, "stutter", 3)
