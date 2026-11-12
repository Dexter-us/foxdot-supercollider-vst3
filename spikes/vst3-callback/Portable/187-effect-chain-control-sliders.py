# Example 187: select a player, expand its Effect chain, then select an insert.
# The seven sliders show controls supported by that specific effect.
# Drag a slider to change sound live; release it to write that value back here.
Clock.bpm = 96
Scale.default = Scale.minor

p1 >> pluck(P[0, 3, 7, 10], dur=0.5, sus=0.42, oct=4, amp=0.24,
            effects=[
                ("Chorus", {"fx_mix": 0.28, "fx_rate": 0.42,
                            "fx_depth": 0.36, "fx_delay": 0.018}),
                ("Phaser", {"fx_mix": 0.16, "fx_rate": 0.18,
                            "fx_depth": 0.34, "fx_center": 540})
            ])

p2 >> triangle(P[0, 7, 3, 10], dur=1, sus=0.88, oct=3, amp=0.13,
               effects=[
                   ("Tremolo", {"fx_mix": 0.3, "fx_rate": 4.5,
                                "fx_depth": 0.28}),
                   ("Chorus", {"fx_mix": 0.2, "fx_rate": 0.3,
                               "fx_depth": 0.24})
               ])
