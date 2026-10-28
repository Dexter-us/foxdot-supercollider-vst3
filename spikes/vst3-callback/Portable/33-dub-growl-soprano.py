# 33: FoxDot dub, growl, and soprano oscillator patches.
Clock.bpm = 86
Scale.default.set("minor")

p1 >> dub(P[0, 3, 5, 7], dur=1, sus=0.9, oct=3,
          amp=0.22, pan=-0.25)
p2 >> growl(P[0, 2, 4, 6], dur=1, sus=0.8, oct=4,
            amp=0.12, pan=0.2)
p3 >> soprano(P[7, 5, 3, 2], dur=2, sus=1.8, oct=5,
              rate=2, amp=0.1, pan=0.35)
