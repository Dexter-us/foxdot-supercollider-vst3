LimutPingPong : MultiOutUGen {
    *ar { |inLeft, inRight, delaytime=0.32, feedback=0.45|
        ^this.multiNew('audio', inLeft, inRight, delaytime, feedback)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}

LimutDuck : MultiOutUGen {
    *ar { |inLeft, inRight, period=0.5, depth=0.7, length=0.12, attack=0.18|
        ^this.multiNew('audio', inLeft, inRight, period, depth, length, attack)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}

LimutDisperser : MultiOutUGen {
    *ar { |inLeft, inRight, frequency=440, q=3, stages=24, mix=0.35|
        ^this.multiNew(
            'audio', inLeft, inRight, frequency, q, stages, mix)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}

LimutFuzz : MultiOutUGen {
    *ar { |inLeft, inRight, driveDb=18, toneHz=9000, mix=0.35|
        ^this.multiNew(
            'audio', inLeft, inRight, driveDb, toneHz, mix)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}
