# LimutVocoder sidechain example

`200-limut-vocoder-two-bus.scd` shows the compiled two-input `LimutVocoder`
SynthDef using independent carrier and modulator buses. It is included for
SuperCollider-side experimentation; it is not an embedded Python player
example, and the current VST player-effect chain cannot route its second input.

Install `LimutFX.scx` under the host's `SCPlugins` directory, install
`LimutVocoder.scsyndef` under `Portable/Sidechain/SynthDefs`, and restart the
host. Do not copy it into `Portable/SynthDefs`: that folder is scanned for
single-input player effects. The example assumes the caller creates distinct
audio buses and keeps the vocoder node ordered after both source nodes.
