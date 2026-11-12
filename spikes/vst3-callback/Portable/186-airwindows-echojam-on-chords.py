# Example 186: Add Echojam to the flattened chord without an unsupported mix control.
# Install AirwindowsEchojam.scsyndef and its matching UGen pack first.
Clock.bpm = 96
Scale.default = Scale.minor

wide_chord = P[0, PGroup(2, 4), 7].flatten()

# Echojam uses its own controls; it does not have a generic fx_mix parameter.
p1 >> pluck(wide_chord, dur=2, sus=1.8, oct=4, amp=0.22, pan=-0.2,
            effect="AirwindowsEchojam",
            fx_vib=0.29, fx_depth=0.32, fx_lforate=0.48, fx_blur=0.62)
p2 >> triangle(P[2, PGroup(4, 7), 11].flatten(),
               dur=2, sus=1.8, oct=3, amp=0.12, pan=0.25)
