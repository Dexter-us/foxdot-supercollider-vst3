# Example 99: a sparse kick/snare-like loop leaves space for Echojam repeats.
# This is synthesized (no drum samples required). The short dry noise ticks
# provide a timing reference while the pitched hits run through persistent Echojam.
# Raise depth for longer feedback; blur raises the wet delay in the mix.
Clock.bpm = 96
Scale.default.set("minor")

# Low kick-like hit on beat 1; higher snare-like hit on beat 3.
# Rests and short sustain make the repeats easier to hear between hits.
p1 >> userSynth(
    "AirwindowsEchojam", P[0, None, None, None, 7, None, None, None],
    dur=0.5, sus=0.10, oct=2, amp=0.20, pan=-0.12, atk=0.005, rel=0.16,
    vib=0.29, depth=0.62, lforate=0.48, blur=0.82
)

# Dry synthetic hi-hat ticks, deliberately not sent through the delay.
p2 >> noise(
    P[0, None], dur=0.5, sus=0.04, oct=6, amp=0.035, pan=0.16,
    atk=0.003, rel=0.025
)
