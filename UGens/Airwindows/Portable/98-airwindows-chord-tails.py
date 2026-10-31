# Example 98: polyphonic notes share one persistent TapeDelay2 insert.
# Install the UGen pack's matching SynthDefs and AirwindowsFX.scx first.
# The chord changes while the stereo delay tail continues between events.
Clock.bpm = 56
Scale.default.set("minor")
Root.default.set("C")

p1 >> userSynth(
    "AirwindowsTapeDelay2",
    P[[0, 3, 7], [2, 5, 9], [0, 5, 8], [3, 7, 10]],
    dur=4, sus=3.7, oct=3, amp=0.12, pan=-0.18, atk=0.015, rel=2.0,
    vib=0.94, depth=0.24, lforate=0.72, lfowidth=0.28, rq=0.12, blur=0.48
)

p2 >> userSynth(
    "AirwindowsEchojam", P[0, 2, 4, 7, 4, 2], dur=2, sus=1.8,
    oct=4, amp=0.07, pan=0.22, atk=0.015, rel=2.5,
    vib=0.29, depth=0.24, lforate=0.48, blur=0.55
)
