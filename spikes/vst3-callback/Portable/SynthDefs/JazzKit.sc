// Dexter Jazz Kit - standalone voices authored for the embedded FoxDot engine.
// Uses only stock SuperCollider UGens already included with the plugin.
// Each SynthDef writes one stereo signal to the supplied private "out" bus.
(
var targetDir = thisProcess.nowExecutingPath.dirname;

SynthDef(\JazzKitKick, {
    arg out=0, amp=0.34, pan=0, tune=48, decay=0.34,
        body=0.82, knock=0.08, shell=0.12, dexterAutoFree=2;
    var env, pitch, bodySig, knockSig, shellSig, sig;
    env = EnvGen.kr(Env.perc(0.003, decay, curve: -5),
        doneAction: dexterAutoFree);
    pitch = XLine.kr(tune * 2.15, tune, 0.05);
    bodySig = SinOsc.ar(pitch) * body;
    knockSig = HPF.ar(WhiteNoise.ar(1), 1900) *
        EnvGen.kr(Env.perc(0.001, 0.012, curve: -6)) * knock;
    shellSig = Resonz.ar(WhiteNoise.ar(1), tune * 2.4, 0.45) *
        EnvGen.kr(Env.perc(0.001, 0.055, curve: -4)) * shell;
    sig = (bodySig + knockSig + shellSig) * env * amp;
    Out.ar(out, Pan2.ar(sig, pan));
}).writeDefFile(targetDir);

SynthDef(\JazzKitBrushTap, {
    arg out=0, amp=0.22, pan=0, tune=185, tone=3400,
        wire=0.65, body=0.2, decay=0.25, dexterAutoFree=2;
    var env, wireEnv, shell, wires, sig;
    env = EnvGen.kr(Env.perc(0.002, decay, curve: -4),
        doneAction: dexterAutoFree);
    wireEnv = EnvGen.kr(Env.perc(0.001, decay * 0.78, curve: -5));
    shell = (SinOsc.ar(tune) + (SinOsc.ar(tune * 1.47) * 0.28)) * body;
    wires = BPF.ar(WhiteNoise.ar(1), tone, 0.78) * wire * wireEnv;
    sig = (shell + wires) * env * amp;
    Out.ar(out, Pan2.ar(sig, pan));
}).writeDefFile(targetDir);

SynthDef(\JazzKitBrushSweep, {
    arg out=0, amp=0.12, pan=0, startTone=850, endTone=4700,
        sweep=0.85, grit=0.55, dexterAutoFree=2;
    var env, center, noise, sig;
    env = EnvGen.kr(Env.linen(0.015, sweep, 0.045, curve: -4),
        doneAction: dexterAutoFree);
    center = XLine.kr(startTone, endTone, sweep + 0.04);
    noise = BPF.ar(WhiteNoise.ar(1), center, 0.42) * grit;
    sig = noise * env * amp;
    Out.ar(out, Pan2.ar(sig, pan));
}).writeDefFile(targetDir);

SynthDef(\JazzKitClosedHat, {
    arg out=0, amp=0.1, pan=0, tone=8200, decay=0.085, metal=0.08,
        dexterAutoFree=2;
    var env, noise, partials, sig;
    env = EnvGen.kr(Env.perc(0.001, decay, curve: -7),
        doneAction: dexterAutoFree);
    noise = HPF.ar(WhiteNoise.ar(1), tone);
    partials = Mix(SinOsc.ar([6200, 7900, 9300, 11100]) *
        [0.4, 0.27, 0.18, 0.12]);
    sig = (noise + (partials * metal)) * env * amp;
    Out.ar(out, Pan2.ar(sig, pan));
}).writeDefFile(targetDir);

SynthDef(\JazzKitOpenHat, {
    arg out=0, amp=0.1, pan=0, tone=7600, decay=0.92, metal=0.12,
        dexterAutoFree=2;
    var env, noise, partials, sig;
    env = EnvGen.kr(Env.perc(0.001, decay, curve: -5),
        doneAction: dexterAutoFree);
    noise = HPF.ar(WhiteNoise.ar(1), tone);
    partials = Mix(SinOsc.ar([6100, 7750, 9400, 11300]) *
        [0.42, 0.28, 0.18, 0.13]);
    sig = (noise + (partials * metal)) * env * amp;
    Out.ar(out, Pan2.ar(sig, pan));
}).writeDefFile(targetDir);

SynthDef(\JazzKitRide, {
    arg out=0, amp=0.12, pan=0, tune=310, decay=1.15,
        bell=0.2, noise=0.42, dexterAutoFree=2;
    var env, metal, wash, sig;
    env = EnvGen.kr(Env.perc(0.001, decay, curve: -3),
        doneAction: dexterAutoFree);
    metal = Mix(SinOsc.ar(tune * [1, 1.47, 1.91, 2.43]) *
        [0.62, 0.33, 0.2, 0.12]);
    wash = HPF.ar(WhiteNoise.ar(1), tune * 8);
    sig = (metal * bell + wash * noise) * env * amp;
    Out.ar(out, Pan2.ar(sig, pan));
}).writeDefFile(targetDir);

SynthDef(\JazzKitCrossStick, {
    arg out=0, amp=0.14, pan=0, tone=2100, decay=0.045, body=0.18,
        dexterAutoFree=2;
    var env, tick, wood, sig;
    env = EnvGen.kr(Env.perc(0.001, decay, curve: -8),
        doneAction: dexterAutoFree);
    tick = BPF.ar(WhiteNoise.ar(1), tone, 0.22);
    wood = SinOsc.ar(tone * 0.42) * body;
    sig = (tick + wood) * env * amp;
    Out.ar(out, Pan2.ar(sig, pan));
}).writeDefFile(targetDir);

SynthDef(\JazzKitRackTom, {
    arg out=0, amp=0.2, pan=0, tune=148, decay=0.48, body=0.9,
        dexterAutoFree=2;
    var env, pitch, shell, sig;
    env = EnvGen.kr(Env.perc(0.003, decay, curve: -3),
        doneAction: dexterAutoFree);
    pitch = XLine.kr(tune * 1.65, tune, 0.065);
    shell = BPF.ar(WhiteNoise.ar(1), tune * 3.2, 0.65) *
        EnvGen.kr(Env.perc(0.001, 0.024, curve: -5)) * 0.12;
    sig = ((SinOsc.ar(pitch) * body) + shell) * env * amp;
    Out.ar(out, Pan2.ar(sig, pan));
}).writeDefFile(targetDir);

SynthDef(\JazzKitFloorTom, {
    arg out=0, amp=0.22, pan=0, tune=86, decay=0.72, body=0.95,
        dexterAutoFree=2;
    var env, pitch, shell, sig;
    env = EnvGen.kr(Env.perc(0.004, decay, curve: -3),
        doneAction: dexterAutoFree);
    pitch = XLine.kr(tune * 1.55, tune, 0.075);
    shell = BPF.ar(WhiteNoise.ar(1), tune * 2.8, 0.58) *
        EnvGen.kr(Env.perc(0.001, 0.032, curve: -5)) * 0.14;
    sig = ((SinOsc.ar(pitch) * body) + shell) * env * amp;
    Out.ar(out, Pan2.ar(sig, pan));
}).writeDefFile(targetDir);
0.exit;
)
