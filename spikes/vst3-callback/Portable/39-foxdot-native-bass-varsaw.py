# 39: Native FoxDot envelopes: no Dexter-specific atk/rel overrides.
# bass divides its input frequency by four, so oct=5 keeps the demo out of
# the subsonic range that can sound like clicks on ordinary speakers.
# It uses the source's 20 ms attack then a sus-length linear decay;
# varsaw rises and falls linearly across two halves of sus.
Clock.bpm = 72
Scale.default.set("minor")

p1 >> bass(P[0, 2, 4, 2], dur=1, sus=0.85, oct=5,
          amp=0.65, pan=-0.2, reverb="none")
p2 >> varsaw(P[0, 2, 4, 2], dur=1, sus=0.85, oct=5,
             rate=0.32, amp=0.7, pan=0.2,
             reverb="freeverb", room=0.25, mix=0.18)
