// GPL-2.0-or-later. Stereo insert wrapper; upstream JPverb DSP is unchanged.
// Fixed control ABI: out, inbus, mix, t60, damp, size, earlydiff, moddepth, modfreq.
SynthDef(\JPverb, {
    |out=0, inbus=0, mix=0.32, t60=3, damp=0.25, size=1,
     earlydiff=0.707, moddepth=0.1, modfreq=2|
    var dry = In.ar(inbus, 2);
    var wet = JPverb.ar(dry, t60.clip(0.1, 60), damp.clip(0, 0.999),
        size.clip(0.5, 3), earlydiff.clip(0, 0.99),
        moddepth.clip(0, 1), modfreq.clip(0, 10));
    Out.ar(out, XFade2.ar(dry, wet, mix.clip(0, 1) * 2 - 1));
})
