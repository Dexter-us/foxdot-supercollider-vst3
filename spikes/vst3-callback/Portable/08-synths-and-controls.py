tempo(112)
scale('minor', root=48)

# Triangle melody on the left; saw bass with filtering and a short echo on the right.
p1 >> sequence([0, 2, 4, 7, 4, 2], step=0.5, duration=0.4, degrees=True,
               synth='triangle', amp=0.7, pan=-0.4, cutoff=5000)
p2 >> sequence([36, None, 43, None], step=0.5, duration=0.25,
               synth='saw', amp=0.5, pan=0.4, cutoff=1800,
               echo=0.25, delay=0.24)
