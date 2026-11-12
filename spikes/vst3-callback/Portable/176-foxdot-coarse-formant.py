# Example 176: FoxDot-style coarse gating and Formlet formant color.
# coarse=0 leaves a note open; higher values add more pulse cycles per sustain.
# formant accepts 0..7; -1 bypasses the Formlet filter. var/linvar values
# are selected per note so edits stay bounded and live.
Clock.bpm = 100
Scale.default = Scale.minor
Root.default.set("C")

p1 >> pluck(
    [0, 3, 5, 7, 5, 3, 2, 0],
    dur=0.5,
    sus=0.38,
    coarse=var([0, 4, 8, 4], 2),
    amp=0.24,
    pan=-0.25,
)

p2 >> saw(
    [0, 2, 4, 7, 5, 3, 2, 4],
    dur=1,
    sus=0.82,
    formant=var([0, 2, 4, 6], 2),
    amp=0.12,
    pan=0.28,
)

# A sample voice can use the same coarse gate while the pitched line carries
# the formant sweep.
p3 >> play(
    "x---o---",
    sample=0,
    dur=0.5,
    sus=0.45,
    coarse=2,
    amp=0.16,
)
