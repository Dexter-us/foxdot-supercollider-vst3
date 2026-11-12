# Example 190: two bounded Renardo pattern generators.
Clock.clear()
Clock.bpm = 108
Scale.default = Scale.minor

# PZ12 follows Renardo's documented experimental, deterministic two-token
# deficit ordering. The weights shape the order of these scale degrees.
weighted_notes = PZ12([0, 3], [1.0, 0.45], length=16)
p1 >> pluck(weighted_notes, dur=0.25, oct=4, amp=0.20, pan=-0.2)

# PFibMod supplies the Fibonacci sequence. Modulo keeps the notes inside one
# octave while the player repeats the bounded phrase.
fibonacci_notes = PFibMod(length=16) % 12
p2 >> triangle(fibonacci_notes, dur=0.25, oct=3, amp=0.10, pan=0.2)
p3 >> play("x-o-", dur=0.5, sample=0, amp=0.10)
