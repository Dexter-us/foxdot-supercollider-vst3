# Bounded finite range patterns and composable pattern slicing.
Clock.bpm = 100
Scale.default.set("dorian")

ascending = P[0:8]
skipping = P[0:8:2]
descending = P[8:0:-2][1:3]

p1 >> saw(ascending, dur=0.25, oct=5, sus=0.18, amp=0.3, pan=-0.4)
p2 >> pulse(skipping, dur=0.5, oct=4, sus=0.35, amp=0.22, pan=0.35)
p3 >> triangle(descending, dur=0.5, oct=5, sus=0.28, amp=0.18, pan=0.05)
