// Per-player stereo insert graph for Dexter U.S. FoxDot.
// The VST supplies the summed player signal through inbus and keeps this graph
// alive between notes so the original Airwindows echo state is not reset.
SynthDef(\AirwindowsEchojam, {
    |out=0, inbus=0, vib=0.29, depth=0.32, lforate=0.48, blur=0.62|
    var stereo, processed;
    stereo = In.ar(inbus, 2);
    processed = Echojam.ar(
        stereo[0], stereo[1], vib.clip(0, 1), depth.clip(0, 1),
        lforate.clip(0, 1), blur.clip(0, 1)
    );
    Out.ar(out, processed);
})
