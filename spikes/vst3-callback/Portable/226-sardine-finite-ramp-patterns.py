# Example 226: finite Sardine-style ramps for notes and durations.
# ramp() includes the endpoint when the step lands on it and caps output at 256.
Clock.bpm = 104
Scale.default = Scale.minor
Root.default.set("C")

ascending_degrees = ramp(0, 15, 1)
note_durations = ramp(0.25, 0.5, 0.05)

p1 >> pluck(ascending_degrees, dur=0.25, sus=0.22,
           oct=4, amp=0.16, pan=-0.2)
p2 >> triangle(P[0, 2, 4, 7, 5, 3], dur=note_durations, sus=0.8,
               oct=3, amp=0.12, pan=0.25)
