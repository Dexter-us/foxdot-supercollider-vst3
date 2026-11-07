# Example 127: trim() keeps the front; ltrim() keeps the tail.
Clock.bpm = 96
Scale.default = Scale.minor
Root.default.set("D")

phrase = P[0, 2, 4, 5, 7, 9, 11, 12]

# Play the first four steps as one part.
p1 >> pluck(phrase.trim(4), dur=0.5, oct=4, amp=0.18)

# Play the final four as a response, entering in a lower register.
p2 >> triangle(phrase.ltrim(4), dur=0.5, oct=3, amp=0.13, pan=-0.3)

p3 >> play("x---o---", dur=0.5, amp=0.11)
