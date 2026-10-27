# 31: Embedded FoxDot-style bass and variable-width saw SynthDefs.
# No sample files or external synth server are used.
Clock.bpm = 92
Scale.default.set("minor")

p1 >> bass(P[0, 2, 4, 6], dur=0.5, sus=0.44, oct=3,
          rate=8, amp=0.24, pan=-0.18)
p2 >> varsaw(P[0, 2, 4, 6], dur=1, sus=0.86, oct=5,
             rate=0.42, amp=0.09, pan=0.22)
