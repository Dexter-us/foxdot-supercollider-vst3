# 37: FoxDot-style bpnoise animates the band-pass center and resonance.
# swell adds a short envelope-shaped high-pass sweep to the second player.
Clock.bpm = 96
Scale.default.set("minor")

p1 >> saw(P[0, 2, 4, 7, 4, 2, 0, None], dur=0.5, sus=0.42, oct=4,
          bpf=linvar([380, 1100], [4, 4]), bpr=0.42, bpnoise=0.18,
          amp=0.13, pan=-0.2)

p2 >> triangle(P[7, 5, 4, 2, 0, 2, 4, 5], dur=0.5, sus=0.40, oct=5,
               hpf=var([180, 520], [4, 4]), hpr=0.72, swell=1.2,
               amp=0.09, pan=0.24)

p3 >> pulse(P[0, None, 3, None, 5, None, 3, None], dur=0.5, sus=0.34, oct=5,
            bpf=linvar([500, 1500], [2, 2]), bpr=0.55,
            bpnoise=var([0.22, 0.48], [4, 4]),
            swell=linvar([0.2, 1.1], [4, 4]), amp=0.045, pan=0.38)
