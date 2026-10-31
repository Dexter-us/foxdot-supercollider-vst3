# Example 96: Airwindows Echojam, wrapped as a native SC UGen.
# The .scsyndef and AirwindowsFX.scx must be installed from the UGen pack.
# vib selects one of Echojam's delay layouts; depth=feedback,
# lforate=speed, blur=dry/wet; normalized values are clipped to 0..1.
Clock.bpm = 62
Scale.default.set("dorian")

p1 >> userSynth(
    "AirwindowsEchojam", P[0, 3, 5, 7, 5, 3], dur=4, sus=3.65,
    oct=4, amp=0.18, pan=0.12, atk=0.01, rel=2.5,
    vib=0.29, depth=0.32, lforate=0.48, blur=0.62
)
