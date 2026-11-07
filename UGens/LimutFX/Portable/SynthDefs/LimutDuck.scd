// Limut-inspired rhythmic ducking insert. Timing controls are seconds.
// Controls: out, inbus, mix, period, depth, length, attack.
SynthDef(\LimutDuck, {
    |out=0, inbus=0, mix=1, period=0.5, depth=0.7, length=0.12, attack=0.18|
    var dry = In.ar(inbus, 2);
    var wet = LimutDuck.ar(dry[0], dry[1],
        period.clip(0.05, 8.0), depth.clip(0, 1),
        length.clip(0, 8.0), attack.clip(0.001, 8.0));
    Out.ar(out, XFade2.ar(dry, wet, mix.clip(0, 1) * 2 - 1));
})
