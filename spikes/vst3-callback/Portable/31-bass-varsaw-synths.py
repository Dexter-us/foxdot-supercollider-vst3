# 31: Rounded bass and a softer variable-width saw; the longer fades avoid hard edges.
# No sample files or external synth server are used.
Clock.bpm = 84
Scale.default.set("minor")

p1 >> bass(P[0, 2, 4, 6], dur=0.5, sus=0.48, oct=3,
          rate=5, atk=0.05, rel=0.22, amp=0.18, pan=-0.18)
p2 >> varsaw(P[0, 2, 4, 6], dur=1, sus=0.86, oct=5,
             rate=0.32, atk=0.04, rel=0.22, amp=0.07, pan=0.22)
