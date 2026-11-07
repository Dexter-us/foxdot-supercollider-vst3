# Example 218: compare the familiar low kick with the intentionally gritty
# JazzKitBrushKick. Use this instead of Example 217, not at the same time.
Clock.bpm = 96
Scale.default = Scale.minor

p1 >> userSynth("JazzKitKick", P[0, 0, 0, 0], dur=1, sus=0.3,
                amp=0.19, pan=-0.12)
p2 >> userSynth("JazzKitBrushKick", P[0, None, 0, None], dur=1,
                sus=0.3, amp=0.12, pan=0.12)
p3 >> userSynth("JazzKitBrushTap", P[0, 0], dur=2, sus=0.75,
                amp=0.105, pan=-0.22)
p4 >> userSynth("JazzKitBrushSweep", P[0], dur=4, sus=3.7,
                amp=0.06, pan=0.18)
p5 >> userSynth("JazzKitClosedHat", P[0, 0, 0, 0], dur=1, sus=0.12,
                amp=0.04, pan=0.3)
p6 >> userSynth("JazzKitOpenHat", P[0], dur=4, sus=0.6,
                amp=0.03, pan=0.34)
p7 >> userSynth("JazzKitRide", P[0, 0], dur=2, sus=0.55,
                amp=0.04, pan=0.3)
p8 >> userSynth("JazzKitCrossStick", P[0, 0], dur=2, sus=0.035,
                amp=0.065, pan=-0.3)
