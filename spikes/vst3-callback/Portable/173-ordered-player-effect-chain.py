# Example 173: one player's sound passes through Phaser, then Tremolo.
# effects=[...] is a Dexter U.S. extension, not original FoxDot syntax.
# Insert order is left-to-right. Each dictionary belongs to that insert only.
Clock.bpm = 96
Scale.default = Scale.minor

p1 >> saw(P[0, 2, 4, 7, 4, 2, 3, 0], dur=0.5, sus=0.42,
          oct=4, amp=0.2, atk=0.015, rel=0.12,
          effects=[
              ("Phaser", {"fx_mix": 0.55, "fx_rate": 0.3,
                          "fx_depth": 0.7, "fx_feedback": 0.2}),
              ("Tremolo", {"fx_mix": 0.9, "fx_rate": var([3, 6], 4),
                           "fx_depth": 0.65, "fx_spread": 0.8})
          ])

# To compare, reverse the two complete entries above and press Run.
# Remove the inserts with effects=[]; effect="Phaser", fx_mix=... still works.
