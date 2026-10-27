# Example 22: native FoxDot beat-based echo plus plugin-only mix and delay-time controls.
# delay schedules note starts; delaytime overrides echo's beat-derived delay in seconds.
Clock.bpm = 100
Scale.default.set("minor")

p1 >> saw(P[0, 2, 4, 6, 4, 2, 0, 2], dur=0.5, echo=linvar([0.25, 0.5], [4, 4]), decay=2.0, echomix=linvar([0.15, 0.45], [4, 4]), delaytime=linvar([0.12, 0.42], [4, 4]), delay=var([0.0, 0.25], [2, 2]), atk=var([0.005, 0.02], [4, 4]), rel=linvar([0.04, 0.18], [4, 4]), amp=0.28)
p2 >> triangle(P[0, 3, 5, 3], dur=1.0, echo=var([0.25, 0.35], [2, 2]), decay=3.0, echomix=0.3, delaytime=var([0.18, 0.32], [2, 2]), delay=var([0.0, 0.25], [2, 2]), feedback=var([0.25, 0.45], [2, 2]), atk=linvar([0.01, 0.05], [2, 2]), rel=var([0.03, 0.12], [2, 2]), amp=0.18, pan=0.35)
