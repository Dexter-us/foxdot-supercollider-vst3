# Example 144: FoxDot-style callable map and dictionary submap.
Clock.bpm = 96
Scale.default = Scale.major

# map() calls the function recursively, including inside a chord.
notes = P[0, -2, PGroup(4, 7), 9]
p1 >> pluck(notes.map(lambda note: abs(note)), dur=0.5,
           oct=4, amp=0.2)

# Keep the existing dictionary map() form for older plugin scripts.
# Its legacy rule is that values with no key become zero.
legacy = P[0, 1, 2, 3].map({0: 0, 2: 5})

# submap() follows FoxDot: keys are replaced and unmatched values stay as-is.
drums = P["x", "-", "o", "x"].submap({"x": "o"})
p2 >> play(drums, dur=0.5, sample=0, amp=0.16, pan=0.25)
