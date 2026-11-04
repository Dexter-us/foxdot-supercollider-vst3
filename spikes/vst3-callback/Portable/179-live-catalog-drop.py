# Example 179: a small script to pair with the Synths/Effects catalog.
# Right-click an empty p3 >> line or drag a sidebar item onto it.
# Catalog edits create code; click Run to apply them.
Clock.bpm = 96
Scale.default.set("minor")

p1 >> pluck(P[0, 2, 4, 7, 4, 2], dur=0.5, sus=0.38,
            amp=0.24, pan=-0.2, lpf=2400)
p2 >> triangle(P[0, 4, 7, 11], dur=2, sus=1.7,
               amp=0.16, pan=0.2, echo=0.2)

# To try the blank-line workflow, temporarily delete the sine(...) call below,
# leave `p3 >>`, then right-click the line or drag an item onto it.
p3 >> sine(P[0, 2, 4, 7], dur=1, amp=0.1)
