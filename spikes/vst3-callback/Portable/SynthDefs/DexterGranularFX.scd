// Stock-SuperCollider granular player insert.
// Source is the selected player's signal, including WAV sample playback.
SynthDef(\DexterGranularFX, {
    |out=0, inbus=0, fx_mix=0.3, fx_mode=0, fx_density=12,
     fx_size=0.08, fx_pitch=1, fx_position=0.5, fx_freeze=0|
    var dry = In.ar(inbus, 2);
    var mono = (dry[0] + dry[1]) * 0.5;
    var buffer = LocalBuf(131072, 1).clear;
    var mode = Lag.kr(fx_mode.round.clip(0, 3), 0.025);
    var density = Lag.kr(fx_density.clip(1, 40), 0.025);
    var size = Lag.kr(fx_size.clip(0.01, 0.25), 0.025);
    var pitch = Lag.kr(fx_pitch.clip(0.5, 2), 0.025);
    var position = Lag.kr(fx_position.clip(0, 1), 0.025);
    var freeze = Lag.kr(fx_freeze.clip(0, 1), 0.025);
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.025);
    var cloudTrigger = Dust.kr(density);
    var sliceTrigger = Impulse.kr(density);
    var liveTrigger = Impulse.kr(density);
    var recordRun = Select.kr(mode.round, [1, 1, 1 - freeze, 1]);
    var cloudPosition;
    var slicePosition;
    var freezePosition;
    var cloud;
    var sliced;
    var frozen;
    var shiftedInput;
    var live;
    var wet;

    // A per-instance 2.97-second ring buffer at 44.1 kHz. In freeze mode the
    // player can fill it first, then fx_freeze=1 holds the captured texture.
    RecordBuf.ar(mono, buffer, recLevel: 1, preLevel: 0,
        run: recordRun, loop: 1, trigger: 1);

    cloudPosition =
        (position + TRand.kr(-0.25, 0.25, cloudTrigger)).wrap(0, 1);
    cloud = GrainBuf.ar(
        2, cloudTrigger, size, buffer, pitch, cloudPosition, 2,
        TRand.kr(-1, 1, cloudTrigger), -1, 128);

    // Walks through sixteen offsets in order. fx_density is grains per second,
    // so set it to BPM/60 for quarter-note spacing.
    slicePosition =
        (position + (PulseCount.kr(sliceTrigger, 0).mod(16) / 16)).wrap(0, 1);
    sliced = GrainBuf.ar(
        2, sliceTrigger, size, buffer, pitch, slicePosition, 2,
        TRand.kr(-0.8, 0.8, sliceTrigger), -1, 128);

    freezePosition =
        (position + TRand.kr(-0.06, 0.06, cloudTrigger)).wrap(0, 1);
    frozen = GrainBuf.ar(
        2, cloudTrigger, size, buffer, pitch, freezePosition, 2,
        TRand.kr(-0.9, 0.9, cloudTrigger), -1, 128);

    // GrainIn reads the player's current signal directly. PitchShift is
    // applied before the grains so fx_pitch remains useful in this mode too.
    shiftedInput = PitchShift.ar(
        mono, (size * 2).clip(0.04, 0.5), pitch, 0, 0);
    live = GrainIn.ar(
        2, liveTrigger, size, shiftedInput,
        TRand.kr(-0.8, 0.8, liveTrigger), -1, 128);

    wet = SelectX.ar(mode, [cloud, sliced, frozen, live]);
    Out.ar(out, XFade2.ar(dry, LeakDC.ar(wet), (mix * 2) - 1));
}).writeDefFile(thisProcess.nowExecutingPath.dirname);
