tempo(112)
scale('minor', root=60)

# Rotate and reverse a scale-degree melody.
melody = rotate(reverse([0, 2, 4, 6, 4, 2]), 1)
play(melody, step=0.5, duration=0.38, degrees=True)

# Three hits distributed over an eight-step rhythm.
bass = euclid(3, 8, note=36, rotation=1)
play(bass, step=0.5, duration=0.22, offset=0.25)

# Repeat a short phrase, retaining its rests.
play(repeat([48, None, 55, None], 2), step=0.25, duration=0.18, offset=4)
