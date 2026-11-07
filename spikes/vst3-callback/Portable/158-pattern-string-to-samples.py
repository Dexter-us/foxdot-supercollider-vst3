# Example 158: turn finite sample-symbol Patterns into playable rhythms.
Clock.bpm = 96

straight = P["x", "-", "o", "-"].string()
offbeat = P["-", "o", "-", "o"].string()

# The first player uses a compact four-step Pattern string.
p1 >> play(straight, dur=0.5, sample=0, amp=0.18, pan=-0.2)

# A second Pattern creates a contrasting offbeat sample line.
p2 >> play(offbeat, dur=1, sample=1, amp=0.12, pan=0.25)
