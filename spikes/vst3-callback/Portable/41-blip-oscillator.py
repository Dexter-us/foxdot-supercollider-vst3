# 41: FoxDot blip combines LFCub and LFTri with a Blip harmonic stack.
Clock.bpm = 76
Scale.default.set("minor")

p1 >> blip(P[0, 2, 4, 6, 4, 2], dur=1, sus=0.9, oct=5,
           rate=8, amp=0.6, reverb="none", pan=-0.1)
p2 >> charm(P[0, 4, 2], dur=2, sus=1.8, oct=3,
            rate=0.25, amp=0.35, reverb="freeverb",
            room=0.2, mix=0.12, pan=0.2)
