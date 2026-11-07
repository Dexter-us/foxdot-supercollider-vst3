# Example 155: live aliases share edits; a copy keeps the earlier phrase.
Clock.bpm = 100
Scale.default = Scale.minor

motif = P[0, 3, 5]
shared_motif = motif
motif.append(7)
snapshot = motif.copy()
motif.extend(P[10, 12])

# p1 and p2 use the same six-note phrase through two references.
p1 >> pluck(motif, dur=0.5, oct=4, amp=0.18, pan=-0.2)
p2 >> triangle(shared_motif, dur=0.5, oct=3, amp=0.14, pan=0.2)

# snapshot was copied before extend(), so it stays a four-note phrase.
p3 >> saw(snapshot, dur=0.5, oct=2, amp=0.12)
