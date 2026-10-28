# 34: Noise and Gendy1 patches. Crunch is deliberately a noise texture.
Clock.bpm = 72
Scale.default.set("minor_pentatonic")

p1 >> crunch(P[0, None, 4, None], dur=0.5, sus=0.1, oct=4,
            amp=0.03, pan=-0.25)
p2 >> lazer(P[0, 3, 5, 7], dur=1, sus=0.8, oct=4,
            rate=2.5, amp=0.08, pan=0.15)
p3 >> rave(P[0, 2, 4, 6], dur=2, sus=1.7, oct=3,
           rate=2, amp=0.08, pan=0.35)
