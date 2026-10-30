# Example 49: original FoxDot prophet graph (core UGens only).
# Test this synth alone first. Longer spacing lets its envelope finish.
# Change notes/rate/amp, then Run again to test live edits and retriggering.
Clock.bpm = 90
Scale.default = Scale.minor
Root.default.set("C")

# Make this a solo test even after a multi-player example.
p2.stop()
p3.stop()
p4.stop()
p5.stop()
p6.stop()
p7.stop()
p8.stop()

p1 >> prophet(P[0, 2, 4, 7], dur=4, sus=0.5, oct=5,
             amp=0.5, reverb="none")

# To stop: uncomment this line without adding leading spaces:
# p1.stop()
