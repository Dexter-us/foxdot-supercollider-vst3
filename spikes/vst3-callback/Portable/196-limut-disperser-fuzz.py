# Example 196: a moving all-pass Disperser beside a warm, driven Fuzz.
# Install the optional LimutFX UGen pack first.
Clock.bpm = 110
Scale.default = Scale.minor

# The staged all-pass network shifts transients and phase; it is not a reverb.
p1 >> pluck(P[0, 3, 7, 10], dur=0.5, oct=4, amp=0.22,
            effect="LimutDisperser", fx_mix=0.42,
            fx_frequency=var([420, 760, 1150], 2), fx_q=2.8,
            fx_stages=linvar([8, 20], [4, 4]))

# Drive thickens the harmonics; tone is the post-drive low-pass cutoff in Hz.
p2 >> bass(P[0, 0, 7, 3], dur=1, oct=5, amp=0.20,
           effect="LimutFuzz", fx_mix=0.55,
           fx_drive=var([12, 24], 4), fx_tone=linvar([3500, 9000], [4, 4]))

# Inserts run left-to-right: Disperser first, then a subtler Fuzz stage.
p3 >> triangle(P[7, 10, 12, 10], dur=1, oct=4, amp=0.12,
               effects=[
                   ("LimutDisperser", {
                       "fx_mix": 0.28, "fx_frequency": 900,
                       "fx_q": 2.2, "fx_stages": 14}),
                   ("LimutFuzz", {
                       "fx_mix": 0.24, "fx_drive": 9, "fx_tone": 11000})
               ])
