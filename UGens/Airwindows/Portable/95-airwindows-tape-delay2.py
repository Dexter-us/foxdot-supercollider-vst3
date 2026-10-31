# Example 95: Airwindows TapeDelay2, wrapped as a native SC UGen.
# The .scsyndef and AirwindowsFX.scx must be installed from the UGen pack.
# vib=delay time, depth=feedback, lforate=tone, lfowidth=resonance,
# rq=flutter, blur=dry/wet; normalized values are clipped to 0..1.
Clock.bpm = 58
Scale.default.set("minor")

p1 >> userSynth(
    "AirwindowsTapeDelay2", P[0, 2, 4, 7, 4, 2], dur=4, sus=3.75,
    oct=3, amp=0.24, pan=-0.15, atk=0.01, rel=2.0,
    vib=0.94, depth=0.28, lforate=0.72, lfowidth=0.28, rq=0.12, blur=0.5
)
