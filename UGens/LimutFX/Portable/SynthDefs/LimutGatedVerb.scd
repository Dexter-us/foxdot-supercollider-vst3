SynthDef(\LimutGatedVerb, { |out=0, inbus=0, mix=0.65, room=0.75, damp=0.5, threshold=0.02, hold=0.25, attack=0.015, release=0.15|
    var input = In.ar(inbus, 2);
    var gated = LimutGatedVerb.ar(
        input[0], input[1], mix, room, damp, threshold, hold, attack, release);
    Out.ar(out, gated);
});
