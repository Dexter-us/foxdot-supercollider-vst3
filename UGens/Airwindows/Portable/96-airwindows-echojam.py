# Example 96: Echojam as a persistent stereo insert on this player.
# Install the matching .scsyndef files and AirwindowsFX.scx from the UGen pack.
# vib selects one of Echojam's delay layouts; depth=feedback,
# lforate=speed, blur=Airwindows' internal mix. The outer fx_mix is the
# plugin-level dry/wet blend; values are normalized and clipped to 0..1.
Clock.bpm = 62
Scale.default.set("dorian")

p1 >> userSynth(
    "AirwindowsEchojam", P[0, 3, 5, 7, 5, 3], dur=4, sus=3.65,
    oct=4, amp=0.14, pan=0.12, atk=0.01, rel=2.5,
    vib=0.29, depth=0.32, lforate=0.48, fx_mix=0.68, blur=0.62
)
