# Live player edit test: update one part while the other keeps playing.
# Run both lines once. Then change p1 and comment out the p2 assignment before Run.
# Only p1 is replaced; p2 continues until you run p2.stop().
Clock.bpm = 100
Scale.default.set("dorian")

p1 >> saw(P[0, 2, 4, 7], dur=0.5, oct=5, sus=0.35, amp=0.32, pan=-0.3)
p2 >> pulse(P[0, None, 3, 5], dur=0.25, oct=4, sus=0.18, amp=0.22, pan=0.35)
