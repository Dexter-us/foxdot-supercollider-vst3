/*
 * SCLOrk-style stereo phazer insert for the Dexter embedded player chain.
 * Adapted from SCLOrkSynths/SynthDefs/misc/phazer.scd by Josh Mitchell.
 * The original voice oscillator is replaced by a stereo inbus; the BAllPass
 * sweep and phase-cancellation character are retained. GPL-3.0-only.
 *
 * Positional ABI: out, inbus, mix, rate, depth, center, spread, rq, tone.
 * Stock SuperCollider UGens only; no additional SCPlugins module is needed.
 */
SynthDef(\SCLOrkPhazerFX, {
    |out=0, inbus=0, mix=0.45, rate=0.22, depth=0.65,
     center=650, spread=0.8, rq=0.25, tone=11000|
    var dry = In.ar(inbus, 2);
    var mixSmooth = Lag.kr(mix.clip(0, 1), 0.025);
    var rateSmooth = Lag.kr(rate.clip(0.05, 8), 0.04);
    var depthSmooth = Lag.kr(depth.clip(0, 1), 0.03);
    var centerSmooth = Lag.kr(center.clip(150, 1800), 0.03);
    var spreadSmooth = Lag.kr(spread.clip(0, 1), 0.03);
    var rqSmooth = Lag.kr(rq.clip(0.05, 1), 0.03);
    var toneSmooth = Lag.kr(tone.clip(200, 16000), 0.03);
    var nyquist = SampleRate.ir * 0.45;
    var amount = depthSmooth * spreadSmooth * 2;
    var phaseLeft = SinOsc.kr(rateSmooth, 0).range(-1, 1);
    var phaseRight = SinOsc.kr(rateSmooth, pi * 0.5).range(-1, 1);
    var centerLeft = (centerSmooth * (2 ** (phaseLeft * amount)))
        .clip(30, nyquist);
    var centerRight = (centerSmooth * (2 ** (phaseRight * amount)))
        .clip(30, nyquist);
    var ratios = [1, 2, 4, 8];
    var allpassLeft = Mix.ar(BAllPass.ar(dry[0], ratios * centerLeft, rqSmooth));
    var allpassRight = Mix.ar(BAllPass.ar(dry[1], ratios * centerRight, rqSmooth));
    var wet = [
        allpassLeft * 0.25 - dry[0],
        allpassRight * 0.25 - dry[1]
    ];
    wet = LPF.ar(LeakDC.ar(wet), toneSmooth.min(nyquist));
    Out.ar(out, XFade2.ar(dry, wet, (mixSmooth * 2) - 1));
}).writeDefFile;
