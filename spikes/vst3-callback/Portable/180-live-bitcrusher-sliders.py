# Example 180: select p2 in the Players panel to expose the live Bits and
# Crush sliders. Moving either slider changes the running sound immediately;
# releasing it also writes the chosen value into this player call.
Clock.bpm = 92

p1 >> pluck(P[0, 3, 5, 7], dur=0.5, sus=0.42, amp=0.3, pan=-0.25)

# Bits is the quantization depth (0 bypasses the bit-depth stage).
# Crush is the sample-rate reduction factor (1 is full-rate).
p2 >> saw(P[0, 3, 5, 7], dur=0.5, sus=0.42,
          bits=var([10, 6, 8], 4), crush=var([4, 12, 6], 4),
          amp=0.2, pan=0.25)
