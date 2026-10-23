#include "SC_WorldOptions.h"
#include "SC_Prototypes.h"
#include "SC_HiddenWorld.h"
#include "SC_GraphDef.h"
#include "SC_Group.h"
#include "SC_Graph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: dexter_sc_world_poc <synthdef> <ugen-plugin-directory>\n");
        return 2;
    }

    std::ifstream synthFile(argv[1], std::ios::binary);
    const std::vector<char> synthBytes((std::istreambuf_iterator<char>(synthFile)), {});
    if (synthBytes.empty()) {
        std::fprintf(stderr, "could not read SynthDef\n");
        return 2;
    }

    WorldOptions options;
    options.mRealTime = false; // Process SC blocks in this process; no audio device or server process.
    options.mPreferredSampleRate = 48000;
    options.mBufLength = 64;
    options.mNumInputBusChannels = 0;
    options.mNumOutputBusChannels = 2;
    options.mNumAudioBusChannels = 32;
    options.mLoadGraphDefs = 0;
    options.mRendezvous = false;
    options.mVerbosity = -1;
    options.mUGensPluginPath = argv[2];

    World* world = World_New(&options);
    if (!world) {
        std::fprintf(stderr, "World_New failed\n");
        return 1;
    }
    World_SetSampleRate(world, 48000.0);
    World_Start(world);

    GraphDef* defs = GraphDef_Recv(world, synthBytes.data(), synthBytes.size(), nullptr);
    if (!defs) {
        std::fprintf(stderr, "GraphDef_Recv failed\n");
        World_Cleanup(world, true);
        return 1;
    }
    GraphDef_Define(world, defs);

    int32 synthName[4]{};
    std::memcpy(synthName, "DexterTone", 10);
    GraphDef* def = World_GetGraphDef(world, synthName);
    Group* root = World_GetGroup(world, 0);
    if (!def || !root) {
        std::fprintf(stderr, "SynthDef or root group missing\n");
        World_Cleanup(world, true);
        return 1;
    }

    sc_msg_iter noControls(0, "");
    Graph* synth = nullptr;
    if (Graph_New(world, def, 100, &noControls, &synth, true) != 0 || !synth) {
        std::fprintf(stderr, "Graph_New failed\n");
        World_Cleanup(world, true);
        return 1;
    }
    Group_AddTail(root, &synth->mNode);

    double sumSquares = 0.0;
    float peak = 0.0f;
    int samples = 0;
    for (int block = 0; block < 64; ++block) {
        World_Run(world);
        if (world->mAudioBusTouched[0] == world->mBufCounter) {
            const float* output = world->mAudioBus;
            for (int sample = 0; sample < world->mBufLength; ++sample) {
                sumSquares += double(output[sample]) * double(output[sample]);
                peak = std::max(peak, std::abs(output[sample]));
                ++samples;
            }
        }
        ++world->mBufCounter;
    }

    const double rms = samples ? std::sqrt(sumSquares / samples) : 0.0;
    std::printf("samples=%d rms=%.8f peak=%.8f\n", samples, rms, peak);
    World_Cleanup(world, true);
    return rms > 0.001 ? 0 : 1;
}