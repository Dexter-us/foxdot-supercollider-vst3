// Per-player stereo insert graph for Dexter U.S. FoxDot.
// The VST supplies the summed player signal through inbus and keeps this graph
// alive between notes so the original Airwindows delay state is not reset.
SynthDef(\AirwindowsTapeDelay2, {
    |out=0, inbus=0, vib=0.94, depth=0.28, lforate=0.72,
     lfowidth=0.28, rq=0.12, blur=0.5|
    var stereo, processed;
    stereo = In.ar(inbus, 2);
    processed = TapeDelay2.ar(
        stereo[0], stereo[1],
        vib.clip(0, 1), depth.clip(0, 1), lforate.clip(0, 1),
        lfowidth.clip(0, 1), rq.clip(0, 1), blur.clip(0, 1)
    );
    Out.ar(out, processed);
})
