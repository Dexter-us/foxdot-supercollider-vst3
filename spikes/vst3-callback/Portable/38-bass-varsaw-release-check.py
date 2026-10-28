# 38: Compare the release edges with separate, slow bass and varsaw layers.
# p1 is dry; p2 uses FreeVerb. Mute either player to isolate that voice.
Clock.bpm = 72
Scale.default.set("minor")

p1 >> bass(P[0, 2, 4, 2], dur=1, sus=0.9, oct=3,
          rate=5, atk=0.04, rel=0.28, amp=0.16,
          reverb="none", pan=-0.2)
p2 >> varsaw(P[0, 2, 4, 2], dur=1, sus=0.9, oct=4,
             rate=0.5, atk=0.04, rel=0.28, amp=0.08,
             reverb="freeverb", room=0.3, mix=0.18, pan=0.2)
