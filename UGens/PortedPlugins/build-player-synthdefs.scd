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

"Wrote thirteen PortedPlugins player/insert SynthDefs.".postln;
0.exit;
)
