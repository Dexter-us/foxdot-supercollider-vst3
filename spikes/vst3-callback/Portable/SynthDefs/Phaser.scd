// Dexter U.S. FoxDot Phaser insert. Uses stock SuperCollider UGens only.
// Positional ABI: out, inbus, mix, rate, depth, center, spread, feedback, tone.
SynthDef(\Phaser, { |out=0, inbus=0, mix=0.45, rate=0.22, depth=0.65,
    center=650, spread=0.8, feedback=0.25, tone=11000|
    var dry = In.ar(inbus, 2);
    var prior = LocalIn.ar(2);
    var leftPhase = SinOsc.kr(rate, 0).range(-1, 1);
    var rightPhase = SinOsc.kr(rate, spread.clip(0, 1) * pi).range(-1, 1);
    var motion = [leftPhase, rightPhase] * depth.clip(0, 1) * 0.7;
    var frequency = (center.clip(150, 1800) * (1 + motion)).clip(100, 4000);
    var baseDelay = (1 / frequency).clip(0.00025, 0.01);
    var signal = dry + (LeakDC.ar(prior) * feedback.clip(0, 0.85));
    var wet = AllpassC.ar(signal, 0.02, (baseDelay * 0.26).clip(0.0002, 0.019), 0.08);
    wet = AllpassC.ar(wet, 0.02, (baseDelay * 0.37).clip(0.0002, 0.019), 0.08);
    wet = AllpassC.ar(wet, 0.02, (baseDelay * 0.52).clip(0.0002, 0.019), 0.08);
    wet = AllpassC.ar(wet, 0.02, (baseDelay * 0.72).clip(0.0002, 0.019), 0.08);
    wet = AllpassC.ar(wet, 0.02, (baseDelay * 1.0).clip(0.0002, 0.019), 0.08);
    wet = AllpassC.ar(wet, 0.02, (baseDelay * 1.38).clip(0.0002, 0.019), 0.08);
    wet = AllpassC.ar(wet, 0.02, (baseDelay * 1.85).clip(0.0002, 0.019), 0.08);
    LocalOut.ar(wet.tanh);
    wet = LPF.ar(wet, tone.clip(200, 16000));
    Out.ar(out, XFade2.ar(dry, wet, mix.clip(0, 1) * 2 - 1));
}).writeDefFile;
