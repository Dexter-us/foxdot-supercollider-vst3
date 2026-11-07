# Example 213: smooth, band-limited triangle notes with a dry sine reference.
Clock.bpm = 100
Scale.default = Scale.minor

# Compare p1's triangle with p2's sine at the same pitch and note length.
# The triangle oscillator is band-limited to reduce high-note aliasing.
p1 >> triangle(P[0, 2, 4, 7], dur=0.5, sus=0.42,
               oct=4, amp=0.22, pan=-0.2)
p2 >> sine(P[0, 2, 4, 7], dur=0.5, sus=0.42,
           oct=4, amp=0.12, pan=0.2)
