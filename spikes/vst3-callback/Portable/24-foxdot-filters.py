# FoxDot-style resonant filters with band-pass modulation and envelope-driven swell.
Clock.bpm = 100
Scale.default.set("minor")

# Low-pass: lpf is cutoff frequency; lpr is reciprocal-Q resonance.
p1 >> saw([0, 2, 4, 6, 4, 2, 0, None], dur=0.5,
          lpf=linvar([300, 1800], [4, 4]), lpr=var([0.35, 0.7], [4, 4]),
          amp=0.18)

# High-pass: removes low frequencies while preserving the upper harmonics.
p2 >> triangle([7, 5, 4, 2, 0, 2, 4, None], dur=0.5,
               hpf=var([150, 900], [4, 4]), hpr=0.8, swell=1.15,
               pan=-0.35, amp=0.14)

# Band-pass: keeps a resonant band and sweeps its center frequency.
p3 >> pulse([0, 4, 7, 4, 2, 5, 7, None], dur=0.5,
            bpf=linvar([350, 1200], [4, 4]), bpr=0.5, bpnoise=0.18,
            pan=0.35, amp=0.12)
