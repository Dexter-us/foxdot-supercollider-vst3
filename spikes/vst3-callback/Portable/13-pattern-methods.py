# FoxDot-style transformations on finite P patterns.
Clock.bpm = 104
Scale.default.set("dorian")

phrase = P[0, 2, 4, 7].palindrome()
rotated = P[0, 2, None, (4, 7), 5].rotate(1)
stuttered = P[0, None, 3, 5].stutter([1, 2])
repeated = P[0, None, 7].repeat(2)
reversed_line = P[0, 2, 4, 5].reverse()

p1 >> saw(phrase, dur=0.5, oct=5, sus=0.38, amp=0.32, pan=-0.4)
p2 >> pulse(rotated, dur=0.5, oct=4, sus=0.32, amp=0.24, pan=0.35)
p3 >> triangle(stuttered, dur=0.25, oct=5, sus=0.18, amp=0.2, pan=0.1)
p4 >> saw(repeated, dur=0.5, oct=3, sus=0.45, amp=0.18, pan=0.55)
p5 >> pulse(reversed_line, dur=0.5, oct=4, sus=0.28, amp=0.16, pan=-0.65)
