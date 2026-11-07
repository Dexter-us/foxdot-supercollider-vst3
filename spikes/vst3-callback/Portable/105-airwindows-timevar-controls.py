# Example 105: FoxDot-style var()/linvar() motion on persistent Airwindows inserts.
# Install both Airwindows SynthDefs and the matching UGen pack first.
Clock.bpm = 100

# TapeDelay2's step controls glide briefly at each var() change; linvar() sweeps
# continuously with the player's beat clock. Every fx_* value stays in 0..1.
p1 >> play("x---x---", dur=0.5, sample=0,
          effect="AirwindowsTapeDelay2",
          fx_vib=linvar([0.18, 0.72], [4, 4]),
          fx_depth=var([0.22, 0.68], [2, 2]),
          fx_lforate=linvar([0.12, 0.58], [4, 4]),
          fx_lfowidth=var([0.12, 0.48], [4, 4]),
          fx_rq=linvar([0.14, 0.52], [4, 4]),
          fx_blur=var([0.22, 0.70], [4, 4]),
          amp=0.22)

# Echojam can move independently; it exposes vib, depth, lforate and blur.
p2 >> play("--o---o-", dur=0.5, sample=0,
          effect="AirwindowsEchojam",
          fx_vib=var([0.20, 0.66], [3, 3]),
          fx_depth=linvar([0.18, 0.62], [3, 3]),
          fx_lforate=var([0.16, 0.54], [3, 3]),
          fx_blur=linvar([0.24, 0.74], [3, 3]),
          amp=0.16, pan=0.25)

# A dry reference makes it easier to hear what the two inserts are changing.
p3 >> play("----x---", dur=0.5, sample=0, amp=0.10, pan=-0.25)
