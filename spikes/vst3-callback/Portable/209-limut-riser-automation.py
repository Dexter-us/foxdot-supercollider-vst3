# Example 209: Limut-inspired one-shot riser automation.
# Each control rises linearly over the requested beat span, then holds its target.
# Restart the transport from beat zero to hear the ramps from their starting values.
Clock.bpm = 96
Scale.default = Scale.minor

# A 16-beat rise in amplitude and filter cutoff.
p1 >> saw(P[0, 2, 4, 7], dur=0.5,
          amp=riser(16, 0.04, 0.28), cutoff=riser(16, 400, 9000), pan=-0.15)

# A shorter, quieter layer uses the same one-shot control shape.
p2 >> triangle(P[0, 3, 5, 7], dur=1,
               amp=riser(8, 0.02, 0.12), cutoff=riser(8, 700, 6500), pan=0.2)
