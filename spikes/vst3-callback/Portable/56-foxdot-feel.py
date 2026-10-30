# Example 56: original FoxDot feel graph (core UGens only).
# Test this synth alone first. Longer spacing lets its envelope finish.
# Change notes/rate/amp, then Run again to test live edits and retriggering.
Clock.bpm = 90
Scale.default = Scale.minor
Root.default.set("C")

p1 >> feel(P[0, 2, 4, 7], dur=4, sus=0.5, oct=4,
             amp=0.5, reverb="none")

# To stop: uncomment this line without adding leading spaces:
# p1.stop()
