SynthDef(\LimutShifter, { |out=0, inbus=0, mix=0.5, ratio=1.5, window=0.12|
    var input = In.ar(inbus, 2);
    var shifted = LimutShifter.ar(input[0], input[1], mix, ratio, window);
    Out.ar(out, shifted);
});
