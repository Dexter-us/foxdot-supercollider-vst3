# Example 228: choose bounded per-note reverb and delay inserts.
# Install the named player-effect SynthDefs in Portable/SynthDefs first.
# Each note owns its effect; tails decay naturally and have a hard time cap.
Clock.bpm = 92
Scale.default = Scale.minor

p1 >> pluck(
    P[0, 3, 7, 10, 7, 3, 5, 2],
    dur=0.5,
    sus=0.38,
    oct=4,
    amp=0.2,
    fx_event=P[
        "JPverb", "Greyhole", "LimutPingPong",
        "LimutShimmer", "LimutGatedVerb",
    ],
    fx_event_mix=P[0.18, 0.25, 0.2, 0.28, 0.22],
)

# Use None to leave selected hits dry; selector patterns repeat across notes.
p2 >> triangle(
    P[0, 3, 5, 7],
    dur=2,
    sus=1.7,
    oct=3,
    amp=0.11,
    fx_event=P[None, "JPverb"],
    fx_event_mix=P[0.0, 0.18],
)
