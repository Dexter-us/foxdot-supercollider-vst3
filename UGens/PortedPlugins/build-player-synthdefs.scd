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

"Wrote eight PortedPlugins player SynthDefs.".postln;
0.exit;
)
