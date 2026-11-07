# Example 202: custom .controls metadata exposes safely ranged live sliders.
# Compile SynthDefs/CustomTone.scd once with sclang, then restart the DAW.
Clock.bpm = 96
Scale.default = Scale.minor

# Select this player in the Players panel. The three sliders change this
# running SynthDef smoothly. The plugin keeps these values if you rerun the
# same player with the same SynthDef; selecting another SynthDef uses its own
# .controls defaults.
p1 >> userSynth("CustomTone", P[0, 2, 4, 7], dur=1, oct=4,
                amp=0.28, pan=-0.15)

# Try blend at 0.0 for sine-like tone, 1.0 for saw; sweep cutoff and resonance.
