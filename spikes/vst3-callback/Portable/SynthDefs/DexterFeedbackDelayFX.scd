// Stock-SuperCollider feedback delay player insert.
// fx_mode: 0 low-pass repeats, 1 saturation, 2 pitch-shifted repeats,
//          3 stereo cross-feedback, 4 chance-gated feedback at fx_interval.
SynthDef(\DexterFeedbackDelayFX, {
    |out=0, inbus=0, fx_mix=0.3, fx_time=0.28, fx_feedback=0.55,
     fx_mode=0, fx_pitch=1.25, fx_interval=1, fx_probability=0.65|
    var dry = In.ar(inbus, 2);
    var mode = Lag.kr(fx_mode.round.clip(0, 4), 0.025);
    var delayTime = Lag.kr(fx_time.clip(0.015, 3.8), 0.025);
    var feedback = Lag.kr(fx_feedback.clip(0, 0.92), 0.025);
    var loop = LocalIn.ar(2);
    var source = dry + loop;
    var ratio = Lag.kr(fx_pitch.clip(0.5, 2), 0.025);
    var timer = Impulse.kr(1 / fx_interval.clip(0.1, 60));
    var chance = Lag.kr(TRand.kr(0, 1, timer) < fx_probability.clip(0, 1), 0.03);
    var timedGate = Select.kr(mode == 4, [1, chance]);
    var filtered = LPF.ar(source, 6500);
    var saturated = tanh(source * 4);
    var shifted = PitchShift.ar(source, 0.12, ratio, 0, 0);
    var crossed = [source[1], source[0]];
    var processed = SelectX.ar(mode.clip(0, 3), [
        filtered, saturated, shifted, crossed
    ]);
    var delayed = DelayC.ar(processed, 4, delayTime);
    var gate = Lag.kr(timedGate, 0.03);

    LocalOut.ar(Limiter.ar(delayed * feedback * gate, 0.95));
    Out.ar(out, XFade2.ar(dry, LeakDC.ar(delayed * gate),
        (fx_mix.clip(0, 1) * 2) - 1));
}).writeDefFile(thisProcess.nowExecutingPath.dirname);
