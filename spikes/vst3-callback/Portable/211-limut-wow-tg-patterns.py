# Example 211: seeded slow wow modulation and a fast alternating trigger gate.
Clock.bpm = 96
Scale.default = Scale.minor

# wow() is bounded seeded smooth noise; seed=0 is repeatable by default.
# Center it above zero for amplitude; use the signed default for pan or pitch controls.
p1 >> saw([0, 2, 4, 7], dur=0.5,
          amp=wow(amount=0.18, center=0.32, period=4, seed=7),
          pan=wow(amount=0.65, period=2, seed=9),
          cutoff=wow(amount=1800, center=3600, period=4, seed=13))

# tg() alternates high/low every 1/8 beat; customize the gate depth if needed.
p2 >> play("x-o-", dur=0.25, sample=0,
           amp=tg(0.125, high=0.7, low=0.08), pan=0.2)
