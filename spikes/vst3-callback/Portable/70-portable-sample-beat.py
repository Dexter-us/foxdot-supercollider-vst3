# Example 70: the included tiny diagnostic kick, hat and snare WAVs.
# "-" is FoxDot's closed-hat symbol. A space is a rest.
Clock.bpm = 100
p2.stop()
p3.stop()
p4.stop()
p5.stop()
p6.stop()
p7.stop()
p8.stop()
p1 >> play("x-o-", dur=0.5, amp=0.65, pan=0)
# Try "x o " to hear the kick and snare with rests instead of hats.
