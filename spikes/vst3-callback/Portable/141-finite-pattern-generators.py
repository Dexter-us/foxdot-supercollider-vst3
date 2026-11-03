# Example 141: Bounded FoxDot sequence generators.
Clock.bpm = 96
Scale.default = Scale.major

# PRange follows Python/FoxDot range semantics; PSq returns 1**2, 2**2, ...
p1 >> pluck(PRange(0, 8, 2), dur=0.5, oct=4, amp=0.28)
p2 >> triangle(PSq(1, 2, 4), dur=PStep(4, 0.5, 0.25), oct=3, amp=0.16)

# PEuclid gives a 1/0 mask; numeric P arithmetic turns it into a two-note motif.
p3 >> sine(PEuclid(3, 8) * 7, dur=0.25, oct=3, amp=0.16)
# PEuclid2 maps pulses to x and the zeroes to space-rests.
p4 >> play(PEuclid2(3, 8, " ", "x"), dur=0.25, sample=0, amp=0.24)
