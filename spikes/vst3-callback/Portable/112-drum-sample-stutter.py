# Example 112: scheduled stutter repeats matching drum hits, not rests.
Clock.bpm = 105
p2.stop()
p3.stop()
p4.stop()
p5.stop()
p6.stop()
p7.stop()
p8.stop()

# x is kick, - is closed hat, o is snare; this four-hit bar repeats.
p1 >> play("x-o-", dur=0.5, sample=0, amp=0.42, pan=0)

# Every beat, stutter the hit that starts on that beat into three even hits.
# The beat-1 snare and the next bar's beat-0 kick are repeated.
p1.every(1, "stutter", 3)
