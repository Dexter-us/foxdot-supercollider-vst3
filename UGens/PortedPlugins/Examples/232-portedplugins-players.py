# Example 232: categorized PortedPlugins UGen-backed Player voices.
# Install the matching PortedPlugins .scx modules and these SynthDefs first.
Clock.bpm = 104
Scale.default = Scale.minor

# Oscillator family: band-limited, wavefolded, variable-slope and formant tones.
# Use the VST Players sliders to audition each wrapper's custom controls.
p1 >> userSynth("PortedBLOsc", P[0, 3, 7, 10], dur=0.5, oct=4,
                amp=0.10, pan=-0.55)
p2 >> userSynth("PortedAnalogFoldOsc", P[0, 7, 3], dur=1, oct=3,
                amp=0.08, pan=0.45)
p3 >> userSynth("PortedNeoVarSawOsc", P[0, 3, 7, 12], dur=0.5, oct=4,
                amp=0.08, pan=0.2)
p4 >> userSynth("PortedVosimOsc", P[0, 5, 7], dur=1, oct=4,
                amp=0.06)
p5 >> userSynth("PortedZOsc", P[0, 7, 10], dur=1, oct=3,
                amp=0.06, pan=-0.2)

# Physical-model string and two analog drum voices from the same UGen set.
p6 >> userSynth("PortedStringVoice", P[0, 7, 3, 10], dur=1, oct=4,
                amp=0.08, pan=0.55)
p7 >> userSynth("PortedAnalogBassDrum", P[0, None, 0, None], dur=1, oct=2,
                amp=0.20)
p8 >> userSynth("PortedAnalogSnareDrum", P[None, 0, None, 0], dur=1, oct=3,
                amp=0.12)

# p1.stop()  # Use player.stop() to silence an individual layer.
