# Example 110: bounded, looping FoxDot-style player degree transformations.
Clock.bpm = 112
Scale.default.set("minor")

# Every four beats, reverse the degree cycle. The player keeps playing while
# the embedded runtime expands a bounded, repeating schedule.
p1 >> pluck(P[0, 2, 4, 6], dur=0.5, sus=0.42, oct=5,
           amp=0.22, pan=-0.25)
p1.every(4, "reverse")

# FoxDot 0.6.7's sometimes() uses PRand(8, 32) / 2 beat intervals.
# Dexter chooses a seeded repeating interval phrase so the schedule stays
# bounded and can run without a Python server after evaluation.
p2 >> triangle(P[0, 4], dur=0.25, sus=0.2, oct=6,
               amp=0.11, pan=0.3)
p2.sometimes("rotate")

# Try reverse/rotate on dense single-note patterns first. This embedded
# schedule subset keeps timing, amp, and synth controls in place.
