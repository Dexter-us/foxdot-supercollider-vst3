# Example 30: mute one player layer without stopping its pattern.
Clock.bpm = 92
Scale.default = Scale.major
Root.default.set("C")

# Run both parts as written. Then remove only the # before p2.mute() and Run;
# p2 keeps its pattern and MIDI notes resume after p2.unmute().
p1 >> pulse(P[0, None, 0, None, 0, None, 0, None], dur=0.5, oct=3, amp=0.28)
p2 >> triangle(P[I, IV, V7, I], dur=2, sus=1.7, oct=4, strum=0.035, amp=0.2)

# Try each command on its own after the two patterns are running:
#p2.mute()
#p2.unmute()
