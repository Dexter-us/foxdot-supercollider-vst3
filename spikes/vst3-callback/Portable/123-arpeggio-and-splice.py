# Example 123: FoxDot-style arpeggiation and cyclic pattern interleaving.
Clock.bpm = 100
Scale.default = Scale.major
Root.default.set("C")

# arp() applies each interval to each phrase note in turn.
p1 >> pluck(P[0, 2, 4, 7].arp(P[0, 2, 4]), dur=0.25,
            oct=4, amp=0.18)

# splice() interleaves patterns, wrapping shorter patterns cyclically.
p2 >> triangle(P[0, 4, 7, 4].splice(P[2, 5], P[0, 2, 4]),
               dur=0.5, oct=3, amp=0.13, pan=-0.3)

p3 >> play("x---x---", dur=0.5, amp=0.12)
