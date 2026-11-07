// LimutVocoder is a true two-input UGen, not a one-input player insert.
// Allocate distinct carrier/modulator buses and create this node after both
// sources, or use addToTail on a group containing the sources.
(
var carrierBus = Bus.audio(s, 2);
var modulatorBus = Bus.audio(s, 2);
var outBus = 0;

SynthDef(\DexterVocoderCarrier, { |out=0, freq=110, amp=0.2|
    Out.ar(out, (Saw.ar(freq) * amp) ! 2);
}).add;

SynthDef(\DexterVocoderModulator, { |out=0, freq=120, amp=0.2|
    var formant = Formant.ar(freq, 600, 90);
    var syllable = LFPulse.kr(3, 0, 0.4);
    Out.ar(out, (formant * syllable * amp) ! 2);
}).add;

SynthDef(\DexterVocoderOutput, {
    |out=0, inbus=16, modbus=18, mix=1, low=120, high=9000,
     attack=0.005, release=0.08, tilt=0, boost=1|
    var carrier = In.ar(inbus, 2);
    var modulator = In.ar(modbus, 2);
    var processed = LimutVocoder.ar(
        carrier[0], carrier[1], modulator[0], modulator[1],
        mix, low, high, attack, release, tilt, boost);
    Out.ar(out, processed);
}).add;

// In a routine after the SynthDefs are loaded:
// carrier = Synth(\DexterVocoderCarrier, [\out, carrierBus]);
// modulator = Synth(\DexterVocoderModulator, [\out, modulatorBus]);
// vocoder = Synth.tail(s, \DexterVocoderOutput, [
//     \out, outBus, \inbus, carrierBus, \modbus, modulatorBus,
//     \mix, 1, \low, 120, \high, 9000, \attack, 0.005,
//     \release, 0.08, \tilt, 0, \boost, 1
// ]);
// IMPORTANT: free vocoder, carrierBus and modulatorBus when done.
)
