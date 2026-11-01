# Example 72: var() changes discrete sample choice and playback direction in time.
# x has two included test kicks: sample 0 = normal kick; sample 1 = tighter kick.
Clock.bpm = 100

p3.stop()
p4.stop()
p5.stop()
p6.stop()
p7.stop()
p8.stop()

p1 >> play("x o", dur=0.5, sample=var([0, 1], 4),
          rate=var([1, -1], 4), amp=0.55, pan=-0.15)

# The chirp is a second, quieter line to make the changing sample/rate easy to hear.
p2 >> play("a ", dur=1, sample=var([0, 1], 4),
          rate=var([1, -1], 4), amp=0.24, pan=0.2)
