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

LimutShifter : MultiOutUGen {
    *ar { |inLeft, inRight, mix=0.5, ratio=1.5, window=0.12|
        ^this.multiNew('audio', inLeft, inRight, mix, ratio, window)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}

LimutShimmer : MultiOutUGen {
    *ar { |inLeft, inRight, mix=0.35, ratio=2, window=0.12, room=0.8, damp=0.35|
        ^this.multiNew(
            'audio', inLeft, inRight, mix, ratio, window, room, damp)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}

LimutGatedVerb : MultiOutUGen {
    *ar { |inLeft, inRight, mix=0.65, room=0.75, damp=0.5, threshold=0.02, hold=0.25, attack=0.015, release=0.15|
        ^this.multiNew(
            'audio', inLeft, inRight, mix, room, damp, threshold, hold,
            attack, release)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}

LimutCompressor : MultiOutUGen {
    *ar { |inLeft, inRight, mix=0.5, threshold=(-24), ratioDb=6, attack=0.01, release=0.1, knee=3|
        ^this.multiNew(
            'audio', inLeft, inRight, mix, threshold, ratioDb, attack,
            release, knee)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(2) }
}

LimutVocoder : MultiOutUGen {
    *ar { |carrierLeft, carrierRight, modulatorLeft, modulatorRight, mix=0.75, low=120, high=8000, attack=0.01, release=0.12, tilt=0, boost=1.5|
        ^this.multiNew(
            'audio', carrierLeft, carrierRight, modulatorLeft, modulatorRight,
            mix, low, high, attack, release, tilt, boost)
    }

    init { |...theInputs|
        inputs = theInputs;
        channels = [OutputProxy(rate, this, 0), OutputProxy(rate, this, 1)];
        ^channels
    }

    checkInputs { ^this.checkNInputs(4) }
}
