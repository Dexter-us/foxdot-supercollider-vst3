# 69: A small arrangement, not a single four-note synth test.
# p3 is a quiet sine reference: if ONLY it plays, star/karp are still failing.
# Use version 0.4.47 or later for the new-bank first-render fix.
Clock.bpm = 100
Scale.default = Scale.minor
Root.default.set("C")

p4.stop()
p5.stop()
p6.stop()
p7.stop()
p8.stop()

# Concatenate two phrases, rotate the result, then transpose up two degrees.
phrase = (P[0, 2, 4, 7] | P[9, 7, 4, 2]).rotate(2) + 2
p1 >> star(phrase, dur=P[0.5, 0.5, 1, 1], sus=0.45,
           oct=5, amp=0.4, pan=-0.25, reverb="none")

# A slower, reversed response on a separate player.
response = P[0, 4, 7, 4].reverse()
p2 >> karp(response, dur=2, sus=1, oct=4,
           amp=0.65, pan=0.25, reverb="none")

# The familiar oscillator is deliberately quieter than the two new synths.
p3 >> sine(P[0, 4, 7, 4], dur=2, sus=1.5, oct=4,
           amp=0.08, pan=0, reverb="none")

# After testing, comment out p3's assignment above to remove the reference.
