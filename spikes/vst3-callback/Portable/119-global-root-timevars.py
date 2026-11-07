# Example 119: move the session root with FoxDot-style bounded timevars.
Clock.bpm = 96
Scale.default = Scale.major

# Both players follow C for two beats, then G for two beats.
Root.default.set(var([0, 7], [2, 2]))
p1 >> pluck(P[0, 2, 4, 6], dur=1, oct=5, sus=0.72,
            amp=0.22, pan=-0.2)
p2 >> triangle(P[4, 2, 0, 2], dur=1, oct=4, sus=0.78,
               amp=0.14, pan=0.25)

# A fresh root assignment glides in semitones from C toward E.
Root.default.set(linvar([0, 4], [4, 4]))
p3 >> sine(P[0, 2, 4, 6], dur=1, oct=5, sus=0.7,
           amp=0.1, pan=0.4)
p4 >> pluck(P[4, 2, 0, 2], dur=1, oct=4, sus=0.75,
            amp=0.1, pan=-0.4)
