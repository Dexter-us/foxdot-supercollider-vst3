# Example 109: Player degree references keep one part following another.
Clock.bpm = 108
Scale.default.set("dorian")

# Keep this reference line in the editor. When you change p1's degrees and run
# the script again, p2 and p3 pick up the new notes without replacing their
# own synth, rhythm, octave, pan, or level.
p1 >> pluck(P[0, 2, 4, 6, 5, 3, 1, 4], dur=0.5, sus=0.38,
           oct=5, amp=0.24, pan=-0.25)
p2 >> bass(P[0, 0, 4, 0, 3, 0, 5, 0], dur=0.5, sus=0.45,
           oct=3, amp=0.32, pan=0.2)
p3 >> triangle(P[0, None, 2, None, 4, None, 3, None], dur=0.5,
               sus=0.3, oct=5, amp=0.12, pan=0.35)

# Either FoxDot-style form creates a live degree relationship.
p2.follow(p1)
p3.degree = p1.degree

# Try changing p1's degree pattern above, then run again: both followers
# update; their bass/triangle timbre and separate controls stay in place.
# p1 >> pluck(P[7, 5, 3, 1, 0, 2, 4, 6], dur=0.5, sus=0.38,
#            oct=5, amp=0.24, pan=-0.25)
