// Limut-inspired effect UGens adapted for Dexter U.S. FoxDot.
// Limut-derived adaptations are distributed under CC BY-SA 4.0;
// see ATTRIBUTION.txt and LICENSE-CC-BY-SA-4.0.txt.
#pragma once

#include "SC_PlugIn.h"
#include "LimutFXDSP.hpp"

struct LimutPingPong : public Unit {
    limutfx::PingPongState state;
};

struct LimutDuck : public Unit {
    limutfx::DuckState state;
};

struct LimutDisperser : public Unit {
    limutfx::DisperserState state;
};

struct LimutFuzz : public Unit {
    limutfx::FuzzState state;
};

struct LimutShifter : public Unit {
    limutfx::ShifterState state;
};

struct LimutShimmer : public Unit {
    limutfx::ShimmerState state;
};

struct LimutGatedVerb : public Unit {
    limutfx::GatedVerbState state;
};

struct LimutCompressor : public Unit {
    limutfx::CompressorState state;
};

struct LimutVocoder : public Unit {
    limutfx::VocoderState state;
};

