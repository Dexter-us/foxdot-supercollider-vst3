# 40: Self-contained FoxDot charm and spark patches.
Clock.bpm = 72
Scale.default.set("minor")

p1 >> charm(P[0, 3, 5, 2], dur=2, sus=1.8, oct=4,
           rate=0.5, amp=0.45, reverb="none", pan=-0.2)
p2 >> spark(P[0, 2, 4, 2], dur=1.5, sus=1.4, oct=3,
            amp=0.55, reverb="freeverb", room=0.2, mix=0.12, pan=0.2)
