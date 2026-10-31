# Example 88: the pattern transforms also accept numeric note patterns.
# Scale degrees are transformed before the synth plays them.
Clock.bpm = 96
Scale.default = Scale.minor

alternate = PAlt(P[0, 2, 4, 7], P[7, 5, 3, 2])
expanded = PStretch(alternate, 12)
stuttered = PStutter(P[0, 2, 4, 5], P[1, 2])
joined = PJoin([P[0, 4, 7], P[2, 5, 9]])
shuffled = PShuf(P[0, 2, 4, 7, 9, 7])

p1 >> saw(expanded, dur=0.5, sus=0.42, oct=4, amp=0.14)
p2 >> triangle(stuttered, dur=0.25, sus=0.20, oct=5, amp=0.10)
p3 >> sine(PJoin([joined, shuffled]), dur=1, sus=0.82, oct=3, amp=0.09)
