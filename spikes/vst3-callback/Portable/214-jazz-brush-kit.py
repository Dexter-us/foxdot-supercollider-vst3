# Example 214: fully synthesized jazz brush kit; no WAV samples or extra UGen pack.
# Install the JazzKit*.scsyndef files and matching .controls sidecars in
# Portable/SynthDefs, restart the DAW, then select players in the Players panel.
Clock.bpm = 112
Scale.default = Scale.minor

# Warm low kick and quiet brush-hat pulse establish a light swing bed.
p1 >> userSynth("JazzKitKick", P[0, 0, 0, 0], dur=1, sus=0.9,
               amp=0.24)
p2 >> userSynth("JazzKitClosedHat", P[0], dur=0.5, sus=0.45,
                amp=0.055, pan=0.28)

# Brush snare comping, alternating with a longer sweep every four beats.
p3 >> userSynth("JazzKitBrushTap", P[0, 0], dur=2, sus=1.8,
                amp=0.14, pan=-0.18)
p4 >> userSynth("JazzKitBrushSweep", P[0], dur=4, sus=3.9,
                amp=0.09, pan=0.12)

# Ride and cross-stick provide two optional comping layers.
p5 >> userSynth("JazzKitRide", P[0], dur=1, sus=0.95,
                amp=0.07, pan=0.32)
p6 >> userSynth("JazzKitCrossStick", P[0, 0], dur=2, sus=1.8,
                amp=0.08, pan=-0.28)

# Use the two tom voices sparingly for fills; mute either line to taste.
p7 >> userSynth("JazzKitRackTom", P[0, 2], dur=4, sus=3.8,
                amp=0.11, pan=-0.12)
p8 >> userSynth("JazzKitFloorTom", P[0, 0], dur=4, sus=3.8,
                amp=0.1, pan=0.18)
