# Example 227: select one short modulation effect for each note event.
# Requires Chorus.scsyndef, Flanger.scsyndef, and Phaser.scsyndef in
# Portable/SynthDefs. None leaves that event dry; selector patterns repeat.
Clock.bpm = 104
Scale.default = Scale.minor

p1 >> pluck(
    P[0, 3, 7, 10, 7, 3, 5, 2],
    dur=0.5,
    sus=0.42,
    oct=4,
    amp=0.22,
    fx_event=P[None, "Chorus", "Flanger", "Phaser"],
    fx_event_mix=P[0.0, 0.35, 0.4, 0.45],
)

# Existing player-wide effect chains remain available and are still shared
# across the player's summed notes. Per-event selection is limited to these
# short modulation effects; delays and reverbs keep their player-wide tails.
p2 >> triangle(P[0, 3, 5, 7], dur=2, sus=1.8, oct=3, amp=0.12)
