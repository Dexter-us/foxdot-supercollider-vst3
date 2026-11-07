/*
 * SCLOrk-style stereo crossover distortion insert.
 * Adapted from SCLOrkSynths/SynthDefs/misc/crossoverDistortion.scd by Josh
 * Mitchell. The original LFTri and per-note envelope are replaced by the
 * stereo player input. The paired positive/negative sigmoid transfer remains.
 * GPL-3.0-only. Uses stock SuperCollider UGens; no SCPlugins module is needed.
 *
 * Positional ABI: out, inbus, mix, drive, nonlinearity, tone.
 */
SynthDef(\SCLOrkCrossoverDistortionFX, {
    |out=0, inbus=0, mix=0.35, drive=2, nonlinearity=0.75, tone=9000|
    var dry = In.ar(inbus, 2);
    var mixSmooth = Lag.kr(mix.clip(0, 1), 0.025);
    var driveSmooth = Lag.kr(drive.clip(0.5, 8), 0.025);
    var shapeSmooth = Lag.kr(nonlinearity.clip(0, 1), 0.025);
    var toneSmooth = Lag.kr(tone.clip(200, 16000), 0.03)
        .min(SampleRate.ir * 0.45);
    var curve = shapeSmooth.linexp(0, 1, 2 ** (-5), 2 ** 5);
    var driven = (dry * driveSmooth).clip(-1, 1);
    var positive = (driven.clip(0, 1) * 2) - 1;
    var negative = ((0 - driven).clip(0, 1) * 2) - 1;
    var positiveCurve = 1 / (1 + ((0 - curve * positive).exp));
    var negativeCurve = 1 / (1 + ((0 - curve * negative).exp));
    var wet = positiveCurve - negativeCurve;
    wet = Limiter.ar(LPF.ar(LeakDC.ar(wet), toneSmooth), 0.98);
    Out.ar(out, XFade2.ar(dry, wet, (mixSmooth * 2) - 1));
}).writeDefFile;
