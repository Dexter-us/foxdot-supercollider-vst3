# Example 146: zipx() keeps nested chord pairs instead of flattening them.
Clock.bpm = 96
Scale.default = Scale.minor

top = P[0, PGroup(2, 4), 5]
bottom = P[-7, -5, PGroup(-3, 0)]
pairs = top.zipx(bottom)
p1 >> pluck(pairs, dur=1, sus=0.85, oct=4, amp=0.22)

# Sample-symbol pairs remain nested in the pattern and play as layered hits.
drum_a = P["x", "o", "x"]
drum_b = P["-", "x", "-"]
p2 >> play(drum_a.zipx(drum_b), dur=1, sample=0, amp=0.12, pan=0.25)
