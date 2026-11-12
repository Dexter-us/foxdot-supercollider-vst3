// Stock-SuperCollider multi-tap delay player insert.
// fx_mode: 0 tempo-synced, 1 free-time seconds, 2 tempo-synced random taps.
SynthDef(\DexterMultiTapDelayFX, {
    |out=0, inbus=0, fx_mix=0.3, fx_mode=0, fx_time=0.25,
     fx_taps=4, fx_feedback=0.35, fx_probability=0.7, fx_bpm=120|
    var dry = In.ar(inbus, 2);
    var mono = (dry[0] + dry[1]) * 0.5;
    var mode = fx_mode.round.clip(0, 2);
    var beatSeconds = 60 / fx_bpm.clip(30, 300);
    var baseTime = Select.kr(mode, [
        fx_time.clip(1/32, 1) * beatSeconds,
        fx_time.clip(0.015, 1),
        fx_time.clip(1/32, 1) * beatSeconds
    ]);
    var taps = fx_taps.round.clip(1, 4);
    var feedback = fx_feedback.clip(0, 0.92);
    var decay = (baseTime * (feedback / (1 - feedback.max(0.02))))
        .clip(0.01, 4);
    var envelope = Amplitude.kr(mono, 0.005, 0.04);
    var onset = HPZ1.kr(envelope > 0.012).max(0);
    var probability = fx_probability.clip(0, 1);
    var wet;

    wet = Array.fill(4, { |index|
        var active = Lag.kr((taps >= (index + 1)), 0.02);
        var randomGate = Lag.kr(
            (TRand.kr(0, 1, onset) < probability), 0.025);
        var gate = Select.kr(mode, [1, 1, randomGate]);
        var tapTime = (baseTime * (index + 1)).clip(0.01, 7.95);
        var tap = CombC.ar(mono, 8.1, tapTime, decay);
        var pan = ((index % 2) * 2 - 1) * 0.6;
        Pan2.ar(tap * active * gate, pan)
    }).sum / taps.sqrt;

    Out.ar(out, XFade2.ar(dry, LeakDC.ar(wet), (fx_mix.clip(0, 1) * 2) - 1));
}).writeDefFile(thisProcess.nowExecutingPath.dirname);
