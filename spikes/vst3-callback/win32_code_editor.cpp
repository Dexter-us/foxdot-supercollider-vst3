          continue;
            for (std::size_t effectSlot = 0;
                 effectSlot < state.effectIndices.size(); ++effectSlot) {
                const auto effectIndex = state.effectIndices[effectSlot];
                if (effectIndex < 0 ||
                    effectIndex >= static_cast<int>(externalNames.size()))
                    continue;
                std::wstring effectRow = L"        ";
                effectRow += selectedItemData == panelItemForEffect(
                    static_cast<int>(index),
                    static_cast<int>(effectSlot)) ? L"[-] " : L"[+] ";
                effectRow += L"#" + std::to_wstring(effectSlot + 1) + L" ";
                effectRow += fromUtf8(externalNames[
                    static_cast<std::size_t>(effectIndex)]);
                addPanelRow(effectRow, panelItemForEffect(
                    static_cast<int>(index), static_cast<int>(effectSlot)));
            }
        }
        lastPanelPlayers_ = states;
        panelPlayersInitialized_ = true;
        bool restoredSelection = false;
        if (selectedItemData >= 0) {
            const int count = static_cast<int>(
                SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
            for (int row = 0; row < count; ++row) {
                if (static_cast<int>(SendMessageW(
                        panelList_, LB_GETITEMDATA, row, 0)) !=
                    selectedItemData)
                    continue;
                SendMessageW(panelList_, LB_SETCURSEL, row, 0);
                restoredSelection = true;
                break;
            }
        }
        if (!restoredSelection) {
            for (std::size_t index = 0; index < states.size(); ++index) {
                if (!states[index].active)
                    continue;
                SendMessageW(panelList_, LB_SETCURSEL,
                             static_cast<WPARAM>(index), 0);
                break;
            }
        }
        restorePanelScrollAnchor(scrollAnchor);
        updatePlayerActionButtons();
    }

    int selectedPlayerIndex() const {
        if (!panelList_ || selectedPanelTab() != 0)
            return -1;
        const int selectedItem = static_cast<int>(
            SendMessageW(panelList_, LB_GETCURSEL, 0, 0));
        if (selectedItem == LB_ERR)
            return -1;
        const int itemData = static_cast<int>(
            SendMessageW(panelList_, LB_GETITEMDATA, selectedItem, 0));
        return panelItemPlayer(itemData);
    }

    int selectedEffectSlotIndex() const {
        if (!panelList_ || selectedPanelTab() != 0)
            return -1;
        const int selectedItem = static_cast<int>(
            SendMessageW(panelList_, LB_GETCURSEL, 0, 0));
        if (selectedItem == LB_ERR)
            return -1;
        const int itemData = static_cast<int>(
            SendMessageW(panelList_, LB_GETITEMDATA, selectedItem, 0));
        return panelItemEffectSlot(itemData);
    }

    void selectPlayerAtEditorCaret() {
        if (!editor_ || !panelList_ || selectedPanelTab() != 0)
            return;
        const auto source = editorUtf8Source();
        const auto position = static_cast<std::size_t>(
            SendMessageW(editor_, SCI_GETCURRENTPOS, 0, 0));
        dexter::editor::PlayerAssignment assignment;
        if (!dexter::editor::findPlayerAssignmentAt(
                source, position, assignment))
            return;
        const int count = static_cast<int>(
            SendMessageW(panelList_, LB_GETCOUNT, 0, 0));
        for (int row = 0; row < count; ++row) {
            const auto player = static_cast<int>(
                SendMessageW(panelList_, LB_GETITEMDATA, row, 0));
            if (player != assignment.playerIndex)
                continue;
            if (SendMessageW(panelList_, LB_GETCURSEL, 0, 0) != row) {
                SendMessageW(panelList_, LB_SETCURSEL, row, 0);
                updatePlayerActionButtons();
            }
            return;
        }
    }

    void updatePlayerActionButtons() {
        const int player = selectedPlayerIndex();
        if (player < 0 || !host_) {
            static constexpr std::array<const wchar_t*, 7> labels{
                L"Level", L"Pan", L"Cutoff", L"FX mix", L"Bits", L"Crush",
                L"PShift"};
            if (panelMuteButton_)
                EnableWindow(panelMuteButton_, FALSE);
            if (panelStopButton_)
                EnableWindow(panelStopButton_, FALSE);
            for (std::size_t index = 0;
                 index < playerControlSliders_.size(); ++index) {
                if (playerControlLabels_[index])
                    SetWindowTextW(playerControlLabels_[index], labels[index]);
                if (playerControlSliders_[index])
                    EnableWindow(playerControlSliders_[index], FALSE);
                const auto control = index == 0 ? EditorPlayerControl::Amplitude
                    : index == 1 ? EditorPlayerControl::Pan
                    : index == 2 ? EditorPlayerControl::Cutoff
                    : index == 3 ? EditorPlayerControl::EffectMix
                    : index == 4 ? EditorPlayerControl::BitDepth
                    : index == 5 ? EditorPlayerControl::CrushRate
                                 : EditorPlayerControl::PShift;
                const int visibility = editorPlayerControlVisible(
                    control, selectedPanelTab() == 0, false, false, false)
                    ? SW_SHOW : SW_HIDE;
                ShowWindow(playerControlLabels_[index], visibility);
                ShowWindow(playerControlSliders_[index], visibility);
                ShowWindow(playerControlValues_[index], visibility);
                ShowWindow(playerControlRanges_[index], SW_HIDE);
            }
            return;
        }
        std::array<EditorPlayerState, 8> states{};
        host_->editorPlayerStates(states);
        const auto& state = states[static_cast<std::size_t>(player)];
        const auto muteText = state.muted
            ? L"Unmute p" + std::to_wstring(player + 1)
            : L"Mute p" + std::to_wstring(player + 1);
        if (panelMuteButton_) {
            SetWindowTextW(panelMuteButton_, muteText.c_str());
            EnableWindow(panelMuteButton_, state.active ? TRUE : FALSE);
        }
        if (panelStopButton_)
            EnableWindow(panelStopButton_, state.active ? TRUE : FALSE);
        updatePlayerControlSliders(states);
    }

    void updatePlayerControlSliders(
        const std::array<EditorPlayerState, 8>& states) {
        const int player = selectedPlayerIndex();
        if (player < 0 || !host_)
            return;
        const auto& state = states[static_cast<std::size_t>(player)];
        const int effectSlot = selectedEffectSlotIndex();
        if (effectSlot >= 0 &&
            effectSlot < static_cast<int>(state.effectIndices.size())) {
            const auto externalNames = host_->editorExternalSynthNames();
            const int effectIndex =
                state.effectIndices[static_cast<std::size_t>(effectSlot)];
            if (effectIndex >= 0 &&
                effectIndex < static_cast<int>(externalNames.size())) {
                const auto effectName = externalNames[
                    static_cast<std::size_t>(effectIndex)];
                const auto specs =
                    dexter::editor::playerEffectControlSpecs(effectName);
                for (std::size_t index = 0;
                     index < playerControlSliders_.size(); ++index) {
                    const bool available = index < specs.size();
                    const int visibility = available ? SW_SHOW : SW_HIDE;
                    ShowWindow(playerControlLabels_[index], visibility);
                    ShowWindow(playerControlSliders_[index], visibility);
                    ShowWindow(playerControlValues_[index], visibility);
                    ShowWindow(playerControlRanges_[index], visibility);
                    if (!available)
                        continue;
                    std::string label = specs[index].name;
                    if (label.rfind("fx_", 0) == 0)
                        label.erase(0, 3);
                    const auto labelText = fromUtf8(label);
                    SetWindowTextW(playerControlLabels_[index],
                                   labelText.c_str());
                    const auto rangeText =
                        fromUtf8(effectControlRangeText(specs[index]));
                    SetWindowTextW(playerControlRanges_[index],
                                   rangeText.c_str());
                    if (playerControlSliders_[index]) {
                        SendMessageW(playerControlSliders_[index],
                                     TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
                        if (activePlayerSlider_ !=
                            static_cast<int>(index)) {
                            const auto controlIndex =
                                specs[index].controlIndex >= 0
                                    ? static_cast<std::size_t>(
                                          specs[index].controlIndex)
                                    : index;
                            const float normalized = std::clamp(
                                state.effectControlValues[
                                    static_cast<std::size_t>(effectSlot)]
                                    [controlIndex],
                                0.0f, 1.0f);
                            const int position = static_cast<int>(
                                std::lround(normalized * 1000.0f));
                            SendMessageW(playerControlSliders_[index],
                                         TBM_SETPOS, TRUE, position);
                            const auto valueText = fromUtf8(
                                effectControlValueText(specs[index],
                                                      normalized));
                            if (playerControlValues_[index])
                                SetWindowTextW(playerControlValues_[index],
                                               valueText.c_str());
                        }
                        EnableWindow(playerControlSliders_[index],
                                     state.active ? TRUE : FALSE);
                    }
                }
                return;
            }
        }
        if (effectSlot < 0 && selectedPanelTab() == 0 &&
            state.userSynthControlCount > 0) {
            for (std::size_t index = 0;
                 index < playerControlSliders_.size(); ++index) {
                const bool available =
                    index < state.userSynthControlCount;
                const int visibility = available ? SW_SHOW : SW_HIDE;
                ShowWindow(playerControlLabels_[index], visibility);
                ShowWindow(playerControlSliders_[index], visibility);
                ShowWindow(playerControlValues_[index], visibility);
                ShowWindow(playerControlRanges_[index], visibility);
                if (!available)
                    continue;
                const auto& control = state.userSynthControls[index];
                const auto label = fromUtf8(control.name);
                SetWindowTextW(playerControlLabels_[index], label.c_str());
                const auto range = fromUtf8(
                    userSynthControlRangeText(control));
                SetWindowTextW(playerControlRanges_[index], range.c_str());
                if (!playerControlSliders_[index])
                    continue;
                SendMessageW(playerControlSliders_[index], TBM_SETRANGE, TRUE,
                              MAKELONG(0, 1000));
                if (activePlayerSlider_ != static_cast<int>(index)) {
                    const float normalized = std::clamp(
                        control.normalizedValue, 0.0f, 1.0f);
                    SendMessageW(playerControlSliders_[index], TBM_SETPOS,
                                 TRUE, static_cast<LPARAM>(
                                     std::lround(normalized * 1000.0f)));
                    const auto value = fromUtf8(
                        userSynthControlValueText(control, normalized));
                    if (playerControlValues_[index])
                        SetWindowTextW(playerControlValues_[index],
                                       value.c_str());
                }
                EnableWindow(playerControlSliders_[index],
                             state.active ? TRUE : FALSE);
            }
            return;
        }
        const auto cutoffPosition = static_cast<int>(std::lround(
            std::clamp(std::log(std::clamp(state.cutoffHz, 20.0f, 20000.0f) /
                                    20.0f) /
                           std::log(1000.0f),
                       0.0f, 1.0f) *
            1000.0f));
        const std::array<int, 7> positions{
            static_cast<int>(std::lround(
                std::clamp(state.amplitude, 0.0f, 1.0f) * 100.0f)),
            static_cast<int>(std::lround(
                (std::clamp(state.pan, -1.0f, 1.0f) + 1.0f) * 100.0f)),
            cutoffPosition,
            static_cast<int>(std::lround(
                std::clamp(state.effectMix, 0.0f, 1.0f) * 100.0f)),
            static_cast<int>(std::lround(
                std::clamp(state.bits, 0.0f, 24.0f))),
            static_cast<int>(std::lround(
                std::clamp(state.crush, 1.0f, 128.0f))),
            static_cast<int>(std::lround(
                (std::clamp(state.pshift, -24.0f, 24.0f) + 24.0f) *
                10.0f))};
        const std::array<std::wstring, 7> valueTexts{
            fromUtf8(sliderValueText(0, positions[0])),
            fromUtf8(sliderValueText(1, positions[1])),
            fromUtf8(sliderValueText(2, positions[2])),
            fromUtf8(sliderValueText(3, positions[3])),
            fromUtf8(sliderValueText(4, positions[4])),
            fromUtf8(sliderValueText(5, positions[5])),
            fromUtf8(sliderValueText(6, positions[6]))};
        static constexpr std::array<const wchar_t*, 7> labels{
            L"Level", L"Pan", L"Cutoff", L"FX mix", L"Bits", L"Crush",
            L"PShift"};
        for (std::size_t index = 0; index < playerControlSliders_.size(); ++index) {
            ShowWindow(playerControlRanges_[index], SW_HIDE);
            if (activePlayerSlider_ == static_cast<int>(index))
                continue;
            const auto control = index == 0 ? EditorPlayerControl::Amplitude
                : index == 1 ? EditorPlayerControl::Pan
                : index == 2 ? EditorPlayerControl::Cutoff
                : index == 3 ? EditorPlayerControl::EffectMix
                : index == 4 ? EditorPlayerControl::BitDepth
                : index == 5 ? EditorPlayerControl::CrushRate
                             : EditorPlayerControl::PShift;
            const bool available = editorPlayerControlAvailable(
                control, state.effectMixAvailable,
                state.bitcrusherEnabled, state.pshiftEnabled);
            const int visibility = (control == EditorPlayerControl::EffectMix &&
                                    !available)
                ? SW_HIDE : editorPlayerControlVisible(
                control, selectedPanelTab() == 0, true,
                state.bitcrusherEnabled, state.pshiftEnabled)
                ? SW_SHOW : SW_HIDE;
            if (playerControlLabels_[index])
                SetWindowTextW(playerControlLabels_[index], labels[index]);
            ShowWindow(playerControlLabels_[index], visibility);
            ShowWindow(playerControlSliders_[index], visibility);
            ShowWindow(playerControlValues_[index], visibility);
            if (playerControlSliders_[index]) {
                const int minimum = index == 5 ? 1 : 0;
                const int maximum = index == 6 ? 480
                    : index == 2 ? 1000
                    : index == 1 ? 200
                    : index == 4 ? 24
                    : index == 5 ? 128 : 100;
                SendMessageW(playerControlSliders_[index], TBM_SETRANGE,
                             TRUE, MAKELONG(minimum, maximum));
                SendMessageW(playerControlSliders_[index], TBM_SETPOS, TRUE,
                             positions[index]);
                EnableWindow(playerControlSliders_[index],
                              state.active && available
                                 ? TRUE
                                 : FALSE);
            }
            if (playerControlValues_[index])
                SetWindowTextW(playerControlValues_[index],
                               valueTexts[index].c_str());
        }
    }

    static std::string sliderValueText(std::size_t index, int position) {
        std::ostringstream text;
        if (index == 0 || index == 3) {
            text << std::fixed << std::setprecision(2) << position / 100.0;
        } else if (index == 1) {
            text << std::fixed << std::setprecision(2)
                 << (position / 100.0 - 1.0);
        } else if (index == 4) {
            text << position << " bits";
        } else if (index == 5) {
            text << position << "x";
        } else if (index == 6) {
            text << std::fixed << std::setprecision(1)
                 << (position / 10.0 - 24.0) << " st";
        } else {
            const double cutoff = 20.0 * std::pow(1000.0, position / 1000.0);
            text << static_cast<int>(std::lround(cutoff)) << " Hz";
        }
        return text.str();
    }

    static const char* effectControlUnit(std::string_view name) {
        if (name == "fx_tone" || name == "fx_center" ||
            name == "fx_frequency" ||
            name == "fx_rate" || name == "fx_modfreq")
            return " Hz";
        if (name == "fx_delay" || name == "fx_t60")
            return " s";
        if (name == "fx_drive")
            return " dB";
        if (name == "fx_stages")
            return " stages";
        return "";
    }

    static std::string effectControlRangeText(
        const dexter::editor::PlayerEffectControlSpec& spec) {
        std::ostringstream text;
        text << std::setprecision(3) << spec.minimum << " - "
             << std::setprecision(3) << spec.maximum
             << effectControlUnit(spec.name);
        return text.str();
    }

    static std::string effectControlValueText(
        const dexter::editor::PlayerEffectControlSpec& spec,
        float normalized) {
        const float value = spec.minimum + std::clamp(normalized, 0.0f, 1.0f) *
            (spec.maximum - spec.minimum);
        std::ostringstream text;
        text << std::setprecision(4) << value << effectControlUnit(spec.name);
        return text.str();
    }

    static float userSynthControlValue(
        const EditorPlayerState::UserSynthControl& control,
        float normalized) {
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        if (control.logarithmic)
            return control.minimum * std::pow(
                control.maximum / control.minimum, normalized);
        return control.minimum +
            normalized * (control.maximum - control.minimum);
    }

    static std::string userSynthControlRangeText(
        const EditorPlayerState::UserSynthControl& control) {
        std::ostringstream text;
        text << std::setprecision(4) << control.minimum << " - "
             << std::setprecision(4) << control.maximum;
        if (control.logarithmic)
            text << " log";
        return text.str();
    }

    static std::string userSynthControlValueText(
        const EditorPlayerState::UserSynthControl& control,
        float normalized) {
        std::ostringstream text;
        text << std::setprecision(5)
             << userSynthControlValue(control, normalized);
        return text.str();
    }

    static float playerControlValue(std::size_t index, int position) {
        if (index == 0 || index == 3)
            return std::clamp(position / 100.0f, 0.0f, 1.0f);
        if (index == 1)
            return std::clamp(position / 100.0f - 1.0f, -1.0f, 1.0f);
        if (index == 4)
            return std::clamp(static_cast<float>(position), 0.0f, 24.0f);
        if (index == 5)
            return std::clamp(static_cast<float>(position), 1.0f, 128.0f);
        if (index == 6)
            return std::clamp(position / 10.0f - 24.0f, -24.0f, 24.0f);
        const double cutoff = 20.0 * std::pow(1000.0, position / 1000.0);
        return static_cast<float>(std::clamp(cutoff, 20.0, 20000.0));
    }

    void onPlayerControlSlider(std::size_t index, int scrollCode) {
        const int player = selectedPlayerIndex();
        if (!host_ || player < 0 || index >= playerControlSliders_.size())
            return;
        activePlayerSlider_ = scrollCode == TB_ENDTRACK
            ? -1 : static_cast<int>(index);
        const int position = static_cast<int>(
            SendMessageW(playerControlSliders_[index], TBM_GETPOS, 0, 0));
        const int effectSlot = selectedEffectSlotIndex();
        if (effectSlot >= 0) {
            std::array<EditorPlayerState, 8> states{};
            host_->editorPlayerStates(states);
            const auto& state = states[static_cast<std::size_t>(player)];
            const auto externalNames = host_->editorExternalSynthNames();
            if (effectSlot >= static_cast<int>(state.effectIndices.size()))
                return;
            const int effectIndex =
                state.effectIndices[static_cast<std::size_t>(effectSlot)];
            if (effectIndex < 0 ||
                effectIndex >= static_cast<int>(externalNames.size()))
                return;
            const auto specs = dexter::editor::playerEffectControlSpecs(
                externalNames[static_cast<std::size_t>(effectIndex)]);
            if (index >= specs.size())
                return;
            const float normalized = std::clamp(position / 1000.0f, 0.0f, 1.0f);
            host_->setEditorPlayerEffectControl(
                player, effectSlot,
                specs[index].controlIndex >= 0
                    ? specs[index].controlIndex
                    : static_cast<int>(index),
                normalized);
            if (playerControlValues_[index]) {
                const auto label = fromUtf8(
                    effectControlValueText(specs[index], normalized));
                SetWindowTextW(playerControlValues_[index], label.c_str());
            }
            if (scrollCode == TB_ENDTRACK)
                commitPlayerEffectControlToSource(
                    player, effectSlot, specs[index], normalized);
            return;
        }
        std::array<EditorPlayerState, 8> states{};
        host_->editorPlayerStates(states);
        const auto& state = states[static_cast<std::size_t>(player)];
        if (index < state.userSynthControlCount) {
            const auto& control = state.userSynthControls[index];
            const float normalized =
                std::clamp(position / 1000.0f, 0.0f, 1.0f);
            host_->setEditorPlayerUserSynthControl(
                player, static_cast<int>(index), normalized);
            if (playerControlValues_[index]) {
                const auto label = fromUtf8(
                    userSynthControlValueText(control, normalized));
                SetWindowTextW(playerControlValues_[index], label.c_str());
            }
            return;
        }
        const float value = playerControlValue(index, position);
        const auto control = index == 0 ? EditorPlayerControl::Amplitude
            : index == 1 ? EditorPlayerControl::Pan
            : index == 2 ? EditorPlayerControl::Cutoff
            : index == 3 ? EditorPlayerControl::EffectMix
            : index == 4 ? EditorPlayerControl::BitDepth
            : index == 5 ? EditorPlayerControl::CrushRate
                         : EditorPlayerControl::PShift;
        host_->setEditorPlayerControl(player, control, value);
        if (playerControlValues_[index]) {
            const auto label = fromUtf8(sliderValueText(index, position));
            SetWindowTextW(playerControlValues_[index], label.c_str());
        }
        if (scrollCode == TB_ENDTRACK)
            commitPlayerControlToSource(player, index, value);
    }

    void commitPlayerControlToSource(int player, std::size_t index,
                                     float value) {
        if (!editor_ || player < 0 || index >= playerControlSliders_.size())
            return;
        auto source = editorUtf8Source();
        dexter::editor::PlayerAssignment assignment;
        if (!dexter::editor::findPlayerAssignmentForIndex(
                source, player, assignment))
            return;
        const char* keyword = index == 0 ? "amp"
            : index == 1 ? "pan"
            : index == 2 ? "cutoff"
            : index == 3 ? "fx_mix"
            : index == 4 ? "bits"
            : index == 5 ? "crush"
                         : "pshift";
        std::ostringstream number;
        number << std::fixed
               << std::setprecision(index == 2 ? 1 : index == 6 ? 1
                                            : index >= 4 ? 0 : 3)
               << value;
        const bool changed = index == 3
            ? dexter::editor::setPlayerEffectMix(
                  source, assignment, number.str())
            : dexter::editor::setPlayerArgument(
                  source, assignment, keyword, number.str());
        if (changed)
            applyEditorTextEdit(source);
    }

    void commitPlayerEffectControlToSource(
        int player, int effectSlot,
        const dexter::editor::PlayerEffectControlSpec& spec,
        float normalized) {
        if (!editor_ || player < 0 || effectSlot < 0)
            return;
        auto source = editorUtf8Source();
        dexter::editor::PlayerAssignment assignment;
        if (!dexter::editor::findPlayerAssignmentForIndex(
                source, player, assignment))
            return;
        const float value = spec.minimum + std::clamp(
            normalized, 0.0f, 1.0f) * (spec.maximum - spec.minimum);
        std::ostringstream number;
        number << std::setprecision(7) << value;
        if (dexter::editor::setPlayerEffectControl(
                source, assignment, static_cast<std::size_t>(effectSlot),
                spec.name, number.str()))
            applyEditorTextEdit(source);
    }

    void toggleSelectedPlayerMute() {
        const int player = selectedPlayerIndex();
        if (player < 0 || !host_)
            return;
        std::array<EditorPlayerState, 8> states{};
        host_->editorPlayerStates(states);
        host_->setEditorPlayerMuted(player,
                                    !states[static_cast<std::size_t>(player)].muted);
        updatePlayerActionButtons();
    }

    void stopSelectedPlayer() {
        const int player = selectedPlayerIndex();
        if (player >= 0 && host_)
            host_->stopEditorPlayer(player);
    }

    void openSamplesFolder() {
        const auto portable = portableScriptDirectory();
        if (portable.empty()) {
            MessageBoxW(window_, L"The plugin bundle path is not available.",
                        L"Samples", MB_OK | MB_ICONERROR);
            return;
        }
        std::error_code error;
        const auto directory = portable / L"Samples";
        std::filesystem::create_directories(directory, error);
        if (error) {
            MessageBoxW(window_, L"Could not create or open Portable\\Samples.",
                        L"Samples", MB_OK | MB_ICONERROR);
            return;
        }
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(window_, L"open", directory.c_str(), nullptr, nullptr,
                          SW_SHOWNORMAL));
        if (result <= 32)
            MessageBoxW(window_, L"Windows could not open Portable\\Samples.",
                        L"Samples", MB_OK | MB_ICONERROR);
    }

    void layoutChildren(int width, int height) {
        if (!editor_)
            return;
        const int buttonY = kMargin;
        MoveWindow(runButton_, kMargin, buttonY, 56, kToolbarHeight - 4, TRUE);
        MoveWindow(stopButton_, kMargin + 60, buttonY, 56, kToolbarHeight - 4, TRUE);
        MoveWindow(openButton_, kMargin + 120, buttonY, 56, kToolbarHeight - 4, TRUE);
        MoveWindow(saveButton_, kMargin + 180, buttonY, 62, kToolbarHeight - 4, TRUE);
        MoveWindow(saveAsButton_, kMargin + 250, buttonY, 76, kToolbarHeight - 4, TRUE);
        MoveWindow(copyErrorButton_, kMargin + 330, buttonY, 82, kToolbarHeight - 4, TRUE);
        MoveWindow(status_, kMargin + 420, buttonY, std::max(100, width - 440),
                   kToolbarHeight - 4, TRUE);
        const int gainRowY = kMargin + kToolbarHeight;
        if (masterGainSlider_) {
            MoveWindow(masterGainLabel_, kMargin, gainRowY + 2, 60, 22, TRUE);
            MoveWindow(masterGainSlider_, kMargin + 68, gainRowY,
                       210, 24, TRUE);
            MoveWindow(masterGainValue_, kMargin + 286, gainRowY + 2,
                       64, 22, TRUE);
        }
        const int languageRowY = kMargin + kToolbarHeight + kGainRowHeight;
        MoveWindow(pythonTabButton_, kMargin, languageRowY, 116, 22, TRUE);
        MoveWindow(superColliderTabButton_, kMargin + 120, languageRowY,
                   150, 22, TRUE);
        const int editorTop = languageRowY + 24;
        const int editorHeight = std::max(80, height - editorTop - kStatusHeight - 2 * kMargin);
        const int panelX = width - kMargin - kPanelWidth;
        const int editorWidth = std::max(280, panelX - kMargin - 12);
        MoveWindow(editor_, kMargin, editorTop, editorWidth, editorHeight, TRUE);
        MoveWindow(panelTabs_, panelX, editorTop, kPanelWidth, 28, TRUE);
        const int panelBottom = editorTop + editorHeight - 27;
        const int controlsHeight =
            static_cast<int>(playerControlSliders_.size()) * 34;
        const int controlsTop = panelBottom - controlsHeight - 4;
        MoveWindow(panelList_, panelX + 2, editorTop + 30, kPanelWidth - 4,
                   std::max(80, controlsTop - (editorTop + 35)), TRUE);
        for (std::size_t index = 0; index < playerControlSliders_.size(); ++index) {
            const int rowY = controlsTop + static_cast<int>(index) * 34;
            MoveWindow(playerControlLabels_[index], panelX + 4, rowY,
                       112, 16, TRUE);
            MoveWindow(playerControlRanges_[index], panelX + 118, rowY,
                       76, 16, TRUE);
            MoveWindow(playerControlValues_[index], panelX + 196, rowY,
                       66, 16, TRUE);
            MoveWindow(playerControlSliders_[index], panelX + 4, rowY + 15,
                       258, 18, TRUE);
        }
        MoveWindow(panelMuteButton_, panelX + 2, editorTop + editorHeight - 27,
                   88, 24, TRUE);
        MoveWindow(panelStopButton_, panelX + 96, editorTop + editorHeight - 27,
                   88, 24, TRUE);
        MoveWindow(panelSamplesButton_, panelX + 2,
                   editorTop + editorHeight - 27, 160, 24, TRUE);
        MoveWindow(panelVersion_, panelX + kPanelWidth - 84,
                   editorTop + editorHeight - 27, 80, 24, TRUE);
        const int footerY = height - kStatusHeight - kMargin;
        MoveWindow(fileName_, kMargin, footerY, std::max(120, width - 250),
                   kStatusHeight, TRUE);
        MoveWindow(brand_, std::max(kMargin, width - 220), footerY, 210, kStatusHeight, TRUE);
    }

    void updateSaveButton() {
        if (saveButton_)
            SetWindowTextW(saveButton_, documentDirty_ ? L"Save*" : L"Save");
        updateFileName();
    }

    void updateFileName() {
        if (!fileName_)
            return;
        std::wstring name = currentFilePath_.empty()
                                ? (activeTab_ == 0
                                       ? L"Untitled Python script"
                                       : L"Untitled SuperCollider script")
                                : std::filesystem::path(currentFilePath_).filename().wstring();
        if (documentDirty_)
            name += L"  *";
        SetWindowTextW(fileName_, name.c_str());
    }

    void updateMasterGainText(int position) {
        if (!masterGainValue_)
            return;
        const auto text = std::to_wstring(std::clamp(
            position, 0, kDexterMasterGainMaximumPercent)) + L"%";
        SetWindowTextW(masterGainValue_, text.c_str());
    }

    std::string editorUtf8Source() const {
        if (!editor_)
            return {};
        const auto length = static_cast<std::size_t>(
            SendMessageW(editor_, SCI_GETTEXTLENGTH, 0, 0));
        std::string source(length + 1, '\0');
        SendMessageW(editor_, SCI_GETTEXT, source.size(),
                     reinterpret_cast<LPARAM>(source.data()));
        source.resize(length);
        return source;
    }

    std::wstring editorSource() const {
        return fromUtf8(editorUtf8Source());
    }

    void applyEditorTextEdit(const std::string& source) {
        if (!editor_)
            return;
        if (source.size() > kMaxScriptBytes) {
            SetWindowTextW(status_, L"Edit would exceed the 256 KiB script limit.");
            return;
        }
        const auto previous = editorUtf8Source();
        if (source == previous)
            return;
        std::size_t prefix = 0;
        while (prefix < source.size() && prefix < previous.size() &&
               source[prefix] == previous[prefix])
            ++prefix;
        std::size_t suffix = 0;
        while (suffix < source.size() - prefix &&
               suffix < previous.size() - prefix &&
               source[source.size() - suffix - 1] ==
                   previous[previous.size() - suffix - 1])
            ++suffix;
        const auto oldEnd = previous.size() - suffix;
        const auto newEnd = source.size() - suffix;
        const auto oldCaret = static_cast<std::size_t>(
            SendMessageW(editor_, SCI_GETCURRENTPOS, 0, 0));
        const auto oldAnchor = static_cast<std::size_t>(
            SendMessageW(editor_, SCI_GETANCHOR, 0, 0));
        updatingEditor_ = true;
        SendMessageW(editor_, SCI_BEGINUNDOACTION, 0, 0);
        SendMessageW(editor_, SCI_SETTARGETSTART, prefix, 0);
        SendMessageW(editor_, SCI_SETTARGETEND, oldEnd, 0);
        SendMessageW(editor_, SCI_REPLACETARGET, newEnd - prefix,
                     reinterpret_cast<LPARAM>(source.data() + prefix));
        SendMessageW(editor_, SCI_ENDUNDOACTION, 0, 0);
        const auto remap = [prefix, oldEnd, newEnd](std::size_t position) {
            if (position <= prefix)
                return position;
            if (position < oldEnd)
                return newEnd;
            return newEnd + position - oldEnd;
        };
        const auto caret = std::min(remap(oldCaret), source.size());
        const auto anchor = std::min(remap(oldAnchor), source.size());
        SendMessageW(editor_, SCI_SETSEL, anchor, caret);
        updatingEditor_ = false;
        currentSource_ = fromUtf8(source);
        sourceInitialized_ = true;
        documentDirty_ = true;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, FALSE);
        updateSaveButton();
        if (activeTab_ == 0) {
            pythonSource_ = currentSource_;
            pythonSourceInitialized_ = true;
            pythonDirty_ = true;
            if (host_)
                host_->setEditorSource(source);
        } else {
            superColliderSource_ = currentSource_;
            superColliderSourceInitialized_ = true;
            superColliderDirty_ = true;
            if (host_)
                host_->setSuperColliderSource(source);
        }
    }

    bool showPlayerContextMenu(HWND editor, LPARAM coordinates) {
        if (!host_ || activeTab_ != 0 || !editor || editor != editor_)
            return false;
        POINT screenPoint{};
        const POINTS packed = MAKEPOINTS(coordinates);
        if (packed.x == -1 && packed.y == -1) {
            const auto caret = static_cast<int>(
                SendMessageW(editor_, SCI_GETCURRENTPOS, 0, 0));
            const auto caretPoint = static_cast<LPARAM>(
                SendMessageW(editor_, SCI_POINTXFROMPOSITION, 0, caret));
            const auto caretY = static_cast<LPARAM>(
                SendMessageW(editor_, SCI_POINTYFROMPOSITION, 0, caret));
            screenPoint.x = static_cast<LONG>(caretPoint);
            screenPoint.y = static_cast<LONG>(caretY);
            ClientToScreen(editor_, &screenPoint);
        } else {
            screenPoint.x = packed.x;
            screenPoint.y = packed.y;
        }
        POINT clientPoint = screenPoint;
        if (!ScreenToClient(editor_, &clientPoint))
            return false;
        const auto position = static_cast<std::size_t>(SendMessageW(
            editor_, kSciPositionFromPointClose, clientPoint.x, clientPoint.y));
        if (position > editorUtf8Source().size())
            return false;
        auto source = editorUtf8Source();
        dexter::editor::PlayerAssignment assignment;
        if (!dexter::editor::findPlayerLineAt(source, position, assignment,
                                               true))
            return false;

        HMENU menu = CreatePopupMenu();
        HMENU instruments = CreatePopupMenu();
        HMENU effects = CreatePopupMenu();
        if (!menu || !instruments || !effects) {
            if (menu) DestroyMenu(menu);
            if (instruments) DestroyMenu(instruments);
            if (effects) DestroyMenu(effects);
            return true;
        }
        const auto externalNames = host_->editorExternalSynthNames();
        std::vector<std::string> embedded(kEmbeddedSynthNames.begin(),
                                          kEmbeddedSynthNames.end());
        const auto instrumentsCatalog =
            dexter::editor::makeInstrumentCatalog(embedded, externalNames);
        const auto effectsCatalog =
            dexter::editor::makeEffectCatalog(externalNames);
        for (std::size_t index = 0; index < instrumentsCatalog.size(); ++index)
            AppendMenuW(
                instruments, MF_STRING |
                    (instrumentsCatalog[index].available ? 0 : MF_GRAYED),
                static_cast<UINT_PTR>(kContextCatalogSynthBaseId + index),
                fromUtf8(instrumentsCatalog[index].label).c_str());
        for (std::size_t index = 0; index < effectsCatalog.size(); ++index)
            AppendMenuW(
                effects, MF_STRING |
                    (effectsCatalog[index].available ? 0 : MF_GRAYED),
                static_cast<UINT_PTR>(kContextCatalogEffectBaseId + index),
                fromUtf8(effectsCatalog[index].label).c_str());
        AppendMenuW(menu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(instruments),
                    L"Change instrument");
        AppendMenuW(menu, MF_POPUP,
                    reinterpret_cast<UINT_PTR>(effects),
                    L"Add effect or control");
        const auto command = TrackPopupMenu(
            menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPoint.x,
            screenPoint.y, 0, window_, nullptr);
        DestroyMenu(menu);
        if (command >= kContextCatalogSynthBaseId &&
            command < kContextCatalogSynthBaseId +
                          static_cast<int>(instrumentsCatalog.size())) {
            source = editorUtf8Source();
            std::string failure;
            if (dexter::editor::applyCatalogEntry(
                    source, position,
                    instrumentsCatalog[command - kContextCatalogSynthBaseId],
                    failure)) {
                applyEditorTextEdit(source);
                SetWindowTextW(status_, L"Instrument inserted. Click Run to apply it.");
            } else {
                SetWindowTextW(status_, fromUtf8(failure).c_str());
            }
        } else if (command >= kContextCatalogEffectBaseId &&
                   command < kContextCatalogEffectBaseId +
                                 static_cast<int>(effectsCatalog.size())) {
            source = editorUtf8Source();
            std::string failure;
            if (dexter::editor::applyCatalogEntry(
                    source, position,
                    effectsCatalog[command - kContextCatalogEffectBaseId],
                    failure)) {
                applyEditorTextEdit(source);
                SetWindowTextW(status_, L"Added to script. Click Run to apply it.");
            } else {
                SetWindowTextW(status_, fromUtf8(failure).c_str());
            }
        }
        return true;
    }

    void setEditorSource(const std::wstring& source) {
        if (!editor_)
            return;
        updatingEditor_ = true;
        setEditorWindowText(editor_, source);
        updatingEditor_ = false;
        currentSource_ = source;
        sourceInitialized_ = true;
        documentDirty_ = false;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, FALSE);
        if (activeTab_ == 0) {
            pythonSource_ = source;
            pythonSourceInitialized_ = true;
            pythonDirty_ = false;
            if (host_)
                host_->setEditorSource(toUtf8(source));
        } else {
            superColliderSource_ = source;
            superColliderSourceInitialized_ = true;
            superColliderDirty_ = false;
            if (host_)
                host_->setSuperColliderSource(toUtf8(source));
        }
        updateSaveButton();
    }

    bool confirmReplaceDocument() {
        if (!documentDirty_)
            return true;
        const int answer = MessageBoxW(
            window_, L"Save changes before replacing this script?",
            L"Unsaved script changes", MB_YESNOCANCEL | MB_ICONWARNING);
        if (answer == IDYES)
            return saveSource();
        return answer == IDNO;
    }

    bool chooseScriptPath(bool saveDialog, std::wstring& selectedPath) {
        const auto portableDirectory = portableScriptDirectory();
        if (portableDirectory.empty()) {
            MessageBoxW(window_,
                        L"Could not locate the VST3 folder for the Portable scripts directory.",
                        L"Portable scripts", MB_OK | MB_ICONERROR);
            return false;
        }

        std::error_code directoryError;
        std::filesystem::create_directories(portableDirectory, directoryError);
        if (directoryError) {
            std::wstring message = L"Could not create the Portable folder:\n";
            message += portableDirectory.wstring();
            MessageBoxW(window_, message.c_str(), L"Portable scripts",
                        MB_OK | MB_ICONERROR);
            return false;
        }

        struct ComApartment {
            HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            bool uninitialize = SUCCEEDED(result);
            ~ComApartment() { if (uninitialize) CoUninitialize(); }
        } apartment;
        if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE) {
            MessageBoxW(window_, L"Could not initialize Windows file dialogs.",
                        L"Portable scripts", MB_OK | MB_ICONERROR);
            return false;
        }

        IFileDialog* dialog = nullptr;
        const CLSID dialogClass = saveDialog ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
        HRESULT result = CoCreateInstance(dialogClass, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&dialog));
        if (FAILED(result) || !dialog) {
            MessageBoxW(window_, L"Could not create a Windows file dialog.",
                        L"Portable scripts", MB_OK | MB_ICONERROR);
            return false;
        }
        struct ComRelease {
            IUnknown* object;
            ~ComRelease() { if (object) object->Release(); }
        } dialogRelease{dialog};

        static constexpr COMDLG_FILTERSPEC pythonFilters[] = {
            {L"FoxDot/Python scripts (*.py)", L"*.py"},
            {L"All files (*.*)", L"*.*"}};
        static constexpr COMDLG_FILTERSPEC superColliderFilters[] = {
            {L"SuperCollider scripts (*.scd; *.sc)", L"*.scd;*.sc"},
            {L"All files (*.*)", L"*.*"}};
        const auto* filters =
            activeTab_ == 0 ? pythonFilters : superColliderFilters;
        result = dialog->SetFileTypes(2, filters);
        if (SUCCEEDED(result))
            result = dialog->SetFileTypeIndex(1);
        if (SUCCEEDED(result))
            result = dialog->SetDefaultExtension(
                activeTab_ == 0 ? L"py" : L"scd");

        IShellItem* folder = nullptr;
        if (SUCCEEDED(result))
            result = SHCreateItemFromParsingName(portableDirectory.c_str(), nullptr,
                                                 IID_PPV_ARGS(&folder));
        ComRelease folderRelease{folder};
        if (SUCCEEDED(result))
            result = dialog->SetFolder(folder);

        DWORD options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
        options |= saveDialog ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST;
        if (SUCCEEDED(result))
            result = dialog->SetOptions(options);
        if (SUCCEEDED(result) && saveDialog)
            result = dialog->SetFileName(
                activeTab_ == 0 ? L"new-pattern.py" : L"new-score.scd");
        if (FAILED(result)) {
            std::wstring message = L"Could not set the dialog folder to:\n";
            message += portableDirectory.wstring();
            MessageBoxW(window_, message.c_str(), L"Portable scripts",
                        MB_OK | MB_ICONERROR);
            return false;
        }

        result = dialog->Show(window_);
        if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            return false;
        if (FAILED(result)) {
            MessageBoxW(window_, L"The file dialog could not be displayed.",
                        L"Portable scripts", MB_OK | MB_ICONERROR);
            return false;
        }

        IShellItem* selectedItem = nullptr;
        result = dialog->GetResult(&selectedItem);
        ComRelease selectedRelease{selectedItem};
        if (FAILED(result) || !selectedItem)
            return false;

        PWSTR selectedFilePath = nullptr;
        result = selectedItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedFilePath);
        if (FAILED(result) || !selectedFilePath)
            return false;
        selectedPath = selectedFilePath;
        CoTaskMemFree(selectedFilePath);
        return true;
    }

    void openSource() {
        std::wstring selectedPath;
        if (!chooseScriptPath(false, selectedPath) || !confirmReplaceDocument())
            return;

        const std::filesystem::path path(selectedPath);
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error || size > kMaxScriptBytes) {
            MessageBoxW(window_,
                        error ? L"Could not read that script file."
                              : L"Script files must be 256 KB or smaller.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }

        std::ifstream input(path, std::ios::binary);
        if (!input) {
            MessageBoxW(window_, L"Could not open that script file.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }
        std::string source(static_cast<std::size_t>(size), '\0');
        if (!source.empty())
            input.read(source.data(), static_cast<std::streamsize>(source.size()));
        if ((!input && !source.empty()) || source.find('\0') != std::string::npos) {
            MessageBoxW(window_, L"That file could not be read as a text script.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }
        if (source.size() >= 3 &&
            static_cast<unsigned char>(source[0]) == 0xEF &&
            static_cast<unsigned char>(source[1]) == 0xBB &&
            static_cast<unsigned char>(source[2]) == 0xBF) {
            source.erase(0, 3);
        }
        const auto wideSource = fromUtf8(source);
        if (!source.empty() && wideSource.empty()) {
            MessageBoxW(window_, L"Scripts must use valid UTF-8 text.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }
        if (wideSource.size() > kMaxScriptCharacters) {
            MessageBoxW(window_, L"Script files must contain 65,536 characters or fewer.",
                        L"Open script", MB_OK | MB_ICONERROR);
            return;
        }

        setEditorSource(wideSource);
        currentFilePath_ = selectedPath;
        persistActiveDocument();
        updateFileName();
        SetWindowTextW(status_, activeTab_ == 0
            ? L"Python script loaded. Press Run for compatibility check and evaluation."
            : L"SuperCollider script loaded. Press Run to send it to the embedded engine.");
    }

    bool saveSourceToPath(const std::wstring& path) {
        const auto source = toUtf8(editorSource());
        if (source.size() > kMaxScriptBytes) {
            MessageBoxW(window_, L"Scripts must be 256 KiB or smaller.",
                        L"Save script", MB_OK | MB_ICONERROR);
            return false;
        }
        std::ofstream output(std::filesystem::path(path),
                             std::ios::binary | std::ios::trunc);
        if (!output) {
            MessageBoxW(window_, L"Could not write that script file.",
                        L"Save script", MB_OK | MB_ICONERROR);
            return false;
        }
        output.write(source.data(), static_cast<std::streamsize>(source.size()));
        output.flush();
        if (!output) {
            MessageBoxW(window_, L"An error occurred while saving the script.",
                        L"Save script", MB_OK | MB_ICONERROR);
            return false;
        }
        return true;
    }

    bool saveSourceAs() {
        std::wstring selectedPath;
        if (!chooseScriptPath(true, selectedPath))
            return false;
        if (!saveSourceToPath(selectedPath))
            return false;
        currentFilePath_ = std::move(selectedPath);
        updateFileName();
        currentSource_ = editorSource();
        sourceInitialized_ = true;
        documentDirty_ = false;
        persistActiveDocument();
        updateSaveButton();
        SetWindowTextW(status_, activeTab_ == 0
            ? L"Python script saved." : L"SuperCollider script saved.");
        return true;
    }

    bool saveSource() {
        if (currentFilePath_.empty())
            return saveSourceAs();
        if (!saveSourceToPath(currentFilePath_))
            return false;
        currentSource_ = editorSource();
        sourceInitialized_ = true;
        documentDirty_ = false;
        persistActiveDocument();
        updateSaveButton();
        SetWindowTextW(status_, activeTab_ == 0
            ? L"Python script saved." : L"SuperCollider script saved.");
        return true;
    }

    void runSource() {
        if (!editor_ || !host_)
            return;
        copyDiagnosticPinned_ = false;
        lastDiagnostic_.clear();
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, FALSE);
        const auto source = toUtf8(editorSource());
        if (source.size() > kMaxScriptBytes) {
            SetWindowTextW(status_, L"Script exceeds the 256 KiB size limit.");
            return;
        }
        if (host_->scriptWorkerBusy()) {
            SetWindowTextW(
                status_,
                L"The previous script is still processing. Wait for Run to be ready before submitting another.");
            return;
        }
        if (activeTab_ == 1) {
            if (!superColliderExecutionWarningShown_) {
                const int answer = MessageBoxW(
                    window_,
                    L"This tab runs full SuperCollider code through bundled sclang. "
                    L"It can read or write files, start processes, and make network "
                    L"connections. Run only code you trust. Continue?",
                    L"Unrestricted SuperCollider code",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (answer != IDYES) {
                    SetWindowTextW(status_, L"SuperCollider code was not run.");
                    return;
                }
                superColliderExecutionWarningShown_ = true;
            }
            host_->setSuperColliderSource(source);
            host_->submitSuperColliderCode(source);
            SetWindowTextW(status_,
                L"Queued for headless sclang; it uses this VST's embedded engine.");
            return;
        }
        const auto report = host_->checkCompatibility(source);
        if (report.rfind("No unsupported constructs detected", 0) != 0) {
            if (report.rfind(
                    "The Python script worker is still processing the previous Run.",
                    0) == 0) {
                SetWindowTextW(status_, fromUtf8(report).c_str());
                return;
            }
            const auto message = fromUtf8(report);
            lastDiagnostic_ = message.empty()
                                  ? L"Compatibility check failed; script was not run."
                                  : message;
            copyDiagnosticPinned_ = true;
            if (copyErrorButton_)
                EnableWindow(copyErrorButton_, TRUE);
            SetWindowTextW(status_, L"Compatibility items found; script was not run. Click Copy Error for details.");
            return;
        }
        host_->submitCode(source);
        updateRunButtonState();
        SetWindowTextW(status_, L"Compatibility check passed; evaluating Python...");
    }

    void copyLastError() {
        if (lastDiagnostic_.empty()) {
            SetWindowTextW(status_, L"There is no error or compatibility report to copy yet.");
            return;
        }
        if (!OpenClipboard(window_)) {
            SetWindowTextW(status_, L"Clipboard is busy. Try Copy Error again.");
            return;
        }
        EmptyClipboard();
        const SIZE_T bytes = (lastDiagnostic_.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory) {
            CloseClipboard();
            SetWindowTextW(status_, L"Could not allocate clipboard text.");
            return;
        }
        auto* destination = static_cast<wchar_t*>(GlobalLock(memory));
        if (!destination) {
            GlobalFree(memory);
            CloseClipboard();
            SetWindowTextW(status_, L"Could not lock clipboard text.");
            return;
        }
        std::memcpy(destination, lastDiagnostic_.c_str(), bytes);
        GlobalUnlock(memory);
        if (!SetClipboardData(CF_UNICODETEXT, memory)) {
            GlobalFree(memory);
            CloseClipboard();
            SetWindowTextW(status_, L"Windows could not place the report on the clipboard.");
            return;
        }
        CloseClipboard();
        copyDiagnosticPinned_ = true;
        SetWindowTextW(status_, L"Full error/report copied to the clipboard.");
    }

    void refreshStatus() {
        if (!host_ || !status_)
            return;
        updateRunButtonState();
        const int gainPosition = static_cast<int>(
            std::lround(std::clamp(host_->masterOutputGain(), 0.0f,
                                   kDexterMasterGainMaximum) * 100.0f));
        if (masterGainSlider_)
            SendMessageW(masterGainSlider_, TBM_SETPOS, TRUE, gainPosition);
        updateMasterGainText(gainPosition);
        const auto restoredSource = activeTab_ == 0
            ? fromUtf8(host_->editorSource())
            : fromUtf8(host_->superColliderSource());
        const bool hasRestoredSource = activeTab_ == 0
            ? host_->hasEditorSource() : host_->hasSuperColliderSource();
        if (!documentDirty_ && hasRestoredSource &&
            restoredSource != currentSource_) {
            currentFilePath_.clear();
            setEditorSource(restoredSource);
        }
        if (copyDiagnosticPinned_)
            return;
        const auto statusText = activeTab_ == 0
            ? host_->editorStatus() : host_->superColliderStatus();
        const auto message = fromUtf8(statusText);
        const bool hasDiagnostic =
            message.find(L"Traceback") != std::wstring::npos ||
            message.find(L"Error") != std::wstring::npos ||
            message.find(L"error") != std::wstring::npos ||
            message.find(L"exception") != std::wstring::npos ||
            message.find(L"invalid") != std::wstring::npos ||
            message.find(L"malformed") != std::wstring::npos ||
            message.find(L"failed") != std::wstring::npos ||
            message.find(L"unsupported") != std::wstring::npos ||
            message.find(L"overlap") != std::wstring::npos ||
            message.find(L"exceeds") != std::wstring::npos ||
            message.find(L"Could not") != std::wstring::npos ||
            message.find(L"could not") != std::wstring::npos ||
            message.find(L"timed out") != std::wstring::npos ||
            message.find(L"Timed out") != std::wstring::npos;
        if (hasDiagnostic) {
            lastDiagnostic_ = message;
            copyDiagnosticPinned_ = true;
        } else {
            lastDiagnostic_.clear();
        }
        if (copyErrorButton_)
            EnableWindow(copyErrorButton_, hasDiagnostic ? TRUE : FALSE);
        const auto newline = message.find_first_of(L"\r\n");
        const auto visibleMessage = newline == std::wstring::npos
            ? message : message.substr(0, newline);
        SetWindowTextW(status_, visibleMessage.c_str());
    }

    void updateRunButtonState() {
        if (!host_ || !runButton_)
            return;
        const bool busy = host_->scriptWorkerBusy();
        EnableWindow(runButton_, busy ? FALSE : TRUE);
        SetWindowTextW(runButton_, busy ? L"Busy" : L"Run");
    }

    ICodeEditorHost* host_ = nullptr;
    Steinberg::FUnknown* keepAlive_ = nullptr;
    bool scintillaClassesAcquired_ = false;
    Steinberg::IPlugFrame* frame_ = nullptr;
    HWND window_ = nullptr;
    HWND editor_ = nullptr;
    HWND pythonEditor_ = nullptr;
    HWND superColliderEditor_ = nullptr;
    HWND pythonTabButton_ = nullptr;
    HWND superColliderTabButton_ = nullptr;
    int activeTab_ = 0;
    HWND runButton_ = nullptr;
    HWND stopButton_ = nullptr;
    HWND openButton_ = nullptr;
    HWND saveButton_ = nullptr;
    HWND saveAsButton_ = nullptr;
    HWND copyErrorButton_ = nullptr;
    HWND status_ = nullptr;
    HWND brand_ = nullptr;
    HWND fileName_ = nullptr;
    HWND masterGainSlider_ = nullptr;
    HWND masterGainValue_ = nullptr;
    HWND masterGainLabel_ = nullptr;
    HWND panelTabs_ = nullptr;
    HWND panelList_ = nullptr;
    HWND panelMuteButton_ = nullptr;
    HWND panelStopButton_ = nullptr;
    HWND panelSamplesButton_ = nullptr;
    HWND panelVersion_ = nullptr;
    std::array<HWND, 7> playerControlLabels_{};
    std::array<HWND, 7> playerControlSliders_{};
    std::array<HWND, 7> playerControlRanges_{};
    int activePlayerSlider_ = -1;
    std::array<HWND, 7> playerControlValues_{};
    std::array<EditorPlayerState, 8> lastPanelPlayers_{};
    std::vector<dexter::editor::CatalogEntry> panelCatalog_;
    std::unordered_map<std::string, bool> collapsedCatalogCategories_;
    std::unordered_map<std::string, bool> collapsedCatalogSubcategories_;
    std::unordered_map<int, std::string> catalogCategoryHeaderRows_;
    std::unordered_map<int, std::pair<std::string, std::string>>
        catalogSubcategoryHeaderRows_;
    std::array<bool, 8> expandedEffectChains_{};
    bool panelPlayersInitialized_ = false;
    bool panelEffectsInitialized_ = false;
    bool panelSynthsInitialized_ = false;
    bool activeEffectSummaryExpanded_ = false;
    int renderedPanelTab_ = -1;
    std::vector<std::string> lastPanelSynthNames_;
    dexter::editor::CatalogEntry dragCatalogEntry_{};
    POINT catalogDragOrigin_{};
    bool dragCatalogPending_ = false;
    bool catalogDragActive_ = false;
    std::wstring lastDiagnostic_;
    bool copyDiagnosticPinned_ = false;
    std::wstring currentFilePath_;
    std::wstring currentSource_;
    std::wstring pythonFilePath_;
    std::wstring superColliderFilePath_;
    std::wstring pythonSource_;
    std::wstring superColliderSource_;
    bool updatingEditor_ = false;
    bool documentDirty_ = false;
    bool sourceInitialized_ = false;
    bool pythonDirty_ = false;
    bool superColliderDirty_ = false;
    bool pythonSourceInitialized_ = false;
    bool superColliderSourceInitialized_ = false;
    bool superColliderExecutionWarningShown_ = false;
};
} // namespace

Steinberg::IPlugView* createCodeEditorView(ICodeEditorHost* host,
                                          Steinberg::FUnknown* keepAlive) {
    return new CodeEditorView(host, keepAlive);
}

#else

Steinberg::IPlugView* createCodeEditorView(ICodeEditorHost*, Steinberg::FUnknown*) {
    return nullptr;
}

#endif
