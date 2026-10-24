tempo(120)
scale('minor', root=48)

# Start two parts independently: a scale-degree line and a bass rhythm.
p1 >> sequence([0, 2, 4, 6, 4, 2], step=0.5, duration=0.4, degrees=True)
p2 >> sequence([36, None, 36, None], step=0.5, duration=0.2)

# Change only p1 by editing and re-running its assignment. p2 keeps looping.
# Stop only the bass with: p2.stop()
