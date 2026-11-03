// GPL-2.0-or-later. Stereo insert wrapper; upstream Greyhole DSP is unchanged.
// Fixed ABI: out, inbus, mix, delaytime, damp, size, diff, feedback, moddepth.
// modFreq is fixed at the upstream default of 2 Hz in this first seven-slot ABI.
SynthDef(\Greyhole, {
    |out=0, inbus=0, mix=0.32, delaytime=0.65, damp=0.25,
     size=1, diff=0.707, feedback=0.78, moddepth=0.1|
    var dry = In.ar(inbus, 2);
    var wet = Greyhole.ar(dry, delaytime.clip(0.001, 1.45),
        damp.clip(0, 0.99), size.clip(0.5, 3), diff.clip(0, 0.99),
        feedback.clip(0, 0.98), moddepth.clip(0, 1), 2);
    Out.ar(out, XFade2.ar(dry, wet, mix.clip(0, 1) * 2 - 1));
})
