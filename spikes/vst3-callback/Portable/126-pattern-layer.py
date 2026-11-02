# Example 126: layer() zips a phrase with a transformed copy as chord notes.
Clock.bpm = 92
Scale.default = Scale.minor
Root.default.set("A")

# Each hit combines one note with its counterpart from the reversed phrase.
p1 >> pluck(P[0, 2, 4, 7].layer("reverse"),
            dur=0.75, oct=4, amp=0.18)

# Extra arguments are passed to the allowed transform method.
p2 >> triangle(P[0, 1, 3, 2].layer("rotate", 1),
               dur=0.75, oct=3, amp=0.12, pan=-0.3)

p3 >> play("x---o---", dur=0.75, amp=0.12)
