# Example 184: click either pN assignment to select its player in the side panel.
# PShift stays discoverable; select p1 for pitch and p2 for Bits/Crush.
# Tip: move the editor caret into a pN line, adjust its sliders, then release to commit values.
Clock.bpm = 92

p1 >> pluck(P[0, 4, 7, 12], dur=0.5, sus=0.42,
            pshift=var([-12, 0, 7, 12], 2), amp=0.24, pan=-0.2)

p2 >> saw(P[0, 3, 5, 7], dur=0.5, sus=0.42,
          bits=var([10, 6, 8], 4), crush=var([4, 12, 6], 4),
          amp=0.18, pan=0.2)
