# Example 25: FoxDot-style echo spacing and decay, both measured in beats.
# echomix is an optional plugin extension for wet level.
Clock.bpm = 120
p1 >> saw([0, 2, 4, 7, 9, 7, 4, 2], dur=0.5, echo=0.5, decay=1.5, echomix=0.35)
