# 42: First six self-contained FoxDot synth patches.
Clock.bpm = 92
Scale.default.set("minor")
Root.default.set("C")

p1 >> scatter(P[0, 3, 7, 10], dur=1, sus=0.8, rate=2, oct=4,
             amp=0.22, pan=-0.7, reverb="none")
p2 >> orient(P[0, 4, 7, 11], dur=1, sus=0.8, oct=3,
             amp=0.3, pan=-0.4, reverb="none")
p3 >> zap(P[0, 3, 5, 7], dur=0.5, sus=0.4, oct=4,
          amp=0.28, pan=-0.1, reverb="none")
p4 >> fuzz(P[0, 7, 3, 10], dur=2, sus=1.6, oct=2,
           amp=0.2, pan=0.2, reverb="none")
p5 >> bug(P[0, 4, 7, 5], dur=1, sus=0.8, oct=3,
          amp=0.3, pan=0.5, reverb="none")
p6 >> ripple(P[0, 3, 5, 2], dur=1.5, sus=1.2, rate=0.5, oct=3,
             amp=0.3, pan=0.8, reverb="none")
