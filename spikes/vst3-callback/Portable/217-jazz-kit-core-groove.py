# Example 217: a compact groove using the familiar kick and core jazz-kit voices.
# Together, Examples 217 and 218 demonstrate every JazzKit instrument.
Clock.bpm = 108
Scale.default = Scale.minor

p1 >> userSynth("JazzKitKick", P[0, 0, 0, 0], dur=1, sus=0.28,
                amp=0.2)
p2 >> userSynth("JazzKitBrushTap", P[0, 0], dur=2, sus=0.7,
                amp=0.12, pan=-0.18)
p3 >> userSynth("JazzKitBrushSweep", P[0], dur=4, sus=3.7,
                amp=0.07, pan=0.12)
p4 >> userSynth("JazzKitClosedHat", P[0, 0, 0, 0], dur=1, sus=0.12,
                amp=0.045, pan=0.28)
p5 >> userSynth("JazzKitOpenHat", P[0], dur=4, sus=0.65,
                amp=0.035, pan=0.3)
p6 >> userSynth("JazzKitRide", P[0, 0, 0, 0], dur=1, sus=0.5,
                amp=0.045, pan=0.34)
p7 >> userSynth("JazzKitCrossStick", P[0, 0], dur=2, sus=0.035,
                amp=0.07, pan=-0.26)
p8 >> userSynth("JazzKitRackTom", P[0, 2], dur=4, sus=0.45,
                amp=0.08, pan=-0.1)
