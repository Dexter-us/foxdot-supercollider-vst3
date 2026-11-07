# Example 210: Limut-style stepped drop gates.
# drop(30, 2) holds the high level for 30 beats, drops for 2, then repeats.
# Supply custom high/low values to match the parameter you are automating.
Clock.bpm = 96
Scale.default = Scale.minor

p1 >> saw(P[0, 2, 4, 7], dur=0.5, oct=3,
         amp=drop(30, 2, high=0.3, low=0.02),
         cutoff=drop(30, 2, high=8000, low=500), pan=-0.15)

p2 >> play("x-o-", dur=1, sample=0,
           amp=drop(30, 2, high=0.24, low=0.01), pan=0.2)
