# Example 140: PZip/PZip2 combine numeric chord groups into playable chords.
Clock.bpm = 92
Scale.default = Scale.minor

# Each zipped event joins the pitches from both chord patterns. Chord rests
# remain rests; the combined chord is capped at eight notes.
p1 >> pluck(
    PZip(
        P[PGroup(0, 4, 7), PGroup(2, 5, 9), PGroup(4, 7, 11)],
        P[PGroup(12, 16), PGroup(11, 14), PGroup(9, 12)]),
    dur=2, sus=1.6, oct=4, amp=0.24)

# PZip2 also preserves its rule callback, then joins the selected chord groups.
p2 >> triangle(
    PZip2(
        P[PGroup(0, 3), PGroup(2, 5)],
        P[PGroup(7, 11), PGroup(9, 12)],
        rule=lambda left, right: left != right),
    dur=2, sus=1.4, oct=3, amp=0.12, pan=0.25)
