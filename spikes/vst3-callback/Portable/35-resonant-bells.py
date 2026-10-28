# 35: FoxDot-style Klank bells and marimba alongside a sustained gong.
Clock.bpm = 96
Scale.default.set("major_pentatonic")

p1 >> bell(P[0, 2, 4, 7], dur=1, sus=0.9, oct=5, rate=1,
          atk=0.01, rel=0.18, amp=0.16, pan=0.22, reverb="freeverb",
          room=0.35, mix=0.22)
p2 >> marimba(P[0, None, 2, None, 4, None, 7, None], dur=0.5, sus=0.42,
              oct=4, atk=0.01, rel=0.14, amp=0.18, pan=-0.18)
p3 >> gong(P[0, None, None, 4], dur=2, sus=1.8, oct=3,
           atk=0.02, rel=0.22, amp=0.055, pan=0.0, reverb="freeverb",
           room=0.55, mix=0.28)
