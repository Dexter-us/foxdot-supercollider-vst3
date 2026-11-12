# Example 233: compare the three PortedPlugins resonator wrappers.
# Their custom sliders are in the Players panel; start with one voice at a time.
Clock.bpm = 88
Scale.default = Scale.minor

p1 >> userSynth("PortedRongs", P[0, 3, 7, 10], dur=1, oct=4,
                amp=0.035, pan=-0.45)
p2 >> userSynth("PortedPhasorModal", P[0, 7, 3], dur=1.5, oct=4,
                amp=0.035, pan=0.45)
p3 >> userSynth("PortedResonator", P[0, 4, 7], dur=2, oct=4,
                amp=0.035, pan=0)

# Try higher octaves only after the voices behave at oct=4.
