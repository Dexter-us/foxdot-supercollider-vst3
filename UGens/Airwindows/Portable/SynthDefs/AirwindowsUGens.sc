// Language-side multi-output declarations for offline SynthDef compilation.
// Runtime DSP is provided by AirwindowsFX.scx; no sclang is needed in the DAW.

TapeDelay2 : MultiOutUGen {
    *ar { |inL, inR, time=1.0, regen=0.0, freq=0.5, reso=0.0, flutter=0.0, mix=1.0|
        ^this.multiNew('audio', inL, inR, time, regen, freq, reso, flutter, mix)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}

Echojam : MultiOutUGen {
    *ar { |inL, inR, select=0.5, regen=0.5, speed=0.5, mix=0.5|
        ^this.multiNew('audio', inL, inR, select, regen, speed, mix)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}
