# Example 97: run both persistent Airwindows inserts at the same time.
# Install the UGen pack's SynthDefs and AirwindowsFX.scx first.
# Each player has its own stereo delay state; notes and chords share that state.
Clock.bpm = 62
Scale.default.set("dorian")

p1 >> userSynth(
    "AirwindowsTapeDelay2", P[0, 3, 5, 7], dur=4, sus=3.7,
    oct=3, amp=0.16, pan=-0.22, atk=0.01, rel=2.0,
    vib=0.94, depth=0.28, lforate=0.72, lfowidth=0.28, rq=0.12, blur=0.5
)

p2 >> userSynth(
    "AirwindowsEchojam", P[0, 2, 4, 7], dur=4, sus=3.7,
    oct=4, amp=0.1, pan=0.24, atk=0.01, rel=2.5,
    vib=0.29, depth=0.32, lforate=0.48, blur=0.62
)
