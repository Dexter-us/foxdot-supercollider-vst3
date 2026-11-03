// Dexter U.S. FoxDot Tremolo insert. Uses stock SuperCollider UGens only.
// Positional ABI: out, inbus, mix, rate, depth, shape, spread, phase, tone.
SynthDef(\Tremolo, { |out=0, inbus=0, mix=1, rate=5, depth=0.7,
    shape=0, spread=0.5, phase=0, tone=12000|
    var dry = In.ar(inbus, 2);
    var phaseOffset = phase.clip(0, 1) * 2pi;
    var stereoPhase = phaseOffset + [0, spread.clip(0, 1) * pi];
    var sine = SinOsc.kr(rate.clip(0.05, 20), stereoPhase);
    var triangle = LFTri.kr(rate.clip(0.05, 20), stereoPhase);
    var modulation = ((1 - shape.clip(0, 1)) * sine +
        (shape.clip(0, 1) * triangle)).range(0, 1);
    var gain = (1 - (depth.clip(0, 1) * modulation)).clip(0, 1);
    var wet = LPF.ar(dry * gain, tone.clip(200, 16000));
    Out.ar(out, XFade2.ar(dry, wet, mix.clip(0, 1) * 2 - 1));
}).writeDefFile;
