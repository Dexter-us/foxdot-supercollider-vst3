# Example 232b: isolated PortedPlugins wrapper/octave check.
# Start with only p1. Change oct by one step after stopping; keep the low amp.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> userSynth("PortedBLOsc", P[0, 3, 7, 10], dur=0.5, oct=4,
                amp=0.035, pan=-0.2)

# Stop p1, then try one wrapper at a time:
# p1 >> userSynth("PortedAnalogFoldOsc", P[0, 7, 3], dur=1, oct=4,
#                 amp=0.03, pan=0.2)
# p1 >> userSynth("PortedResonator", P[0, 7], dur=1, oct=4,
#                 amp=0.035)
# If octave 4 is clean, change only oct=5, then oct=6; stop between tests.
