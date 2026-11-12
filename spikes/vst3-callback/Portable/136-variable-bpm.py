# Example 136: one shared tempo curve drives drums and a melody.
# Listen for 8 beats at 90 BPM, 8 at 135 BPM, then 8 at 105 BPM.
# The 24-beat tempo cycle keeps running across both players' shorter loops.
Clock.bpm = var([90, 135, 105], [8, 8, 8])
Scale.default = Scale.minor
Root.default.set("C")

p1 >> play("x-o-", dur=0.5, sample=0, amp=0.32)
p2 >> pluck(P[0, 2, 4, 7, 4, 2], dur=0.5, sus=0.35,
            oct=4, amp=0.20, pan=0.15)

# For a continuous ramp instead, replace the Clock.bpm line above with:
# Clock.bpm = linvar([80, 150], [16, 16])
# This ramps up for 16 beats, then back down for 16 beats.
#
# Durations are BEATS, not seconds. sus is converted to seconds at the note's
# trigger tempo (as in FoxDot); already-playing WAVs are not time-stretched.
# Run starts the players and tempo curve together. This is the plugin's own
# clock, not DAW transport/tempo synchronization.
