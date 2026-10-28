# Example 43: FoxDot viola with delayed vibrato and a soft sine counterline.
Clock.bpm = 92
Scale.default = Scale.minor

p1 >> viola(P[0, 2, 4, 6], dur=2, sus=1.6, oct=4,
            vib=6, amp=0.35, pan=-0.2)
p2 >> sine(P[0, 4, 3, 5], dur=2, sus=1.6, oct=3,
           amp=0.18, pan=0.2)
