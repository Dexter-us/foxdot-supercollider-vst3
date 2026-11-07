# Example 178: use the Players panel while this FoxDot-style pattern plays.
# Select p1 in the Players list to adjust Level, Pan, and Cutoff live.
# The slider values are written back into this line when you release a slider.
# Right-click the p1 >> line to change its instrument or choose a loaded effect.
Clock.bpm = 96

p1 >> pluck(P[0, 4, 7, 11], dur=0.5, sus=0.44,
            amp=0.28, pan=-0.2, cutoff=4200)

p2 >> triangle(P[0, 7, 4, 9], dur=1, sus=0.85,
               amp=0.12, pan=0.25, cutoff=7200)

# FX mix becomes available for a player after an effect SynthDef is loaded and
# attached to that player's effect= or effects=[...] chain.
# It adjusts the FIRST insert. Releasing a slider replaces a var/linvar for that
# parameter with the chosen scalar value; use Undo or edit the code to restore it.
