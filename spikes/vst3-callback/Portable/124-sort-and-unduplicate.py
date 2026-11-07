# Example 124: sort() orders a phrase; unduplicate() removes adjacent repeats.
Clock.bpm = 96
Scale.default = Scale.minor
Root.default.set("D")

# Sorting changes pitch order while keeping repeated notes.
p1 >> pluck(P[7, 0, 3, 3, 10, 5].sort(), dur=0.5, oct=4, amp=0.18)

# unduplicate() keeps the order and removes only neighboring repeats.
# Non-adjacent repeats remain, so this stays a phrase rather than a set.
p2 >> triangle(P[0, 0, 3, 3, 7, 3, 3, 0].unduplicate(),
               dur=0.5, oct=3, amp=0.14, pan=-0.25)

p3 >> play("x---o---", dur=0.5, amp=0.12)
