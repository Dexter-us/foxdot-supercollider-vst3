// Standalone SuperCollider example for the two-input LimutVocoder UGen.
// This is not an embedded-FoxDot player script: the current VST player effect
// chain has no per-player modulator/sidechain bus route yet.
(
s.waitForBoot {
    var carrierBus = Bus.audio(s, 2);
    var modulatorBus = Bus.audio(s, 2);

    SynthDef(\VocoderCarrierExample, { |out=0, freq=110|
        var signal = (Saw.ar(freq) * 0.18) ! 2;
        Out.ar(out, signal);
    }).add;

    SynthDef(\VocoderModulatorExample, { |out=0|
        var syllable = LFPulse.kr(3.2, 0, 0.36);
        var voiceLike = (Formant.ar(180, 700 + (syllable * 600), 90) * 0.4) ! 2;
        Out.ar(out, voiceLike);
    }).add;

    SynthDef(\VocoderExample, {
        var carrier = In.ar(carrierBus, 2);
        var modulator = In.ar(modulatorBus, 2);
        var signal = LimutVocoder.ar(
            carrier[0], carrier[1], modulator[0], modulator[1],
            1.0, 120, 9000, 0.005, 0.08, 0, 1.8);
        Out.ar(0, signal);
    }).add;

    s.sync;
    Synth(\VocoderCarrierExample, [\out, carrierBus]);
    Synth(\VocoderModulatorExample, [\out, modulatorBus]);
    Synth.tail(s, \VocoderExample);
};
)
