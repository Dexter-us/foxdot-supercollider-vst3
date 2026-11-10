issing reverb definition " +
                            std::string(reverbNames[type]) + ". Press Run to retry.");
            }
            for (std::size_t player = 0; player < kPlayerCount; ++player) {
                auto*& graph = reverbGraphs_[player][type];
                LifecycleTrace reverbTrace(reverbNames[type], this);
                sc_msg_iter noControls(0, "");
                const auto graphError =
                    Graph_New(world_, reverbDefinition, reverbNodeId++, &noControls,
                              &graph, true);
                if (graphError != 0 || !graph) {
                    std::fprintf(stderr,
                                 "Dexter VST3: Graph_New failed for %s (player %zu, error %d).\n",
                                 reverbNames[type], player,
                                 static_cast<int>(graphError));
                    discardPartialWorld();
                    return fail("SuperCollider startup failed: could not create " +
                                std::string(reverbNames[type]) + " for player " +
                                std::to_string(player + 1) + " (SC error " +
                                std::to_string(static_cast<int>(graphError)) +
                                "). Press Run to retry.");
                }
                const auto bus = kReverbBaseBus + static_cast<int>(player) * kReverbBusStride;
                Graph_SetControl(graph, 0, static_cast<float>(bus));
                Graph_SetControl(graph, 1, static_cast<float>(bus + 2));
                if (type < 2) {
                    Graph_SetControl(graph, 2, 0.5f);
                    Graph_SetControl(graph, 3, 0.5f);
                } else {
                    Graph_SetControl(graph, 2, 50.0f);
                    Graph_SetControl(graph, 3, 3.0f);
                    Graph_SetControl(graph, 4, 0.5f);
                    Graph_SetControl(graph, 5, 15.0f);
                }
                Group_AddTail(rootGroup_, &graph->mNode);
                Node_SetRun(&graph->mNode, 0);
            }
        }
        // Synth voice graphs are created lazily by the script worker. IDs
        // follow the small startup reverb bank and are unique within the world.
        nextDynamicGraphNodeId_ = reverbNodeId;
        cursor_ = kSCBlockSize;
        worldInitializationError_.clear();
        moduleStatus += " User SynthDefs loaded (" +
                        std::to_string(userSynthDefinitions_.size()) + "):";
        for (const auto& definition : userSynthDefinitions_)
            moduleStatus += " " + definition.name;
        moduleStatus += ".";
        std::string bridgeError;
        scLanguageServerPort_ = dexter::sclang::findAvailableServerPort();
        if (scLanguageServerPort_ <= 0 || !scHostBridge_.start(
                scLanguageServerPort_,
                [this](std::vector<char>& packet, std::uint16_t port) {
                    return dispatchHostSuperCollider(packet, port);
                },
                [this] { maintainHostSuperCollider(); }, bridgeError)) {
            scLanguageServerPort_ = 0;
            moduleStatus += " SuperCollider language bridge unavailable: " + bridgeError;
        }
        setEditorStatus("SuperCollider ready. Press Run to evaluate the script. " + moduleStatus);
        return true;
    }

    void discardPartialWorld() {
        scLanguageRuntime_.shutdown();
        scHostBridge_.stop();
        languageSynthDefNames_.clear();
        languageClientPort_ = 0;
        if (world_) {
            World_Cleanup(world_, false);
            world_ = nullptr;
        }
        scLanguageServerPort_ = 0;
        for (auto& voice : audioVoices_)
            voice = {};
        for (auto& player : reverbGraphs_)
            player.fill(nullptr);
        for (auto& playerGraphs : playerEffectGraphs_)
            playerGraphs.fill(nullptr);
        playerEffectTransitions_ = {};
        shapeEffectSmoothed_.fill(0.0f);
        driveEffectSmoothed_.fill(0.0f);
        bitsEffectSmoothed_.fill(0.0f);
        crushEffectSmoothed_.fill(1.0f);
        bitcrusherStates_.fill({});
        for (auto& effectSlots : playerEffectDefinitionIndices_)
            effectSlots.fill(-1);
        for (auto& indices : playerEffectControlEffectIndices_)
            indices.fill(-1);
        for (auto& player : playerEffectControlSmoothed_)
            for (auto& controls : player)
                controls.fill(0.0f);
        synthDefinitions_.fill(nullptr);
        userSynthDefinitions_.clear();
        userSynthFiles_.clear();
        {
            std::lock_guard<std::mutex> lock(editorCatalogMutex_);
            editorExternalSynthNames_.clear();
            editorUserSynthControlSpecs_.clear();
        }
        rootGroup_ = nullptr;
        nextDynamicGraphNodeId_ = 100;
    }

    static std::size_t echoOffset(std::size_t player, std::size_t channel,
                                  std::size_t sample) {
        return (player * 2 + channel) * kMaxEchoBufferSamples + sample;
    }

    void renderSCBlock(std::uint64_t blockStart) {
        // Control modulation is block-rate, interpolated inside the 64-sample
        // block. Note scheduling uses the exact map, not this approximation.
        std::array<double, kPlayerCount> controlBeatStart{};
        std::array<double, kPlayerCount> controlBeatStep{};
        for (std::size_t player = 0; player < kPlayerCount; ++player) {
            const auto& pattern = activePatterns_[player];
            const auto start = patternStartSamples_[player];
            const double elapsed = blockStart >= start
                ? static_cast<double>(blockStart - start) / sampleRate_ : 0.0;
            controlBeatStart[player] = beatAtSeconds(pattern, elapsed);
            const double endBeat = beatAtSeconds(
                pattern, elapsed + static_cast<double>(kSCBlockSize) / sampleRate_);
            controlBeatStep[player] =
                (endBeat - controlBeatStart[player]) / kSCBlockSize;
        }
        for (auto& voice : audioVoices_) {
            if (!voice.activeSynth || (voice.midiNote < 0 && !voice.releasing) ||
                voice.playerIndex < 0 ||
                voice.playerIndex >= static_cast<int>(kPlayerCount))
                continue;
            const auto player = static_cast<std::size_t>(voice.playerIndex);
            const auto& pattern = activePatterns_[player];
            const bool isUserSynth =
                static_cast<std::size_t>(voice.synthIndex) >=
                kSynthIndexToGraphIndex.size();
            const bool isPlayerEffect =
                isPersistentPlayerEffectSynthIndex(voice.synthIndex);
            if (isUserSynth && !isPlayerEffect) {
                const auto userIndex = static_cast<std::size_t>(voice.synthIndex) -
                                       kSynthIndexToGraphIndex.size();
                if (userIndex >= userSynthDefinitions_.size())
                    continue;
                const auto& definition = userSynthDefinitions_[userIndex];
                if (definition.frequencyControl >= 0)
                    Graph_SetControl(voice.activeSynth, definition.frequencyControl,
                                     voice.frequencyHz);
                if (definition.amplitudeControl >= 0)
                    Graph_SetControl(voice.activeSynth, definition.amplitudeControl, 1.0f);
                if (definition.panControl >= 0)
                    Graph_SetControl(voice.activeSynth, definition.panControl, 0.0f);
                if (definition.gateControl >= 0 && !voice.userSynthGateReleased &&
                    blockStart >= voice.sustainEndSample) {
                    Graph_SetControl(voice.activeSynth, definition.gateControl, 0.0f);
                    voice.userSynthGateReleased = true;
                }
                if (definition.liveControlCount > 0) {
                    const double blockSeconds =
                        static_cast<double>(kSCBlockSize) / sampleRate_;
                    const bool resetSmoothing =
                        !voice.userSynthControlSmoothingInitialized ||
                        voice.userSynthControlDefinitionIndex != userIndex;
                    for (std::size_t control = 0;
                         control < definition.liveControlCount; ++control) {
                        const bool hasOverride =
                            pattern.userSynthControlDefinitionIndex ==
                                static_cast<int>(userIndex) &&
                            (pattern.userSynthControlOverrideMask &
                             static_cast<std::uint8_t>(1u << control)) != 0;
                        const float target = hasOverride
                            ? pattern.userSynthControlValues[control]
                            : definition.liveControls[control].defaultNormalized();
                        auto& current =
                            voice.userSynthControlSmoothed[control];
                        current = resetSmoothing
                            ? target
                            : smoothAirwindowsFxControl(current, target,
                                                        blockSeconds);
                        const auto& spec = definition.liveControls[control];
                        Graph_SetControl(
                            voice.activeSynth, spec.controlIndex,
                            spec.map(current));
                    }
                    voice.userSynthControlDefinitionIndex =
                        static_cast<int>(userIndex);
                    voice.userSynthControlSmoothingInitialized = true;
                } else {
                    voice.userSynthControlDefinitionIndex = -1;
                    voice.userSynthControlSmoothingInitialized = false;
                }
            } else if (voice.synthIndex >= 31 && !isPlayerEffect) {
                const double beat = controlBeatStart[player];
                Graph_SetControl(voice.activeSynth, 10,
                    pattern.editorAmpOverride
                        ? editorAmplitudeCurrent_[player]
                        : evaluateTimeVar(pattern.ampControl, beat,
                                          pattern.amplitude));
                Graph_SetControl(voice.activeSynth, 18,
                    pattern.editorCutoffOverride
                        ? editorCutoffCurrent_[player]
                        : evaluateTimeVar(pattern.cutoffControl, beat,
                                          pattern.cutoffHz));
            }
            if (!isUserSynth || isPlayerEffect) {
                const float level = reverbEnabled(pattern)
                    ? voiceEnvelopeAt(voice, blockStart) *
                          (static_cast<float>(voice.velocity) / 127.0f) * voice.amplitude
                    : 0.0f;
                Graph_SetControl(voice.activeSynth, 7, level);
                Graph_SetControl(voice.activeSynth, 8, voice.cutoffHz);
            }
        }
        World_Run(world_);
        const auto eventEffectBusLength =
            static_cast<std::size_t>(world_->mBufLength);
        for (std::size_t voiceIndex = 0;
             voiceIndex < audioVoices_.size(); ++voiceIndex) {
            auto& voice = audioVoices_[voiceIndex];
            if (!voice.eventEffectGraph ||
                (!voice.releasing && voice.midiNote < 0))
                continue;
            const auto sourceBus = voice.stereoUserSynth
                ? static_cast<std::size_t>(kStereoSynthOutputBaseBus) +
                      voiceIndex * 2
                : static_cast<std::size_t>(kFirstAudioBus) +
                      voiceIndex * static_cast<std::size_t>(kAudioBusStride);
            const auto inputBus =
                static_cast<std::size_t>(kEventEffectInputBusBase) +
                voiceIndex * 2;
            for (int sampleIndex = 0; sampleIndex < kSCBlockSize; ++sampleIndex) {
                const auto sample = static_cast<std::size_t>(sampleIndex);
                const auto sourceOffset = sourceBus * eventEffectBusLength + sample;
                const float left = voice.eventEffectTail.active
                    ? 0.0f : world_->mAudioBus[sourceOffset];
                const float right = voice.eventEffectTail.active
                    ? 0.0f
                    : voice.stereoUserSynth
                          ? world_->mAudioBus[(sourceBus + 1) *
                                                  eventEffectBusLength +
                                              sample]
                          : left;
                world_->mAudioBus[inputBus * eventEffectBusLength + sample] =
                    left;
                world_->mAudioBus[(inputBus + 1) * eventEffectBusLength +
                                  sample] = right;
            }
            world_->mAudioBusTouched[inputBus] = world_->mBufCounter;
            world_->mAudioBusTouched[inputBus + 1] = world_->mBufCounter;
            Graph_Calc(voice.eventEffectGraph);
            if (voice.eventEffectTail.active) {
                const auto outputBus =
                    static_cast<std::size_t>(kEventEffectOutputBusBase) +
                    voiceIndex * 2;
                double energy = 0.0;
                for (int sampleIndex = 0; sampleIndex < kSCBlockSize;
                     ++sampleIndex) {
                    const auto sample = static_cast<std::size_t>(sampleIndex);
                    const float left = world_->mAudioBus[
                        outputBus * eventEffectBusLength + sample];
                    const float right = world_->mAudioBus[
                        (outputBus + 1) * eventEffectBusLength + sample];
                    energy += static_cast<double>(left) * left +
                              static_cast<double>(right) * right;
                }
                const float rms = static_cast<float>(
                    std::sqrt(energy / (2.0 * kSCBlockSize)));
                dexter::updateEventEffectTail(
                    voice.eventEffectTail, rms, blockStart, kSCBlockSize);
            }
        }
        diskIO_.renderBlock();
        for (auto& voice : audioVoices_) {
            if (voice.triggerControlPendingReset && voice.activeSynth) {
                Graph_SetControl(voice.activeSynth, 9, 0.0f);
                voice.triggerControlPendingReset = false;
            }
        }
        std::array<std::array<float, kSCBlockSize>, kPlayerCount> playerLeftBlock{};
        std::array<std::array<float, kSCBlockSize>, kPlayerCount> playerRightBlock{};
        std::array<std::array<float, kSCBlockSize>, kPlayerCount> playerMuteGainBlock{};
        std::array<std::array<bool, kSCBlockSize>, kPlayerCount> playerHasVoiceBlock{};
        std::array<std::size_t, kSCBlockSize> contributingVoicesBlock{};
        const float mixSmoothing = std::exp(
            -1.0f / static_cast<float>(sampleRate_ * kMixSmoothingSeconds));
        const float mouseSmoothing = 1.0f - mixSmoothing;
        for (int sampleIndex = 0; sampleIndex < kSCBlockSize; ++sampleIndex) {
            const auto absoluteSample = blockStart + static_cast<std::uint64_t>(sampleIndex);
            for (std::size_t player = 0; player < kPlayerCount; ++player) {
                const float target = mutedPlayers_[player] ? 0.0f : 1.0f;
                playerMuteGain_[player] += mouseSmoothing * (target - playerMuteGain_[player]);
                playerMuteGainBlock[player][static_cast<std::size_t>(sampleIndex)] =
                    playerMuteGain_[player];
                editorAmplitudeCurrent_[player] =
                    smoothEditorPlayerControl(
                        editorAmplitudeCurrent_[player],
                        activePatterns_[player].amplitude,
                        mouseSmoothing);
                editorCutoffCurrent_[player] = smoothEditorPlayerControl(
                    editorCutoffCurrent_[player],
                    activePatterns_[player].cutoffHz, mouseSmoothing);
            }
            std::array<float, kPlayerCount> playerLeft{};
            std::array<float, kPlayerCount> playerRight{};
            std::array<bool, kPlayerCount> playerHasVoice{};
            std::size_t contributingVoices = 0;
            for (std::size_t voiceIndex = 0; voiceIndex < audioVoices_.size(); ++voiceIndex) {
                auto& voice = audioVoices_[voiceIndex];
                const bool eventEffectTail = voice.eventEffectTail.active;
                const float envelope = eventEffectTail
                    ? 1.0f : voiceEnvelopeAt(voice, absoluteSample);
                if (envelope <= 0.0f || voice.velocity <= 0 || voice.playerIndex < 0 ||
                    voice.playerIndex >= static_cast<int>(kPlayerCount))
                    continue;
                const auto bus = static_cast<std::size_t>(kFirstAudioBus) +
                                 voiceIndex * static_cast<std::size_t>(kAudioBusStride);
                const auto busLength = static_cast<std::size_t>(world_->mBufLength);
                const auto offset = bus * busLength + static_cast<std::size_t>(sampleIndex);
                float source = world_->mAudioBus[offset];
                float sourceRight = source;
                const bool stereoOutput =
                    voice.stereoUserSynth || voice.eventEffectGraph != nullptr;
                if (voice.eventEffectGraph) {
                    const auto effectBus =
                        static_cast<std::size_t>(kEventEffectOutputBusBase) +
                        voiceIndex * 2;
                    source = world_->mAudioBus[
                        effectBus * busLength +
                        static_cast<std::size_t>(sampleIndex)];
                    sourceRight = world_->mAudioBus[
                        (effectBus + 1) * busLength +
                        static_cast<std::size_t>(sampleIndex)];
                } else if (voice.stereoUserSynth) {
                    const auto stereoBus =
                        static_cast<std::size_t>(kStereoSynthOutputBaseBus) +
                        voiceIndex * 2;
                    source = world_->mAudioBus[
                        stereoBus * busLength +
                        static_cast<std::size_t>(sampleIndex)];
                    sourceRight = world_->mAudioBus[
                        (stereoBus + 1) * busLength +
                        static_cast<std::size_t>(sampleIndex)];
                }
                const auto controlPlayerIndex = static_cast<std::size_t>(voice.playerIndex);
                const auto& livePattern = activePatterns_[controlPlayerIndex];
                const double beat = controlBeatStart[controlPlayerIndex] +
                    controlBeatStep[controlPlayerIndex] * sampleIndex;
                if (livePattern.editorAmpOverride) {
                    voice.amplitude = foxDotAmplitude(
                        voice.synthIndex,
                        editorAmplitudeCurrent_[controlPlayerIndex]);
                } else if (livePattern.ampControl.active &&
                    (voice.synthIndex < 31 ||
                     static_cast<std::size_t>(voice.synthIndex) >=
                         kSynthIndexToGraphIndex.size())) {
                    const float patternAmp = evaluateTimeVar(livePattern.ampControl, beat,
                                                             livePattern.amplitude);
                    const float targetAmp = foxDotAmplitude(voice.synthIndex, patternAmp);
                    if (livePattern.ampControl.linear)
                        voice.amplitude = targetAmp;
                    else
                        voice.amplitude += mouseSmoothing * (targetAmp - voice.amplitude);
                }
                if (voice.mousePan && !livePattern.editorPanOverride) {
                    const float mouseX = std::clamp(
                        world_->mAudioBus[(bus + 1) * busLength + static_cast<std::size_t>(sampleIndex)],
                        0.0f, 1.0f);
                    const float targetPan = mouseX * 2.0f - 1.0f;
                    voice.pan += mouseSmoothing * (targetPan - voice.pan);
                    voice.leftGain = std::sqrt(std::max(0.0f, (1.0f - voice.pan) * 0.5f));
                    voice.rightGain = std::sqrt(std::max(0.0f, (1.0f + voice.pan) * 0.5f));
                } else if (livePattern.editorPanOverride) {
                    voice.pan += mouseSmoothing *
                        (livePattern.editorPan - voice.pan);
                    voice.leftGain = std::sqrt(
                        std::max(0.0f, (1.0f - voice.pan) * 0.5f));
                    voice.rightGain = std::sqrt(
                        std::max(0.0f, (1.0f + voice.pan) * 0.5f));
                } else if (livePattern.panControl.active) {
                    const float targetPan = evaluateTimeVar(livePattern.panControl, beat,
                                                            livePattern.pan);
                    if (livePattern.panControl.linear)
                        voice.pan = targetPan;
                    else
                        voice.pan += mouseSmoothing * (targetPan - voice.pan);
                    voice.leftGain = std::sqrt(std::max(0.0f, (1.0f - voice.pan) * 0.5f));
                    voice.rightGain = std::sqrt(std::max(0.0f, (1.0f + voice.pan) * 0.5f));
                }
                bool dynamicCutoff = voice.mouseCutoff;
                float targetCutoff = voice.cutoffHz;
                if (voice.mouseCutoff && !livePattern.editorCutoffOverride) {
                    const float mouseY = std::clamp(
                        world_->mAudioBus[(bus + 2) * busLength + static_cast<std::size_t>(sampleIndex)],
                        0.0f, 1.0f);
                    targetCutoff = 200.0f + mouseY * 7800.0f;
                } else if (livePattern.editorCutoffOverride) {
                    targetCutoff = livePattern.editorCutoffHz;
                    dynamicCutoff = true;
                } else if (livePattern.cutoffControl.active) {
                    targetCutoff = evaluateTimeVar(livePattern.cutoffControl, beat,
                                                   livePattern.cutoffHz);
                    dynamicCutoff = true;
                }
                if (dynamicCutoff) {
                    if (!livePattern.cutoffControl.linear || voice.mouseCutoff)
                        voice.cutoffHz += mouseSmoothing * (targetCutoff - voice.cutoffHz);
                    else
                        voice.cutoffHz = targetCutoff;
                    const double maxCutoff = sampleRate_ * 0.45;
                    const double clampedCutoff = std::min<double>(voice.cutoffHz, maxCutoff);
                    voice.filterCoefficient = clampedCutoff >= maxCutoff
                        ? 1.0f
                        : static_cast<float>(1.0 - std::exp(-2.0 * kPi * clampedCutoff / sampleRate_));
                }
                // New source graphs own cutoff filtering; do not apply a
                // second plugin-only low-pass at the same cutoff.
                if (voice.synthIndex >= 31 &&
                    static_cast<std::size_t>(voice.synthIndex) <
                        kSynthIndexToGraphIndex.size())
                    voice.filterState = source;
                else
                    voice.filterState += voice.filterCoefficient * (source - voice.filterState);
                if (stereoOutput)
                    voice.filterStateRight += voice.filterCoefficient *
                        (sourceRight - voice.filterStateRight);
                const float velocity = static_cast<float>(voice.velocity) / 127.0f;
                auto updateFilterControl = [&](float& current, const TimeVarControl& control, float fallback) {
                    if (!control.active) return;
                    const float target = evaluateTimeVar(control, beat, fallback);
                    if (control.linear) current = target;
                    else current += mouseSmoothing * (target - current);
                };
                updateFilterControl(voice.lpfHz, livePattern.lpfControl, livePattern.lpfHz);
                updateFilterControl(voice.lpr, livePattern.lprControl, livePattern.lpr);
                updateFilterControl(voice.hpfHz, livePattern.hpfControl, livePattern.hpfHz);
                updateFilterControl(voice.hpr, livePattern.hprControl, livePattern.hpr);
                updateFilterControl(voice.bpfHz, livePattern.bpfControl, livePattern.bpfHz);
                updateFilterControl(voice.bpr, livePattern.bprControl, livePattern.bpr);
                updateFilterControl(voice.bpnoise, livePattern.bpnoiseControl,
                                    livePattern.bpnoise);
                updateFilterControl(voice.swell, livePattern.swellControl,
                                    livePattern.swell);
                if (sampleIndex == 0) {
                    const float controlSmoothing = static_cast<float>(
                        1.0 - std::exp(-static_cast<double>(kSCBlockSize) /
                                       (0.01 * sampleRate_)));
                    float targetBpfHz = voice.bpfHz;
                    float targetBpr = voice.bpr;
                    if (voice.bpnoise > 0.0f && voice.bpfHz > 0.0f) {
                        const double sustainSeconds = std::max(
                            0.005, static_cast<double>(voice.endSample -
                                                       voice.startSample) /
                                       sampleRate_);
                        const double noiseRateHz = voice.bpnoise / sustainSeconds;
                        targetBpfHz *= std::exp2(
                            advanceNoise1(voice.bpfNoise, noiseRateHz));
                        targetBpr *= std::exp2(
                            advanceNoise1(voice.bprNoise, noiseRateHz));
                        targetBpr = std::clamp(targetBpr, 0.05f, 1.0f);
                        voice.modulatedBpfHz += controlSmoothing *
                                                (targetBpfHz - voice.modulatedBpfHz);
                        voice.modulatedBpr += controlSmoothing *
                                              (targetBpr - voice.modulatedBpr);
                    } else {
                        voice.modulatedBpfHz = targetBpfHz;
                        voice.modulatedBpr = targetBpr;
                    }
                    const float swellTarget = swellEnvelopeAt(voice, absoluteSample) *
                                              voice.swell * 2000.0f;
                    voice.swellCutoffHz += controlSmoothing *
                                           (swellTarget - voice.swellCutoffHz);
                    configureBiquad(voice.lpfState, voice.lpfHz, voice.lpr, 0);
                    configureBiquad(voice.hpfState, voice.hpfHz, voice.hpr, 1);
                    configureBiquad(voice.bpfState, voice.modulatedBpfHz,
                                    voice.modulatedBpr, 2);
                    configureBiquad(voice.swellState, voice.swellCutoffHz,
                                    voice.hpr, 1);
                    if (stereoOutput) {
                        configureBiquad(voice.lpfRightState, voice.lpfHz,
                                        voice.lpr, 0);
                        configureBiquad(voice.hpfRightState, voice.hpfHz,
                                        voice.hpr, 1);
                        configureBiquad(voice.bpfRightState,
                                        voice.modulatedBpfHz,
                                        voice.modulatedBpr, 2);
                        configureBiquad(voice.swellRightState,
                                        voice.swellCutoffHz, voice.hpr, 1);
                    }
                }
                float shaped = processBiquad(voice.lpfState, voice.filterState);
                shaped = processBiquad(voice.hpfState, shaped);
                shaped = processBiquad(voice.bpfState, shaped);
                shaped = processBiquad(voice.swellState, shaped);
                float shapedRight = shaped;
                if (stereoOutput) {
                    shapedRight = processBiquad(voice.lpfRightState,
                                                voice.filterStateRight);
                    shapedRight = processBiquad(voice.hpfRightState,
                                                shapedRight);
                    shapedRight = processBiquad(voice.bpfRightState,
                                                shapedRight);
                    shapedRight = processBiquad(voice.swellRightState,
                                                shapedRight);
                }
                if (voice.formant >= 0.0f)
                    shaped = dexter::foxDotFormantSample(
                        voice.formantState, shaped, voice.formant, sampleRate_);
                if (stereoOutput && voice.formant >= 0.0f)
                    shapedRight = dexter::foxDotFormantSample(
                        voice.formantStateRight, shapedRight, voice.formant,
                        sampleRate_);
                const double effectDuration =
                    voice.sustainEndSample > voice.startSample
                    ? static_cast<double>(voice.sustainEndSample -
                                          voice.startSample)
                    : 0.0;
                const double effectProgress = !eventEffectTail &&
                    effectDuration > 0.0
                    ? static_cast<double>(absoluteSample - voice.startSample) /
                          effectDuration
                    : 1.0;
                const float noteEffectGain = eventEffectTail ? 1.0f :
                    dexter::foxDotNoteEffectGain(
                        effectProgress, voice.slide, voice.slideFrom,
                        voice.slideDelay, voice.bend, voice.bendDelay);
                const float chopGain = eventEffectTail ? 1.0f :
                    dexter::foxDotChopGain(effectProgress, voice.chop);
                const float coarseGain = eventEffectTail ? 1.0f :
                    dexter::foxDotCoarseGain(effectProgress, voice.coarse);
                const double elapsedSeconds =
                    static_cast<double>(absoluteSample - voice.startSample) /
                    sampleRate_;
                auto spinGains = dexter::foxDotSpinGains(
                    elapsedSeconds, voice.spin);
                if (eventEffectTail)
                    spinGains = {1.0f, 1.0f};
                const auto player = static_cast<std::size_t>(voice.playerIndex);
                const float noteGain = envelope * velocity * voice.amplitude *
                    noteEffectGain * chopGain * coarseGain;
                const float amplitude = shaped * noteGain;
                if (stereoOutput) {
                    const auto balance = dexter::stereoBalanceGains(voice.pan);
                    playerLeft[player] += amplitude * balance[0] *
                        spinGains.left;
                    playerRight[player] += shapedRight * noteGain * balance[1] *
                        spinGains.right;
                } else {
                    playerLeft[player] += amplitude * voice.leftGain *
                        spinGains.left;
                    playerRight[player] += amplitude * voice.rightGain *
                        spinGains.right;
                }
                playerHasVoice[player] = playerMuteGain_[player] > 1e-4f;
                if (playerMuteGain_[player] > 1e-4f)
                    ++contributingVoices;
            }

            for (std::size_t player = 0; player < kPlayerCount; ++player) {
                playerLeftBlock[player][static_cast<std::size_t>(sampleIndex)] =
                    playerLeft[player];
                playerRightBlock[player][static_cast<std::size_t>(sampleIndex)] =
                    playerRight[player];
                playerHasVoiceBlock[player][static_cast<std::size_t>(sampleIndex)] =
                    playerHasVoice[player];
            }
            contributingVoicesBlock[static_cast<std::size_t>(sampleIndex)] =
                contributingVoices;
        }

        // Native FoxDot-compatible player inserts run on the summed player
        // signal, before optional user SynthDef effects. Modulated controls
        // are smoothed sample-by-sample to avoid zipper noise.
        for (std::size_t player = 0; player < kPlayerCount; ++player) {
            const auto& pattern = activePatterns_[player];
            auto& shape = shapeEffectSmoothed_[player];
            auto& drive = driveEffectSmoothed_[player];
            auto& bits = bitsEffectSmoothed_[player];
            auto& crush = crushEffectSmoothed_[player];
            auto& bitcrusher = bitcrusherStates_[player];
            const float bitcrusherSmoothing = static_cast<float>(
                1.0 - std::exp(-1.0 / (0.01 * sampleRate_)));
            for (int sampleIndex = 0; sampleIndex < kSCBlockSize; ++sampleIndex) {
                const double beat = controlBeatStart[player] +
                    controlBeatStep[player] * sampleIndex;
                const float targetShape = evaluateTimeVar(
                    pattern.shapeControl, beat, pattern.shape);
                const float targetDrive = evaluateTimeVar(
                    pattern.driveControl, beat, pattern.drive);
                const float targetBits = evaluateTimeVar(
                    pattern.bitsControl, beat, pattern.bits);
                const float targetCrush = evaluateTimeVar(
                    pattern.crushControl, beat, pattern.crush);
                shape = dexter::smoothPlayerEffectControl(
                    shape, targetShape, mouseSmoothing);
                drive = dexter::smoothPlayerEffectControl(
                    drive, targetDrive, mouseSmoothing);
                bits = dexter::smoothPlayerEffectControl(
                    bits, targetBits, bitcrusherSmoothing);
                crush = dexter::smoothPlayerEffectControl(
                    crush, targetCrush, bitcrusherSmoothing);
                auto& left = playerLeftBlock[player][
                    static_cast<std::size_t>(sampleIndex)];
                auto& right = playerRightBlock[player][
                    static_cast<std::size_t>(sampleIndex)];
                left = dexter::processFoxDotDrive(
                    dexter::processFoxDotShape(left, shape), drive);
                right = dexter::processFoxDotDrive(
                    dexter::processFoxDotShape(right, shape), drive);
                const auto crushed = dexter::processFoxDotBitcrusher(
                    bitcrusher, left, right, bits, crush, sampleRate_,
                    bitcrusherSmoothing);
                left = crushed[0];
                right = crushed[1];
            }
        }

        const auto busLength = static_cast<std::size_t>(world_->mBufLength);
        for (std::size_t player = 0; player < kPlayerCount; ++player) {
            const auto& pattern = activePatterns_[player];
            const bool effectSelected = pattern.active &&
                pattern.playerEffectCount > 0;
            const auto inputBus = static_cast<std::size_t>(kAirwindowsInputBusBase) +
                                  player * 2;
            for (int channel = 0; channel < 2; ++channel) {
                const auto bus = inputBus + static_cast<std::size_t>(channel);
                auto* destination = world_->mAudioBus + bus * busLength;
                const auto& source = channel == 0
                    ? playerLeftBlock[player] : playerRightBlock[player];
                for (int sampleIndex = 0; sampleIndex < kSCBlockSize; ++sampleIndex)
                    destination[sampleIndex] = effectSelected
                        ? source[static_cast<std::size_t>(sampleIndex)] : 0.0f;
                world_->mAudioBusTouched[bus] = world_->mBufCounter;
            }
            for (std::size_t slot = 0; slot < kMaxPlayerEffectChain; ++slot) {
                auto* graph = playerEffectGraphs_[player][slot];
                if (!graph)
                    continue;
                const bool slotSelected = effectSelected &&
                    slot < pattern.playerEffectCount &&
                    pattern.playerEffects[slot].userSynthIndex ==
                        playerEffectDefinitionIndices_[player][slot];
                if (!slotSelected)
                    playerEffectControlEffectIndices_[player][slot] = -1;
                if (slotSelected) {
                    const auto userIndex = static_cast<std::size_t>(
                        pattern.playerEffects[slot].userSynthIndex);
                    if (userIndex >= userSynthDefinitions_.size()) {
                        playerEffectControlEffectIndices_[player][slot] = -1;
                        continue;
                    }
                    const auto& definition = userSynthDefinitions_[userIndex];
                    const bool newEffect =
                        playerEffectControlEffectIndices_[player][slot] !=
                        pattern.playerEffects[slot].userSynthIndex;
                    const double effectBeat = controlBeatStart[player] +
                        controlBeatStep[player] * (kSCBlockSize * 0.5);
                    const float blockSeconds =
                        static_cast<float>(kSCBlockSize / sampleRate_);
                    for (std::size_t control = 0;
                         control < definition.effectControlIndices.size(); ++control) {
                        const auto controlIndex =
                            definition.effectControlIndices[control];
                        if (controlIndex < 0)
                            continue;
                        const float normalizedTarget = evaluateTimeVar(
                            pattern.playerEffects[slot].controlVariables[control],
                            effectBeat,
                            static_cast<float>(
                                pattern.playerEffects[slot].controlValues[control]));
                        auto& current =
                            playerEffectControlSmoothed_[player][slot][control];
                        current = newEffect ? normalizedTarget :
                            smoothAirwindowsFxControl(current, normalizedTarget,
                                                      blockSeconds);
                        const float mapped =
                            definition.effectControlMinimum[control] +
                            current * (definition.effectControlMaximum[control] -
                                       definition.effectControlMinimum[control]);
                        Graph_SetControl(graph, controlIndex, mapped);
                    }
                    playerEffectControlEffectIndices_[player][slot] =
                        pattern.playerEffects[slot].userSynthIndex;
                }
                // The paused insert graph is advanced once after its source or
                // previous insert's return bus is ready.
                Graph_Calc(graph);
            }
        }

        std::array<std::size_t, kPlayerCount> effectGraphCounts{};
        std::array<bool, kPlayerCount> selectedEffectChains{};
        for (std::size_t player = 0; player < kPlayerCount; ++player) {
            auto& count = effectGraphCounts[player];
            while (count < kMaxPlayerEffectChain &&
                   playerEffectGraphs_[player][count])
                ++count;
            const auto& pattern = activePatterns_[player];
            bool selected = pattern.active && pattern.playerEffectCount > 0 &&
                pattern.playerEffectCount == count;
            std::uint64_t signature = 0;
            for (std::size_t slot = 0; slot < count; ++slot) {
                selected &= playerEffectDefinitionIndices_[player][slot] ==
                    pattern.playerEffects[slot].userSynthIndex;
                signature |= static_cast<std::uint64_t>(
                    playerEffectDefinitionIndices_[player][slot] + 1) << (slot * 16);
            }
            selectedEffectChains[player] = selected;
            playerEffectTransitions_[player].select(
                selected ? signature : 0, sampleRate_);
        }
        for (int sampleIndex = 0; sampleIndex < kSCBlockSize; ++sampleIndex) {
            const auto absoluteSample = blockStart + static_cast<std::uint64_t>(sampleIndex);
            std::array<float, kPlayerCount> playerLeft{};
            std::array<float, kPlayerCount> playerRight{};
            std::array<bool, kPlayerCount> playerHasVoice{};
            for (std::size_t player = 0; player < kPlayerCount; ++player) {
                playerLeft[player] =
                    playerLeftBlock[player][static_cast<std::size_t>(sampleIndex)];
                playerRight[player] =
                    playerRightBlock[player][static_cast<std::size_t>(sampleIndex)];
                playerHasVoice[player] =
                    playerHasVoiceBlock[player][static_cast<std::size_t>(sampleIndex)];
                playerMuteGain_[player] =
                    playerMuteGainBlock[player][static_cast<std::size_t>(sampleIndex)];
            }
            std::size_t contributingVoices =
                contributingVoicesBlock[static_cast<std::size_t>(sampleIndex)];
            float leftMixed = 0.0f;
            float rightMixed = 0.0f;
            const auto writeIndex = static_cast<std::size_t>(absoluteSample % kMaxEchoBufferSamples);
            std::size_t echoTailPlayers = 0;
            std::size_t reverbTailPlayers = 0;
            std::size_t effectTailPlayers = 0;
            for (std::size_t player = 0; player < kPlayerCount; ++player) {
                const auto& pattern = activePatterns_[player];
                const auto effectCount = effectGraphCounts[player];
                const bool hasEffectGraph = effectCount > 0;
                const bool effectSelected = selectedEffectChains[player];
                const auto effectOutputBus =
                    static_cast<std::size_t>(playerEffectChainOutputBus(
                        player, hasEffectGraph ? effectCount - 1 : 0,
                        kAirwindowsOutputBusBase));
                const float effectLeft = hasEffectGraph
                    ? world_->mAudioBus[effectOutputBus * busLength +
                                        static_cast<std::size_t>(sampleIndex)]
                    : 0.0f;
                const float effectRight = hasEffectGraph
                    ? world_->mAudioBus[(effectOutputBus + 1) * busLength +
                                        static_cast<std::size_t>(sampleIndex)]
                    : 0.0f;
                const auto routed = playerEffectTransitions_[player].process(
                    effectSelected ? effectLeft : playerLeft[player] + effectLeft,
                    effectSelected ? effectRight : playerRight[player] + effectRight);
                const float dryLeft = routed[0];
                const float dryRight = routed[1];
                if (hasEffectGraph && !playerHasVoice[player] &&
                    playerMuteGain_[player] > 1e-4f &&
                    (std::abs(effectLeft) > 1e-6f ||
                     std::abs(effectRight) > 1e-6f))
                    ++effectTailPlayers;
                const float targetReverbMix = reverbEnabled(pattern) ? pattern.reverbMix : 0.0f;
                auto& reverbMix = reverbMixSmoothed_[player];
                reverbMix += (targetReverbMix - reverbMix) * mixSmoothing;
                const auto reverbBus = static_cast<std::size_t>(kReverbBaseBus) +
                                       player * static_cast<std::size_t>(kReverbBusStride);
                const auto reverbOffset = static_cast<std::size_t>(sampleIndex);
                const float wetLeft = world_->mAudioBus[(reverbBus + 2) * busLength + reverbOffset];
                const float wetRight = world_->mAudioBus[(reverbBus + 3) * busLength + reverbOffset];
                if (!playerHasVoice[player] && playerMuteGain_[player] > 1e-4f && reverbMix > 1e-5f &&
                    (std::abs(wetLeft) > 1e-6f || std::abs(wetRight) > 1e-6f))
                    ++reverbTailPlayers;
                const float reverbedLeft = dryLeft * (1.0f - reverbMix) + wetLeft * reverbMix;
                const float reverbedRight = dryRight * (1.0f - reverbMix) + wetRight * reverbMix;
                const double playerBeat = controlBeatStart[player] +
                    controlBeatStep[player] * sampleIndex;
                const double playerBpm = tempoAtBeat(pattern, playerBeat);
                auto& echoMix = echoMixSmoothed_[player];
                const float echoBeats = evaluateTimeVar(pattern.echoControl, playerBeat,
                                                        static_cast<float>(pattern.echoBeats));
                const float requestedDelayTime = evaluateTimeVar(pattern.delayControl, playerBeat,
                                                                  static_cast<float>(pattern.delayTimeSeconds));
                const float targetEchoMix = (echoBeats > 0.0f || requestedDelayTime > 0.0f)
                    ? evaluateTimeVar(pattern.echoMixControl, playerBeat, pattern.echoMix)
                    : 0.0f;
                echoMix += (targetEchoMix - echoMix) * mixSmoothing;
                auto& delaySeconds = echoDelaySmoothed_[player];
                const float targetDelaySeconds = requestedDelayTime > 0.0f
                    ? requestedDelayTime
                    : static_cast<float>(echoBeats * 60.0 / playerBpm);
                delaySeconds += (targetDelaySeconds - delaySeconds) * mixSmoothing;
                if (sampleIndex % kSCBlockSize == 0) {
                    const float decayBeats = evaluateTimeVar(pattern.echoDecayControl, playerBeat,
                                                            static_cast<float>(pattern.echoDecayBeats));
                    const double decaySeconds =
                        static_cast<double>(decayBeats) * 60.0 / playerBpm;
                    const float requestedFeedback = evaluateTimeVar(pattern.echoFeedbackControl,
                                                                     playerBeat, pattern.echoFeedback);
                    echoFeedbackTarget_[player] = requestedFeedback >= 0.0f
                        ? std::clamp(requestedFeedback, 0.0f, 0.95f)
                        : (decaySeconds > 0.0 && delaySeconds > 0.0f
                            ? std::pow(0.001f, static_cast<float>(delaySeconds / decaySeconds))
                            : 0.0f);
                }
                auto& feedback = echoFeedbackSmoothed_[player];
                feedback += (echoFeedbackTarget_[player] - feedback) * mixSmoothing;
                if (echoMix > 1e-5f && delaySeconds > 0.0f && !echoBuffer_.empty()) {
                    const auto requestedDelay = std::round(delaySeconds * sampleRate_);
                    const auto delaySamples = static_cast<std::size_t>(
                        std::max(1.0, std::min<double>(kMaxEchoBufferSamples - 1,
                                                       requestedDelay)));
                    const auto readIndex = (writeIndex + kMaxEchoBufferSamples - delaySamples) %
                                           kMaxEchoBufferSamples;
                    const auto leftOffset = echoOffset(player, 0, readIndex);
                    const auto rightOffset = echoOffset(player, 1, readIndex);
                    const float delayedLeft = echoBuffer_[leftOffset];
                    const float delayedRight = echoBuffer_[rightOffset];
                    if (!playerHasVoice[player] && playerMuteGain_[player] > 1e-4f &&
                        (std::abs(delayedLeft) > 1e-6f || std::abs(delayedRight) > 1e-6f))
                        ++echoTailPlayers;
                    leftMixed += (reverbedLeft + delayedLeft * echoMix) * playerMuteGain_[player];
                    rightMixed += (reverbedRight + delayedRight * echoMix) * playerMuteGain_[player];
                    echoBuffer_[echoOffset(player, 0, writeIndex)] =
                        reverbedLeft + delayedLeft * feedback;
                    echoBuffer_[echoOffset(player, 1, writeIndex)] =
                        reverbedRight + delayedRight * feedback;
                } else {
                    leftMixed += reverbedLeft * playerMuteGain_[player];
                    rightMixed += reverbedRight * playerMuteGain_[player];
                    echoBuffer_[echoOffset(player, 0, writeIndex)] = 0.0f;
                    echoBuffer_[echoOffset(player, 1, writeIndex)] = 0.0f;
                }
            }

            const auto mixContributors = contributingVoices + echoTailPlayers +
                                         reverbTailPlayers + effectTailPlayers;
            const float targetMixGain = mixContributors == 0
                                            ? 1.0f
                                            : 1.0f / static_cast<float>(mixContributors);
            mixGain_ += (targetMixGain - mixGain_) * mixSmoothing;
            // The language tab writes normal SC hardware-output buses 0/1.
            // FoxDot uses private buses; include each direct bus only when it
            // was written this block, so stale tails cannot repeat forever.
            const auto direct = [&](int channel) {
                if (world_->mAudioBusTouched[channel] != world_->mBufCounter)
                    return 0.0f;
                const auto value = world_->mAudioBus[
                    channel * world_->mBufLength + sampleIndex];
                return std::isfinite(value) ? value : 0.0f;
            };
            outputBlock_[0][static_cast<std::size_t>(sampleIndex)] = leftMixed * mixGain_ +
                diskIO_.left(static_cast<unsigned>(sampleIndex)) + direct(0);
            outputBlock_[1][static_cast<std::size_t>(sampleIndex)] = rightMixed * mixGain_ +
                diskIO_.right(static_cast<unsigned>(sampleIndex)) + direct(1);
        }

        const auto blockEnd = blockStart + static_cast<std::uint64_t>(kSCBlockSize);
        for (auto& voice : audioVoices_) {
            if (voice.eventEffectTail.active &&
                voice.eventEffectTail.finished) {
                deactivateAudioVoice(voice);
            } else if (voice.reverbTail &&
                       voice.releaseEndSample <= blockEnd) {
                if (!beginNoteEventEffectTail(voice, blockEnd))
                    deactivateAudioVoice(voice);
            } else if (voice.releasing && !voice.reverbTail &&
                       !voice.eventEffectTail.active &&
                       voice.releaseEndSample <= blockEnd) {
                // SynthDef reverb sends use a control-rate Lag. Keep the graph alive
                // after its voice envelope reaches zero so a later block can set the
                // send to zero and let that Lag finish before stopping the graph.
                beginReverbTail(voice, blockEnd);
            } else if (!voice.releasing && voice.midiNote >= 0 &&
                       voice.endSample <= blockEnd) {
                beginReverbTail(voice, blockEnd);
            }
        }
        ++world_->mBufCounter;
        cursor_ = 0;
    }

    // UGen registration is process-global in this SC revision. Keep modules loaded
    // while any VST instance exists; the VST module terminator unloads them once.
    void stopWorld() {
        LifecycleTrace trace("stopWorld including locks", this);
        std::lock_guard<std::mutex> worldLock(worldAccessMutex_);
        stopWorldLocked();
        hostProcessingConfigured_ = false;
    }

    // Caller owns worldAccessMutex_. Never call the locking wrapper recursively.
    void stopWorldLocked() {
        scLanguageRuntime_.shutdown();
        scHostBridge_.stop();
        languageSynthDefNames_.clear();
        languageClientPort_ = 0;
        scLanguageServerPort_ = 0;
        if (world_) {
            std::string diskError;
            const bool diskDrained = diskIO_.closeAll(diskError);
            diskCleanupFailed_ |= !diskDrained;
            detachSampleBuffers();
            std::lock_guard<std::mutex> lifecycleLock(scLibraryLifecycleMutex);
            LifecycleTrace cleanupTrace("World_Cleanup", this);
            if (diskDrained) World_Cleanup(world_, false);
            else std::fprintf(stderr, "Dexter DiskIO: %s; retaining the inactive SC World rather than freeing memory still referenced by disk I/O.\n", diskError.c_str());
            if (diskDrained && !diskError.empty())
                std::fprintf(stderr, "Dexter DiskIO finalization error: %s\n", diskError.c_str());
            world_ = nullptr;
        }
        sampleCache_.clear();
        sampleCacheBytes_ = 0;
        ++sampleWorldGeneration_;
        for (auto& voice : audioVoices_)
            voice = {};
        for (auto& player : reverbGraphs_)
            player.fill(nullptr);
        for (auto& graphs : playerEffectGraphs_)
            graphs.fill(nullptr);
        playerEffectTransitions_ = {};
        shapeEffectSmoothed_.fill(0.0f);
        driveEffectSmoothed_.fill(0.0f);
        bitsEffectSmoothed_.fill(0.0f);
        crushEffectSmoothed_.fill(1.0f);
        bitcrusherStates_.fill({});
        for (auto& indices : playerEffectDefinitionIndices_)
            indices.fill(-1);
        for (auto& indices : playerEffectControlEffectIndices_)
            indices.fill(-1);
        for (auto& player : playerEffectControlSmoothed_)
            for (auto& controls : player)
                controls.fill(0.0f);
        synthDefinitions_.fill(nullptr);
        userSynthDefinitions_.clear();
        userSynthFiles_.clear();
        rootGroup_ = nullptr;
        nextDynamicGraphNodeId_ = 100;
        cursor_ = kSCBlockSize;
        for (auto& channel : outputBlock_)
            channel.fill(0.0f);
        std::fill(echoBuffer_.begin(), echoBuffer_.end(), 0.0f);
        echoMixSmoothed_.fill(0.0f);
        echoFeedbackTarget_.fill(0.0f);
        echoFeedbackSmoothed_.fill(0.0f);
        reverbMixSmoothed_.fill(0.0f);
        mixGain_ = 0.0f;
        {
            std::lock_guard<std::mutex> patternLock(patternMutex_);
            for (auto& pattern : activePatterns_)
                pattern = {};
            for (auto& pattern : pendingPatterns_)
                pattern = {};
            for (auto& control : pendingEditorControls_)
                control = {};
            for (auto& playerGates : activeMidiGates_)
                playerGates.fill({});
            pendingPlayerMask_ = 0;
            pendingMuteMask_ = 0;
            pendingMuteStates_.fill(false);
            mutedPlayers_.fill(false);
            playerMuteGain_.fill(1.0f);
            editorAmplitudeCurrent_.fill(1.0f);
            editorCutoffCurrent_.fill(20000.0f);
            for (std::size_t player = 0; player < kPlayerCount; ++player)
                publishEditorPlayerState(player);
        }
    }

    std::string scriptNamespace_;
    double sampleRate_ = 48000.0;
    std::mutex worldAccessMutex_;
    bool hostProcessingConfigured_ = false;
    std::string worldInitializationError_;
    World* world_ = nullptr;
    int scLanguageServerPort_ = 0;
    dexter::sclang::Runtime scLanguageRuntime_;
    dexter::sclang::HostOscBridge scHostBridge_;
    std::set<std::string> languageSynthDefNames_;
    std::uint16_t languageClientPort_ = 0;
    PortableDiskIO diskIO_;
    bool diskCleanupFailed_ = false;
    Group* rootGroup_ = nullptr;
    std::array<GraphDef*, kSynthGraphCount> synthDefinitions_{};
    std::vector<UserSynthFile> userSynthFiles_;
    std::vector<UserSynthDefinition> userSynthDefinitions_;
    std::vector<std::vector<dexter::PortableUserSynthControlSpec>>
        editorUserSynthControlSpecs_;
    int32 nextDynamicGraphNodeId_ = 100;
    std::array<AudioVoice, kAudioVoiceCount> audioVoices_{};
    std::array<std::array<Graph*, 3>, kPlayerCount> reverbGraphs_{};
    std::array<std::array<Graph*, kMaxPlayerEffectChain>, kPlayerCount>
        playerEffectGraphs_{};
    std::array<PlayerEffectChainTransition, kPlayerCount> playerEffectTransitions_{};
    std::array<float, kPlayerCount> shapeEffectSmoothed_{};
    std::array<float, kPlayerCount> driveEffectSmoothed_{};
    std::array<float, kPlayerCount> bitsEffectSmoothed_{};
    std::array<float, kPlayerCount> crushEffectSmoothed_{};
    std::array<dexter::FoxDotBitcrusherState, kPlayerCount> bitcrusherStates_{};
    std::array<std::array<std::array<float, 7>, kMaxPlayerEffectChain>,
               kPlayerCount> playerEffectControlSmoothed_{};
    std::array<std::array<int, kMaxPlayerEffectChain>, kPlayerCount>
        playerEffectControlEffectIndices_ = [] {
        std::array<std::array<int, kMaxPlayerEffectChain>, kPlayerCount> indices{};
        for (auto& player : indices)
            player.fill(-1);
        return indices;
    }();
    std::array<std::array<int, kMaxPlayerEffectChain>, kPlayerCount>
        playerEffectDefinitionIndices_ = [] {
        std::array<std::array<int, kMaxPlayerEffectChain>, kPlayerCount> indices{};
        for (auto& player : indices)
            player.fill(-1);
        return indices;
    }();
    std::string ugenPluginPath_;
    std::array<std::array<float, kSCBlockSize>, 2> outputBlock_{};
    std::vector<float> echoBuffer_;
    std::array<float, kPlayerCount> echoMixSmoothed_{};
    std::array<float, kPlayerCount> echoFeedbackTarget_{};
    std::array<float, kPlayerCount> echoFeedbackSmoothed_{};
    std::array<float, kPlayerCount> reverbMixSmoothed_{};
    std::array<float, kPlayerCount> echoDelaySmoothed_ = [] {
        std::array<float, kPlayerCount> initial{};
        initial.fill(0.25f);
        return initial;
    }();
    float mixGain_ = 0.0f;
    std::atomic<float> masterGainTarget_{1.0f};
    float masterGainCurrent_ = 1.0f;
    int cursor_ = kSCBlockSize;
    std::mutex patternMutex_;
    std::vector<std::unique_ptr<CachedSample>> sampleCache_;
    std::size_t sampleCacheBytes_ = 0;
    std::uint64_t sampleWorldGeneration_ = 1;
    std::array<PatternState, kPlayerCount> activePatterns_{};
    std::array<PatternState, kPlayerCount> pendingPatterns_{};
    std::array<PendingEditorControl, kPlayerCount> pendingEditorControls_{};
    std::array<PendingEditorEffectControls, kPlayerCount>
        pendingEditorEffectControls_{};
    std::array<PendingEditorUserSynthControls, kPlayerCount>
        pendingEditorUserSynthControls_{};
    std::array<std::atomic<std::uint32_t>, kPlayerCount> editorPlayerState_{};
    std::array<std::array<std::atomic<float>, 7>, kPlayerCount>
        editorPlayerValues_{};
    std::array<std::array<std::array<std::atomic<float>, 7>,
                          kMaxPlayerEffectChain>, kPlayerCount>
        editorPlayerEffectValues_{};
    std::array<std::array<std::atomic<float>,
                          dexter::kMaxPortableUserSynthControls>,
                kPlayerCount> editorPlayerUserSynthValues_{};
    std::array<std::atomic<std::uint64_t>, kPlayerCount> editorPlayerEffectChain_{};
    std::uint8_t pendingPlayerMask_ = 0;
    std::array<bool, kPlayerCount> pendingMuteStates_{};
    std::uint8_t pendingMuteMask_ = 0;
    std::array<bool, kPlayerCount> mutedPlayers_{};
    std::array<float, kPlayerCount> playerMuteGain_ = [] {
        std::array<float, kPlayerCount> values{};
        values.fill(1.0f);
        return values;
    }();
    std::array<float, kPlayerCount> editorAmplitudeCurrent_ = [] {
        std::array<float, kPlayerCount> values{};
        values.fill(1.0f);
        return values;
    }();
    std::array<float, kPlayerCount> editorCutoffCurrent_ = [] {
        std::array<float, kPlayerCount> values{};
        values.fill(20000.0f);
        return values;
    }();
    std::array<std::uint64_t, kPlayerCount> patternStartSamples_{};
    std::uint64_t sampleClock_ = 0;
    std::array<std::array<ActiveMidiGate, kMaxPolyphony>, kPlayerCount> activeMidiGates_{};
    std::atomic<bool> stopRequested_{false};
    std::mutex scriptThreadMutex_;
    std::thread scriptThread_;
    std::atomic<std::uint64_t> scSourceGeneration_{0};
    std::atomic<bool> scPreparationStatusOverride_{false};
    std::mutex catalogThreadMutex_;
    std::thread catalogThread_;
    bool catalogDiscoveryStarted_ = false;
    std::atomic<bool> scriptRunning_{false};
    std::atomic<std::uint64_t> scriptGeneration_{0};
    std::mutex statusMutex_;
    std::string editorStatus_ = "Ready. Use p1 >> sequence(...) or play().";
    std::mutex editorCatalogMutex_;
    std::vector<std::string> editorExternalSynthNames_;
    std::mutex editorStateMutex_;
    std::string editorSource_ =
        "tempo(120)\r\nplay([60, 64, 67, 72], step=0.5, duration=0.45)\r\n";
    bool hasEditorSource_ = true;
    std::string superColliderSource_ = defaultSuperColliderSource();
    bool hasSuperColliderSource_ = true;
};

} // namespace Steinberg::Vst

BEGIN_FACTORY_DEF("Dexter U.S.", "", "")

DEF_CLASS2(INLINE_UID(0xC7F03BC1, 0xDEEC47AA, 0x8E28B107, 0xF9B1A553),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           "Dexter U.S FoxDot",
           0,
           "Instrument|Synth",
    DEXTER_PLUGIN_VERSION,
           kVstVersionString,
           Steinberg::Vst::DexterSCTone::createInstance)

END_FACTORY
