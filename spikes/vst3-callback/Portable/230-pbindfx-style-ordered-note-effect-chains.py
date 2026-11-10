# Example 230: a bounded, ordered two-insert chain on selected note events.
Clock.bpm = 104
Scale.default = Scale.minor

# A nested tuple is the ordered chain for that note; None leaves that note dry.
# fx_event_mix is applied to each insert in the selected chain.
p1 >> pluck(
    P[0, 4, 7, 3, 0, 2, 5, 7],
    dur=0.5, sus=0.42, oct=4, amp=0.18,
    fx_event=P[
        ("Chorus", "Phaser"),
        None,
        ("Flanger", "Tremolo"),
    ],
    fx_event_mix=P[0.22, 0.0, 0.28, 0.32],
)

# The seeded choice is repeatable; every selected chain still runs in listed order.
p2 >> sine(
    P[0, 2, 4, 6],
    dur=2, sus=1.7, oct=3, amp=0.10, pan=0.25,
    fx_event=PRand(
        [("Chorus", "Phaser"), ("Flanger", "Tremolo"), None],
        length=4, seed=230,
    ),
    fx_event_mix=0.20,
)
