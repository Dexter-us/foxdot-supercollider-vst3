# Example 165: FoxDot-style chop gates each note; spin moves it across stereo.
Clock.bpm = 120

# Chop divides each note's sustain into repeated half-on, half-quiet pulses.
p1 >> pluck([0, 2, 4, 7], dur=2, sus=2,
            chop=var([4, 8], [4, 4]), amp=0.24)

# Spin follows FoxDot's stereo LFO; its rate is spin/2 Hz.
p2 >> pads([0, 3, 5, 7], dur=4, sus=2,
           spin=linvar([0, 6], [4, 4]), pan=-0.15, amp=0.16)

# Both effects also work on sample players.
p3 >> play("x---", dur=1, sus=1, chop=8, spin=2, amp=0.2, pan=0.2)
