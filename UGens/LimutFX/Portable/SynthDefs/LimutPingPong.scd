// Limut-inspired stereo ping-pong insert.
// Controls: out, inbus, mix, delaytime (seconds), feedback.
SynthDef(\LimutPingPong, {
    |out=0, inbus=0, mix=0.28, delaytime=0.32, feedback=0.45|
    var dry = In.ar(inbus, 2);
    var wet = LimutPingPong.ar(dry[0], dry[1],
        delaytime.clip(0.01, 2.0), feedback.clip(0, 0.95));
    Out.ar(out, XFade2.ar(dry, wet, mix.clip(0, 1) * 2 - 1));
})
