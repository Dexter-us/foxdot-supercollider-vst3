SynthDef(\LimutShimmer, { |out=0, inbus=0, mix=0.35, ratio=2, window=0.12, room=0.8, damp=0.35|
    var input = In.ar(inbus, 2);
    var shimmer = LimutShimmer.ar(
        input[0], input[1], mix, ratio, window, room, damp);
    Out.ar(out, shimmer);
});
