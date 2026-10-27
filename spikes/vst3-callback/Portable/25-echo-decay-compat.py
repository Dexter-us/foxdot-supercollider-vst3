# FoxDot-style echo decay syntax compatibility check.
# `decay` is measured in beats and controls the repeat tail.
# The plugin echo mix/time behavior is still a documented subset.
Clock.bpm = 120
p1 >> saw([0, 2, 4, 7, 9, 7, 4, 2], dur=0.5, echo=0.35, decay=1.5)
