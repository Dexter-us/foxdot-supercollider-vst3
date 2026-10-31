# Example 93: Clock.swing adds FoxDot's half-beat clock nudge.
# A positive amount pulls events in every other half-beat slightly earlier.
Clock.bpm = 120
Clock.swing(0.04)

# At 120 BPM this is a 40 ms nudge (0.08 beats). Set swing to 0 to disable.
p1 >> triangle([0, 2, 4, 2], dur=0.5, sus=0.32, oct=4, amp=0.28)
p2 >> saw([7, 5, 7, 3], dur=0.5, sus=0.28, oct=3, amp=0.18)

# Clock.swing(0)
