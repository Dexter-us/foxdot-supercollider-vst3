# Example 71: two kick variations and an easily heard forward/reverse chirp.
Clock.bpm = 100
p3.stop()
p4.stop()
p5.stop()
p6.stop()
p7.stop()
p8.stop()
p1 >> play("x", dur=1, sample=P[0, 1], amp=0.5, pan=-0.25)
# A rising test chirp should become falling when rate=-1.
# The second chirp file is a 48000 Hz stereo input, downmixed for this batch.
p2 >> play("a", dur=2, sample=P[0, 1, 0, 1],
           rate=P[1, 1, -1, -1], amp=0.45, pan=0.25)
