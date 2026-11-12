SynthDef(\LimutDisperser, { |out=0, inbus=0, mix=0.35, frequency=440, q=3, stages=24|
    var input = In.ar(inbus, 2);
    var effected = LimutDisperser.ar(
        input[0], input[1], frequency, q, stages, mix);
    Out.ar(out, effected);
});
