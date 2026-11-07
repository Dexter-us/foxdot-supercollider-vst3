/*
 * SCLOrk-style stereo sample-rate/bit-depth reducer.
 * Adapted from SCLOrkSynths/SynthDefs/misc/decimator.scd by Josh Mitchell.
 * The original SinOsc and envelope are replaced with the stereo player inbus.
 * GPL-3.0-only. Uses stock SuperCollider UGens; no SCPlugins module is needed.
 *
 * Positional ABI: out, inbus, mix, rate, bits, drive, tone.
 */
SynthDef(\SCLOrkDecimatorFX, {
    |out=0, inbus=0, mix=0.4, rate=8000, bits=8, drive=1.3, tone=8000|
    var dry = In.ar(inbus, 2);
    var mixSmooth = Lag.kr(mix.clip(0, 1), 0.025);
    var rateSmooth = Lag.kr(rate.clip(80, 22000), 0.03)
        .min(SampleRate.ir * 0.45).max(20);
    var bitsSmooth = Lag.kr(bits.clip(1, 16), 0.025);
    var driveSmooth = Lag.kr(drive.clip(0.5, 8), 0.025);
    var toneSmooth = Lag.kr(tone.clip(200, 16000), 0.03)
        .min(rateSmooth * 0.48).max(40);
    var clock = LFPulse.ar(rateSmooth, 0, 0.5);
    var gained = (dry * driveSmooth).clip(-1, 1);
    var held = Latch.ar(gained, clock);
    var lowerBits = bitsSmooth.floor;
    var upperBits = bitsSmooth.ceil;
    var blend = (bitsSmooth - lowerBits).clip(0, 1);
    var lowerStep = 2 / (2 ** lowerBits);
    var upperStep = 2 / (2 ** upperBits);
    var lowerQuantized = (held / lowerStep).round * lowerStep;
    var upperQuantized = (held / upperStep).round * upperStep;
    var quantized = (lowerQuantized * (1 - blend)) +
        (upperQuantized * blend);
    var wet = LPF.ar(LeakDC.ar(quantized), toneSmooth);
    wet = Limiter.ar(wet, 0.98);
    Out.ar(out, XFade2.ar(dry, wet, (mixSmooth * 2) - 1));
}).writeDefFile;
