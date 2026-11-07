LimutVocoder is a true two-input UGen: it needs separate stereo carrier and
modulator buses. Example 200 demonstrates that routing in SuperCollider.

The embedded FoxDot engine currently gives a player-effect insert only the
player's own stereo signal. It does not yet provide a second player/sidechain
bus, so LimutVocoder is intentionally not accepted as effect= or effects=.
Do not move LimutVocoder.scsyndef into Portable/SynthDefs; that folder is
automatically scanned for single-player SynthDefs.

The LimutVocoder unit is included in LimutFX.scx. To run the standalone example,
install the UGen module and LimutFX.sc class in a SuperCollider environment,
then evaluate 200-limut-vocoder-two-bus.scd.
