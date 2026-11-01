# Example 102: bounded FoxDot-style PRand and PWhite patterns.
Clock.bpm = 100
Scale.default = Scale.minor

# PRand chooses from these scale degrees; the seeded 16-note phrase is repeatable.
p1 >> pluck(PRand([0, 2, 3, 5, 7], length=16, seed=42),
           dur=0.5, oct=4, amp=0.22)

# PWhite uses FoxDot's triangular distribution for varied note durations.
# PRand(0, 7) chooses inclusive integer scale degrees.
p2 >> triangle(PRand(0, 7, length=16, seed=7),
               dur=PWhite(0.25, 0.75, length=16, seed=12),
               oct=3, amp=0.12, pan=0.25)
