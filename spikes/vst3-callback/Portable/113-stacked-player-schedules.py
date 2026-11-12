# Example 113: two independent schedule methods on one numeric player.
Clock.bpm = 108
Scale.default.set("minor")

p1 >> pluck(P[0, 2, 4, 6], dur=0.5, sus=0.38, oct=5,
            amp=0.22, pan=-0.2)

# Often reshuffles the degree order; every beat, stutter the current note.
p1.often("shuffle")
p1.every(1, "stutter", 3)
