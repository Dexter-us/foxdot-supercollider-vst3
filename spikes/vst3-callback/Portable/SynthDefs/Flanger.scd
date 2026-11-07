// Dexter U.S. FoxDot Flanger insert. Uses only stock SuperCollider UGens.
// Positional ABI: out, inbus, mix, rate, depth, delay, spread, feedback, tone.
SynthDef(\Flanger, { |out=0, inbus=0, mix=0.35, rate=0.4, depth=0.6,
    delay=0.0035, spread=0.6, feedback=0.5, tone=12000|
    var dry = In.ar(inbus, 2);
    var prior = LocalIn.ar(2);
    var leftPhase = SinOsc.kr(rate, 0);
    var rightPhase = SinOsc.kr(rate, spread.clip(0, 1) * 2pi);
    var leftTime = (delay + (leftPhase * depth * 0.003)).clip(0.00025, 0.05);
    var rightTime = (delay + (rightPhase * depth * 0.003)).clip(0.00025, 0.05);
    var delayed = DelayC.ar(
        dry + (LeakDC.ar(prior) * feedback.clip(0, 0.95)),
        0.05, [leftTime, rightTime]);
    var wet = LPF.ar(delayed, tone.clip(200, 16000));
    LocalOut.ar(delayed.tanh);
    Out.ar(out, XFade2.ar(dry, wet, mix.clip(0, 1) * 2 - 1));
}).writeDefFile;
