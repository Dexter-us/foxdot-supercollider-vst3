# Example 73: linvar glides playback rate; sample choices switch at each halfway point.
# Sample selection is discrete; it does not crossfade between WAV files.
Clock.bpm = 96

p2.stop()
p3.stop()
p4.stop()
p5.stop()
p6.stop()
p7.stop()
p8.stop()

p1 >> play("x-o-", dur=0.5, sample=linvar([0, 1], 4),
          rate=linvar([0.75, 1.5], 4), amp=0.48, pan=0)
