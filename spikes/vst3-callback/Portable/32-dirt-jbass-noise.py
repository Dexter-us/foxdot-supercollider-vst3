# 32: A clearer bass groove with FoxDot-style dirt and noise colors.
Clock.bpm = 84
Scale.default.set("minor")

# A defined low-end riff so the bass line is easy to hear and follow.
p1 >> jbass(P[0, None, 0, 3, 5, None, 3, 2], dur=0.5, sus=0.38, oct=2,
            atk=0.015, rel=0.10, amp=0.30, pan=-0.12)

# Dirt is meant to be rough and unstable; use it as a quiet upper accent.
p2 >> dirt(P[None, 0, None, 2], dur=1, sus=0.72, oct=4,
          atk=0.02, rel=0.12, amp=0.055, pan=0.18)

# LFNoise0 is intentionally static-like; leave it very quiet as texture.
p3 >> noise(P[0, None, None, None], dur=0.5, sus=0.12, oct=5,
            atk=0.01, rel=0.06, amp=0.006, pan=0.35)
