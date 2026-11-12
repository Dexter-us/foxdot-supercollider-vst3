(
var output = thisProcess.nowExecutingPath.dirname +/+ "SynthDefs";
var write = { |definition|
    definition.writeDefFile(output);
};

// Drum voices: one-shot PortedPlugins models triggered by the player gate.
write.value(SynthDef(\PortedAnalogBassDrum, {
    |out=0, freq=50, amp=0.22, gate=0, atk=0.001, rel=0.12,
     accent=0.7, tone=0.45, decay=0.45, attackfm=0.45, selffm=0.25|
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var voice = AnalogBassDrum.ar(
        gate, 0, accent.clip(0, 1), freq.clip(25, 180),
        tone.clip(0, 1), decay.clip(0, 1),
        attackfm.clip(0, 1), selffm.clip(0, 1));
    Out.ar(out, voice * amp * env);
}));

write.value(SynthDef(\PortedAnalogSnareDrum, {
    |out=0, freq=180, amp=0.18, gate=0, atk=0.001, rel=0.12,
     accent=0.65, tone=0.55, decay=0.4, snappy=0.7|
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var voice = AnalogSnareDrum.ar(
        gate, 0, accent.clip(0, 1), freq.clip(80, 500),
        tone.clip(0, 1), decay.clip(0.02, 1), snappy.clip(0, 1));
    Out.ar(out, voice * amp * env);
}));

// Oscillator voices.
write.value(SynthDef(\PortedBLOsc, {
    |out=0, freq=440, amp=0.16, gate=0, atk=0.01, rel=0.2,
     pulsewidth=0.5, waveform=0|
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var voice = BLOsc.ar(
        freq, pulsewidth.clip(0.02, 0.98), waveform.round(1).clip(0, 2));
    Out.ar(out, voice * amp * env);
}));

write.value(SynthDef(\PortedAnalogFoldOsc, {
    |out=0, freq=220, amp=0.14, gate=0, atk=0.01, rel=0.25|
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var voice = AnalogFoldOsc.ar(freq.clip(20, 12000), 1);
    Out.ar(out, voice * amp * env);
}));

write.value(SynthDef(\PortedNeoVarSawOsc, {
    |out=0, freq=220, amp=0.14, gate=0, atk=0.01, rel=0.25,
     pw=0, waveshape=0.5|
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var voice = NeoVarSawOsc.ar(
        freq.clip(20, 12000), pw.clip(-1, 1), waveshape.clip(0, 1));
    Out.ar(out, voice * amp * env);
}));

write.value(SynthDef(\PortedVosimOsc, {
    |out=0, freq=220, amp=0.14, gate=0, atk=0.01, rel=0.25,
     form1freq=700, form2freq=1400, shape=0|
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var voice = VosimOsc.ar(
        freq.clip(20, 12000), form1freq.clip(40, 10000),
        form2freq.clip(40, 12000), shape.clip(-1, 1));
    Out.ar(out, voice * amp * env);
}));

write.value(SynthDef(\PortedZOsc, {
    |out=0, freq=220, amp=0.14, gate=0, atk=0.01, rel=0.25,
     formantfreq=900, shape=0.5, mode=0|
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var voice = ZOsc.ar(
        freq.clip(20, 12000), formantfreq.clip(40, 12000),
        shape.clip(0, 1), mode.clip(-1, 1));
    Out.ar(out, voice * amp * env);
}));

// Plucked string model, triggered once for each player note.
write.value(SynthDef(\PortedStringVoice, {
    |out=0, freq=220, amp=0.16, gate=0, atk=0.001, rel=0.35,
     accent=0.65, structure=0.35, brightness=0.55, damping=0.45|
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var voice = StringVoice.ar(
        gate, 0, freq.clip(30, 5000), accent.clip(0, 1),
        structure.clip(0, 1), brightness.clip(0, 1), damping.clip(0, 1));
    Out.ar(out, voice * amp * env);
}));

// Modal/resonant voices. Rongs uses the inexpensive 32-mode setting; its
// mode count and cosine table are init-only by the upstream UGen.
write.value(SynthDef(\PortedRongs, {
    |out=0, freq=220, amp=0.08, gate=0, atk=0.001, rel=0.45, pan=0,
     structure=0.45, brightness=0.55, damping=0.55, accent=0.65,
     stretch=0.35, position=0.15, loss=0.15|
    var trigger = Trig1.kr(gate, 0.001);
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var voice = Rongs.ar(
        trigger, gate, freq.clip(35, 3000), structure.clip(0, 1),
        brightness.clip(0, 1), damping.clip(0, 1), accent.clip(0, 1),
        stretch.clip(0, 1), position.clip(0, 1), loss.clip(0, 1),
        1, 0.025);
    Out.ar(out, Pan2.ar(LeakDC.ar(voice).tanh, pan) * amp * env);
}));

write.value(SynthDef(\PortedPhasorModal, {
    |out=0, freq=220, amp=0.08, gate=0, atk=0.001, rel=0.4,
     decay=0.8, damp=0.75, modalamp=0.45, phase=0, excite=0.35, pan=0|
    var trigger = Trig1.kr(gate, 0.001);
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var burst = Decay2.kr(trigger, 0.001, 0.035);
    var excitation = LPF.ar(WhiteNoise.ar * burst * excite.clip(0, 1),
        (freq * 5).clip(150, 12000));
    var voice = PhasorModal.ar(
        excitation, freq.clip(35, 6000), decay.clip(0.04, 5),
        damp.clip(0, 1), modalamp.clip(0, 1), phase.clip(0, 1));
    Out.ar(out, Pan2.ar(LeakDC.ar(voice).tanh, pan) * amp * env);
}));

write.value(SynthDef(\PortedResonator, {
    |out=0, freq=220, amp=0.08, gate=0, atk=0.001, rel=0.45,
     position=0.15, structure=0.4, brightness=0.35, damping=0.55,
     excite=0.3, pan=0|
    var trigger = Trig1.kr(gate, 0.001);
    var env = EnvGen.kr(Env.asr(atk, 1, rel), gate, doneAction: 2);
    var burst = Decay2.kr(trigger, 0.001, 0.045);
    var excitation = PinkNoise.ar * burst * excite.clip(0, 1);
    var voice = Resonator.ar(
        excitation, freq.clip(35, 6000), position.clip(0, 1), 24,
        structure.clip(0, 1), brightness.clip(0, 1), damping.clip(0, 1));
    Out.ar(out, Pan2.ar(LeakDC.ar(voice).tanh, pan) * amp * env);
}));

// Persistent stereo insert wrapper. Resonator's position and resolution are
// init-only upstream controls, so they are fixed here; the exposed sliders are
// only for parameters that safely respond to live updates.
write.value(SynthDef(\PortedResonatorFX, {
    |out=0, inbus=0, fx_mix=0.28, fx_freq=220, fx_structure=0.45,
     fx_brightness=0.35, fx_damping=0.55|
    var dry = In.ar(inbus, 2);
    var frequency = Lag.kr(fx_freq.clip(35, 6000), 0.025);
    var structure = Lag.kr(fx_structure.clip(0, 1), 0.025);
    var brightness = Lag.kr(fx_brightness.clip(0, 1), 0.025);
    var damping = Lag.kr(fx_damping.clip(0, 1), 0.025);
    var wetLeft = Resonator.ar(
        dry[0], frequency, 0.15, 24, structure, brightness, damping);
    var wetRight = Resonator.ar(
        dry[1], frequency, 0.15, 24, structure, brightness, damping);
    var wet = [LeakDC.ar(wetLeft).tanh, LeakDC.ar(wetRight).tanh];
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.025);
    Out.ar(out, (dry * (1 - mix)) + (wet * mix));
}));

// Fverb is the stereo Dattorro reverb included in the PortedPlugins pack.
// Keep the insert's live surface deliberately small; the remaining diffusion,
// filter, and modulation settings use the upstream defaults.
write.value(SynthDef(\PortedFverbFX, {
    |out=0, inbus=0, fx_mix=0.3, fx_predelay=0, fx_decay=50,
     fx_damping=5500, fx_mod_depth=0.5|
    var dry = In.ar(inbus, 2);
    var predelay = Lag.kr(fx_predelay.clip(0, 10), 0.025);
    var decay = Lag.kr(fx_decay.clip(0, 100), 0.025);
    var damping = Lag.kr(fx_damping.clip(10, 20000), 0.025);
    var modDepth = Lag.kr(fx_mod_depth.clip(0, 1), 0.025);
    var wet = Fverb.ar(
        dry[0], dry[1], predelay, 100, 10000, 100,
        75, 62.5, 70, decay, damping, 1, modDepth);
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.025);
    Out.ar(out, XFade2.ar(dry, LeakDC.ar(wet), (mix * 2) - 1));
}));

// Tape saturation. Oversampling and solver mode remain fixed at conservative
// settings so live slider movement cannot change CPU cost abruptly.
write.value(SynthDef(\PortedAnalogTapeFX, {
    |out=0, inbus=0, fx_mix=0.25, fx_bias=0.5, fx_saturation=0.3,
     fx_drive=0.25|
    var dry = In.ar(inbus, 2);
    var bias = Lag.kr(fx_bias.clip(0, 1), 0.04);
    var saturation = Lag.kr(fx_saturation.clip(0, 1), 0.04);
    var drive = Lag.kr(fx_drive.clip(0, 1), 0.04);
    var wet = LeakDC.ar(AnalogTape.ar(dry, bias, saturation, drive, 1, 0));
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.04);
    Out.ar(out, (dry * (1 - mix)) + (wet * mix));
}));

// Tape chew / dropouts.
write.value(SynthDef(\PortedAnalogChewFX, {
    |out=0, inbus=0, fx_mix=0.25, fx_depth=0.2, fx_frequency=0.5,
     fx_variance=0.1|
    var dry = In.ar(inbus, 2);
    var depth = Lag.kr(fx_depth.clip(0, 1), 0.05);
    var frequency = Lag.kr(fx_frequency.clip(0, 1), 0.05);
    var variance = Lag.kr(fx_variance.clip(0, 1), 0.05);
    var wet = LeakDC.ar(AnalogChew.ar(dry, depth, frequency, variance));
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.04);
    Out.ar(out, (dry * (1 - mix)) + (wet * mix));
}));

// Tape wear and noisy degradation.
write.value(SynthDef(\PortedAnalogDegradeFX, {
    |out=0, inbus=0, fx_mix=0.25, fx_depth=0.2, fx_amount=0.2,
     fx_variance=0.1, fx_envelope=0.25|
    var dry = In.ar(inbus, 2);
    var depth = Lag.kr(fx_depth.clip(0, 1), 0.05);
    var amount = Lag.kr(fx_amount.clip(0, 1), 0.05);
    var variance = Lag.kr(fx_variance.clip(0, 1), 0.05);
    var envelope = Lag.kr(fx_envelope.clip(0, 1), 0.05);
    var wet = LeakDC.ar(
        AnalogDegrade.ar(dry, depth, amount, variance, envelope));
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.04);
    Out.ar(out, (dry * (1 - mix)) + (wet * mix));
}));

// Tape-head loss. Slow parameter smoothing avoids abrupt coefficient jumps;
// avoid fast modulation because this upstream UGen is CPU intensive.
write.value(SynthDef(\PortedAnalogLossFX, {
    |out=0, inbus=0, fx_mix=0.25, fx_gap=0.25, fx_thick=0.5,
     fx_space=0.35, fx_speed=0.5|
    var dry = In.ar(inbus, 2);
    var gap = Lag.kr(fx_gap.clip(0, 1), 0.12);
    var thick = Lag.kr(fx_thick.clip(0, 1), 0.12);
    var space = Lag.kr(fx_space.clip(0, 1), 0.12);
    var speed = Lag.kr(fx_speed.clip(0, 1), 0.12);
    var wet = LeakDC.ar(AnalogLoss.ar(dry, gap, thick, space, speed));
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.05);
    Out.ar(out, (dry * (1 - mix)) + (wet * mix));
}));

// Feedback phaser with an internal, bounded LFO.
write.value(SynthDef(\PortedAnalogPhaserFX, {
    |out=0, inbus=0, fx_mix=0.3, fx_rate=0.25, fx_skew=0,
     fx_feedback=0.25, fx_modulation=0.5, fx_stages=8|
    var dry = In.ar(inbus, 2);
    var rate = Lag.kr(fx_rate.clip(0.05, 8), 0.05);
    var skew = Lag.kr(fx_skew.clip(-1, 1), 0.05);
    var feedback = Lag.kr(fx_feedback.clip(0, 0.95), 0.05);
    var modulation = Lag.kr(fx_modulation.clip(0, 1), 0.05);
    var stages = Lag.kr(fx_stages.round.clip(1, 50), 0.05);
    var lfo = SinOsc.kr(rate).range(-1, 1);
    var wet = LeakDC.ar(
        AnalogPhaser.ar(dry, lfo, skew, feedback, modulation, stages));
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.04);
    Out.ar(out, (dry * (1 - mix)) + (wet * mix));
}));

// Baxandall-style EQ plus vintage drive. Oversampling stays disabled to keep
// the default insert inexpensive; it is not a live control.
write.value(SynthDef(\PortedAnalogVintageDistortionFX, {
    |out=0, inbus=0, fx_mix=0.3, fx_drivegain=0.25, fx_bias=0,
     fx_lowgain=0.08, fx_highgain=0.08, fx_shelvingfreq=600|
    var dry = In.ar(inbus, 2);
    var drivegain = Lag.kr(fx_drivegain.clip(0, 1), 0.05);
    var bias = Lag.kr(fx_bias.clip(0, 2.5), 0.05);
    var lowgain = Lag.kr(fx_lowgain.clip(0.0001, 0.3), 0.05);
    var highgain = Lag.kr(fx_highgain.clip(0.001, 0.3), 0.05);
    var shelf = Lag.kr(fx_shelvingfreq.clip(20, 20000), 0.05);
    var wet = LeakDC.ar(
        AnalogVintageDistortion.ar(
            dry, drivegain, bias, lowgain, highgain, shelf, 0));
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.04);
    Out.ar(out, (dry * (1 - mix)) + (wet * mix));
}));

// Lockhart wavefolder: the number of cells is fixed; bias and pregain are
// smoothed and bounded to stay within the upstream UGen's safe operating zone.
write.value(SynthDef(\PortedLockhartWavefolderFX, {
    |out=0, inbus=0, fx_mix=0.25, fx_bias=0.1, fx_pregain=2|
    var dry = In.ar(inbus, 2);
    var bias = Lag.kr(fx_bias.clip(0, 1), 0.05);
    var pregain = Lag.kr(fx_pregain.clip(1, 20), 0.05);
    var folded = LockhartWavefolder.ar((dry * pregain) + bias, 4);
    var wet = LeakDC.ar(folded).tanh;
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.04);
    Out.ar(out, (dry * (1 - mix)) + (wet * mix));
}));

// Self-keyed compressor. The player chain has no external sidechain bus, so
// DCompressor's built-in detector follows the signal being compressed.
write.value(SynthDef(\PortedDCompressorFX, {
    |out=0, inbus=0, fx_mix=0.5, fx_ratio=4, fx_threshold=(-24),
     fx_attack=0.01, fx_release=0.1, fx_makeup=0.5, fx_automakeup=1|
    var dry = In.ar(inbus, 2);
    var ratio = Lag.kr(fx_ratio.clip(1, 40), 0.04);
    var threshold = Lag.kr(fx_threshold.clip(-80, 0), 0.04);
    var attack = Lag.kr(fx_attack.clip(0.001, 10), 0.04);
    var release = Lag.kr(fx_release.clip(0.001, 10), 0.04);
    var makeup = Lag.kr(fx_makeup.clip(0, 24), 0.04);
    var automakeup = Lag.kr(fx_automakeup.round.clip(0, 1), 0.04);
    var wet = LeakDC.ar(
        DCompressor.ar(
            dry, 0, 0, ratio, threshold, attack, release, makeup, automakeup));
    var mix = Lag.kr(fx_mix.clip(0, 1), 0.04);
    Out.ar(out, (dry * (1 - mix)) + (wet * mix));
}));

"Wrote twenty-one PortedPlugins player/insert SynthDefs.".postln;
0.exit;
)
