# 36: A short plucked melody with twang, donk, and squish accents.
Clock.bpm = 104
Scale.default.set("minor_pentatonic")

p1 >> pluck(P[0, 2, 4, 7, 4, 2, 1, None], dur=0.5, sus=0.44, oct=5,
            atk=0.01, rel=0.12, amp=0.18, pan=-0.2)
p2 >> twang(P[0, None, 3, None, 5, None, 3, None], dur=0.5, sus=0.44,
            oct=4, rate=1, atk=0.01, rel=0.14, amp=0.12, pan=0.18)
p3 >> donk(P[0, None, None, 5], dur=1, sus=0.8, oct=3,
           atk=0.01, rel=0.18, amp=0.14, pan=0.0)
p4 >> squish(P[0, None, 2, None], dur=1, sus=0.8, oct=4, rate=1,
             atk=0.01, rel=0.18, amp=0.035, pan=0.3)
