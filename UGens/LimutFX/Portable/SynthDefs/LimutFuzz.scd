SynthDef(\LimutFuzz, { |out=0, inbus=0, mix=0.35, driveDb=18, toneHz=9000|
    var input = In.ar(inbus, 2);
    var effected = LimutFuzz.ar(
        input[0], input[1], driveDb, toneHz, mix);
    Out.ar(out, effected);
});
