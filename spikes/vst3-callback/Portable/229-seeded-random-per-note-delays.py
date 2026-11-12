# Example 229: seeded per-note Airwindows delay choice and wet amount.
# Requires the Airwindows UGen pack's scsyndefs and AirwindowsFX.scx.
Clock.bpm = 96
Scale.default = Scale.minor

# Each note chooses TapeDelay2 or Echojam from the installed-effect whitelist.
# Both patterns are seeded, so this 16-note phrase repeats reproducibly.
# fx_event_mix is the per-note dry/wet amount, independent of the effect choice.
p1 >> pluck(
    P[0, 3, 5, 7, 3, 5, 8, 7],
    dur=0.5,
    sus=0.46,
    oct=4,
    fx_event=PRand(
        ["AirwindowsTapeDelay2", "AirwindowsEchojam"],
        length=16,
        seed=21,
    ),
    fx_event_mix=PWhite(0.15, 0.65, length=16, seed=7),
    amp=0.24,
)

# A dry reference layer makes the random delay changes easier to hear.
p2 >> triangle(P[0, 7, 12, 7], dur=2, sus=1.8, oct=3, amp=0.10, pan=0.25)
