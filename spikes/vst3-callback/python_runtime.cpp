           raise TypeError('Group accepts only players p1 through p8.')
        if len(set(player.number for player in players)) != len(players):
            raise ValueError('Group cannot contain the same player twice.')
        self.__dict__['players'] = list(players)

    def __len__(self):
        return len(self.players)

    def __iter__(self):
        return iter(self.players)

    def __getattr__(self, name):
        if name in _GROUP_PATTERN_FIELDS:
            fieldName = _GROUP_PATTERN_FIELDS[name]
            values = []
            for player in self.players:
                pattern = player._currentPattern
                if pattern is None:
                    values.append(None)
                elif name == 'degree':
                    if pattern.degreeControl is not None:
                        values.append(pattern.degreeControl)
                    else:
                        shift = ((pattern.baseOctave - 5) *
                                 pattern.degreesPerOctave)
                        values.append(_group_shift_notes(
                            pattern.notes, -shift) if pattern.degrees else
                            pattern.notes)
                elif name == 'oct':
                    values.append(pattern.octaveControl
                                  if pattern.octaveControl is not None else
                                  pattern.baseOctave)
                elif name == 'root':
                    values.append(pattern.rootControl
                                  if pattern.rootControl is not None else
                                  pattern.rootMidi)
                else:
                    values.append(pattern.__dict__.get(fieldName))
            return values
        raise AttributeError('unsupported Group attribute ' + name)

    def __setattr__(self, name, value):
        if name not in _GROUP_PATTERN_FIELDS:
            raise AttributeError('Group assignments support shared sound/effect and pitch/timing controls.')
        value = _normalize_group_control(name, value)
        prepared = []
        for player in self.players:
            if player._currentPattern is not None:
                copiedPattern = PlayerPattern.__new__(PlayerPattern)
                copiedPattern.__dict__ = dict(player._currentPattern.__dict__)
                _set_group_pattern_control(copiedPattern, name, value)
                prepared.append((player, copiedPattern))
        for player in self.players:
            player._groupOverrides[name] = value
        for player, pattern in prepared:
            player.__rshift__(pattern)
        return self

    def add(self, player):
        if not isinstance(player, Player) or not 1 <= player.number <= 8:
            raise TypeError('Group.add accepts only a player from p1 through p8.')
        if player.number in [item.number for item in self.players]:
            raise ValueError('Group already contains that player.')
        if len(self.players) >= 8:
            raise ValueError('Group is limited to eight players.')
        self.players.append(player)
        return self

    def stop(self):
        for player in self.players:
            player.stop()
        return self

    def mute(self):
        for player in self.players:
            player.mute()
        return self

    def unmute(self):
        for player in self.players:
            player.unmute()
        return self

    def solo(self, enabled=True):
        if not isinstance(enabled, bool):
            raise TypeError('Group.solo accepts a boolean.')
        selected = set(player.number - 1 for player in self.players)
        for index in range(8):
            _dexter_mutes[index] = bool(enabled and index not in selected)
        return self

    def only(self):
        selected = set(player.number for player in self.players)
        for player in _DEXTER_PLAYERS:
            if player.number not in selected and player._active:
                player.stop()
        return self

    def _schedule(self, method, *args, **kwargs):
        for player in self.players:
            if method == 'every':
                player.every(*args, **kwargs)
            elif method == 'often':
                player.often(*args, **kwargs)
            elif method == 'sometimes':
                player.sometimes(*args, **kwargs)
            elif method == 'rarely':
                player.rarely(*args, **kwargs)
        return self

    def every(self, *args, **kwargs):
        return self._schedule('every', *args, **kwargs)

    def often(self, *args, **kwargs):
        return self._schedule('often', *args, **kwargs)

    def sometimes(self, *args, **kwargs):
        return self._schedule('sometimes', *args, **kwargs)

    def rarely(self, *args, **kwargs):
        return self._schedule('rarely', *args, **kwargs)

def Master():
    return Group(*(player for player in _DEXTER_PLAYERS if player._active))

p_all = Group(*_DEXTER_PLAYERS)

class _PlayerDegreeReference:
    def __init__(self, player):
        self.player = player

# Explicit VST extensions, not claims of upstream FoxDot Buffer/sclang APIs.
def _disk_number(value, name, lo, hi):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise TypeError(name + ' requires a finite scalar number')
    value = float(value)
    if value != value or not lo <= value <= hi:
        raise ValueError(name + ' is outside the supported range')
    return value

def _disk_slot(slot):
    if isinstance(slot, bool) or not isinstance(slot, int) or not 1 <= slot <= 8:
        raise ValueError('Disk stream slot must be a whole number from 1 to 8')
    return slot - 1

def _disk_command(command):
    if len(_dexter_disk_commands) >= 16:
        raise ValueError('One Run may contain at most 16 disk lifecycle operations')
    _dexter_disk_commands.append(command)

class _DiskReader:
    def __init__(self, mode):
        self.mode = mode
    def play(self, path, slot=1, loop=False, amp=0.5, pan=0.0, rate=1.0):
        if not isinstance(path, str) or not 1 <= len(path) <= 512:
            raise ValueError('Disk input needs a relative file path under Portable/Samples')
        normalized = path.replace('\\', '/')
        if (normalized.startswith('/') or ':' in normalized or
                any(part in ('', '.', '..') for part in normalized.split('/')) or
                any(ord(character) < 32 for character in normalized)):
            raise ValueError('Disk input cannot be absolute, escape Portable/Samples, or contain control characters')
        if not isinstance(loop, bool):
            raise TypeError('Disk loop must be True or False')
        rate = _disk_number(rate, 'Disk rate', 0.0625, 8.0)
        if self.mode == 1 and rate != 1.0:
            raise ValueError('DiskIn has no rate control; use VDiskIn.play(..., rate=...)')
        _disk_command((self.mode, _disk_slot(slot), rate,
                       _disk_number(amp, 'Disk amp', 0.0, 2.0),
                       _disk_number(pan, 'Disk pan', -1.0, 1.0), 0.0,
                       int(loop), normalized.encode('utf-8').hex()))
    def stop(self, slot=1):
        _disk_command((0, _disk_slot(slot), 1.0, 0.0, 0.0, 0.0, 0, '-'))

class _DiskRecorder:
    def record(self, seconds=30.0):
        _disk_command((3, 0, 1.0, 0.0, 0.0,
                       _disk_number(seconds, 'Recording seconds', 0.05, 3600.0), 0, '-'))
    def stop(self):
        _disk_command((4, 0, 1.0, 0.0, 0.0, 0.0, 0, '-'))

DiskIn = _DiskReader(1)
VDiskIn = _DiskReader(2)
DiskOut = _DiskRecorder()
)PY";

    std::ostringstream script;
    script << "import builtins, traceback, sys, time, dis, types, math\n"
           << "_dexter_scope = globals().setdefault(" << pythonLiteral(namespaceName) << ", {})\n"
           << "_dexter_scope['__name__'] = 'dexter'\n"
           << "_dexter_scope.update({'_dexter_events': [], '_dexter_bpm': 120.0, '_dexter_bpm_control': None, "
           << "'_dexter_length': 1.0, '_dexter_slots': 0, '_dexter_updates': {}, '_dexter_default_touched': False, "
             << "'_dexter_scale': (0, 2, 4, 5, 7, 9, 11), '_dexter_root': 60, "
             << "'_dexter_root_control': None, '_dexter_mutes': {}, "
            << "'_dexter_swing_seconds': 0.0, '_dexter_disk_commands': [], "
           << "'_dexter_shuffle_seed': int(time.monotonic_ns() & 0xffffffff)})\n"
           << "_dexter_scope['__builtins__'] = {name: getattr(builtins, name) for name in "
            << "('abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter', 'float', "
           << "'int', 'isinstance', 'len', 'list', 'map', 'max', 'min', 'range', 'ord', 'chr', "
           << "'reversed', 'round', 'set', 'slice', 'sorted', 'str', 'sum', 'tuple', "
           << "'zip', 'ValueError', 'TypeError', 'ZeroDivisionError', "
              << "'AttributeError', 'Exception', 'IndexError', 'OverflowError', "
              << "'__build_class__')}\n"
             << "_dexter_scope['_dexter_dis'] = dis\n"
             << "_dexter_scope['_dexter_math'] = math\n"
            << "_dexter_scope['_dexter_function_type'] = types.FunctionType\n"
            << "_dexter_scope['_dexter_builtin_module'] = builtins\n"
            << "_dexter_scope['_dexter_pure_map_builtins'] = {name: getattr(builtins, name) for name in "
            << "('abs', 'bool', 'float', 'int', 'len', 'max', 'min', 'round', 'str')}\n"
           << "exec(compile(" << pythonLiteral(api) << ", '<Dexter API>', 'exec'), "
            << "_dexter_scope, _dexter_scope)\n"
            << "_dexter_scope['_dexter_user_synth_names'] = "
            << pythonStringTuple(userSynthNames) << "\n"
           << "_dexter_deadline = time.monotonic() + 2.0\n"
           << "def _dexter_time_limit(frame, event, arg):\n"
           << "    if time.monotonic() > _dexter_deadline:\n"
           << "        raise TimeoutError('live-code evaluation exceeded 2 seconds')\n"
           << "    return _dexter_time_limit\n"
           << "sys.settrace(_dexter_time_limit)\n"
           << "try:\n"
           << "    try:\n"
           << "        with open(" << pythonLiteral(sourcePath) << ", 'r', encoding='utf-8') as _f:\n"
           << "            _src = _f.read()\n"
           << "        exec(compile(_src, '<Dexter live code>', 'exec'), _dexter_scope, _dexter_scope)\n"
           << "    finally:\n"
           << "        sys.settrace(None)\n"
           << "    with open(" << pythonLiteral(resultPath) << ", 'w', encoding='utf-8') as _f:\n"
            << "        if _dexter_scope['_dexter_default_touched']:\n"
             << "            _dexter_scope['_dexter_updates'][0] = (False, _dexter_scope['_dexter_bpm'], _dexter_scope['_dexter_length'], list(_dexter_scope['_dexter_events']), 0, 1.0, 0.0, 20000.0, 0.0, 0.0, False, False, 0.01, 0.04, '-', '-', '-', '-', '-', 1, 0.1, 0.0, 0.5, 3.0, 15.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, None, None, None, None, None, None, 1.0, None, 0.35, '-', -1.0, '-', '-', 0.0, 0.0, None, 0.0, None, 0.0, None, 0.0, None) + (_dexter_scope['_dexter_swing_seconds'], 0, 1.0, 0, 0, 0)\n"
            << "        _updates = _dexter_scope['_dexter_updates']\n"
            << "        _mutes = _dexter_scope['_dexter_mutes']\n"
            << "        _f.write('OK\\t{}\\t{}\\t{}\\n'.format(len(_updates), len(_mutes), len(_dexter_scope['_dexter_disk_commands'])))\n"
            << "        for _player in sorted(_updates):\n"
              << "            _stopped, _bpm, _length, _events, _synth, _amp, _pan, _cutoff, _echo, _delay, _mouse_pan, _mouse_cutoff, _attack, _release, _amp_control, _pan_control, _cutoff_control, _echo_control, _delay_control, _reverb, _mix, _room, _damp, _revtime, _spread, _lpf, _lpr, _hpf, _hpr, _bpf, _bpr, _lpf_control, _lpr_control, _hpf_control, _hpr_control, _bpf_control, _bpr_control, _decay, _decay_control, _echo_mix, _echo_mix_control, _feedback, _feedback_control, _note_delay_control, _synth_rate, _bpnoise, _bpnoise_control, _swell, _swell_control, _shape, _shape_control, _drive, _drive_control, _swing_seconds, _until_cycles, _chance_probability, _chance_seed, _bar_loaf, _bar_on_mask = _updates[_player]\n"
             << "            _bpm = _dexter_scope['_dexter_bpm']\n"
             << "            _tempo_control = _dexter_scope['_dexter_bpm_control']\n"
             << "            _p_values = (_player, int(_stopped), _bpm, _length, len(_events), _synth, _amp, _pan, _cutoff, _echo, _delay, int(_mouse_pan), int(_mouse_cutoff), _attack, _release, _amp_control, _pan_control, _cutoff_control, _echo_control, _delay_control, _synth_rate, _swing_seconds, _until_cycles)\n"
             << "            _f.write('P' + chr(9) + chr(9).join(map(str, _p_values)) + chr(10))\n"
              << "            _f.write('C\\t{}\\t{}\\t{}\\t{}\\t{}\\n'.format(_player, _chance_probability, _chance_seed, _bar_loaf, _bar_on_mask))\n"
             << "            _f.write('T\\t{}\\t{}\\n'.format(_player, _tempo_control.wire() if _tempo_control is not None else '-'))\n"
            << "            _f.write('R\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, _reverb, _mix, _room, _damp, _revtime, _spread))\n"
             << "            _f.write('E\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, _decay, _decay_control.wire() if _decay_control is not None else '-', _echo_mix, _echo_mix_control, _feedback, _feedback_control if _feedback_control is not None else '-', _note_delay_control.wire() if hasattr(_note_delay_control, 'wire') else (_note_delay_control if _note_delay_control is not None else '-')))\n"
              << "            _controls = (_lpf_control, _lpr_control, _hpf_control, _hpr_control, _bpf_control, _bpr_control)\n"
             << "            _f.write('F\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\\n'.format(_player, _lpf, _lpr, _hpf, _hpr, _bpf, _bpr, *[control.wire() if control is not None else '-' for control in _controls]))\n"
            << "            _bits, _bits_control, _crush, _crush_control = _dexter_scope['_dexter_bitcrusher_controls'].get(_player, (0.0, '-', 1.0, '-'))\n"
            << "            _modulation_values = (_player, _bpnoise, _swell, _shape, _drive, _bpnoise_control.wire() if _bpnoise_control is not None else '-', _swell_control.wire() if _swell_control is not None else '-', _shape_control.wire() if _shape_control is not None else '-', _drive_control.wire() if _drive_control is not None else '-', _bits, _bits_control, _crush, _crush_control)\n"
            << "            _f.write('G' + chr(9) + chr(9).join(map(str, _modulation_values)) + chr(10))\n"
            << "            _synth_controls = _dexter_scope['_dexter_synth_controls'].get(_player, (0.0, 0.5, 1.0, 0.5, 0.5, 0.0, 1.0))\n"
            << "            _f.write('S' + chr(9) + str(_player) + chr(9) + chr(9).join(map(str, _synth_controls)) + chr(10))\n"
              << "            _effect_chain = _dexter_scope['_dexter_player_effect_chains'].get(_player, ())\n"
              << "            _effect_user_indices = tuple(item[0] for item in _effect_chain)\n"
              << "            _chain_values = tuple(item[1] for item in _effect_chain)\n"
              << "            _chain_variables = tuple(item[2] for item in _effect_chain)\n"
              << "            _effect_user_index = _effect_user_indices[0] if _effect_user_indices else -1\n"
              << "            _effect_control_values = _chain_values[0] if _chain_values else (0.0, 0.5, 1.0, 0.5, 0.5, 0.0, 1.0)\n"
              << "            _effect_control_variables = _chain_variables[0] if _chain_variables else ('-', '-', '-', '-', '-', '-', '-')\n"
              << "            _f.write('A' + chr(9) + str(_player) + chr(9) + str(_effect_user_index) + chr(10))\n"
              << "            _f.write('W' + chr(9) + str(_player) + chr(9) + chr(9).join(map(str, _effect_control_values)) + chr(10))\n"
              << "            _f.write('X' + chr(9) + str(_player) + chr(9) + chr(9).join(_effect_control_variables) + chr(10))\n"
              << "            _extra_effects = [value for index, values, variables in zip(_effect_user_indices[1:], _chain_values[1:], _chain_variables[1:]) for value in (index,) + tuple(values) + tuple(variables)]\n"
              << "            _f.write('J' + chr(9) + str(_player) + chr(9) + str(max(0, len(_effect_user_indices) - 1)) + ''.join(chr(9) + str(value) for value in _extra_effects) + chr(10))\n"
              << "            _note_effect_control_variables = _dexter_scope['_dexter_note_effect_control_variables'].get(_player, ('-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-'))\n"
             << "            _f.write('Y' + chr(9) + str(_player) + chr(9) + chr(9).join(_note_effect_control_variables) + chr(10))\n"
             << "            if _synth == 57:\n"
             << "                _sample_control, _rate_control = _dexter_scope['_dexter_sample_controls'].get(_player, ('-', '-'))\n"
             << "                _f.write('Q' + chr(9) + str(_player) + chr(9) + _sample_control + chr(9) + _rate_control + chr(10))\n"
            << "            for _event_index, _e in enumerate(_events):\n"
               << "                _fx = _e[10:18] if len(_e) >= 18 else (0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)\n"
               << "                _cut = _e[18:19] if len(_e) >= 19 else (1.0,)\n"
               << "                _extra_note_fx = _e[19:22] if len(_e) >= 22 else (0.0, -1.0, 0.0)\n"
               << "                _event_fx = _e[22:30] if len(_e) >= 30 else (-1, 0.0, 0.5, 1.0, 0.5, 0.5, 0.0, 1.0)\n"
               << "                _f.write('N' + chr(9) + chr(9).join(map(str, _e[:7] + _fx + _cut + _extra_note_fx + _event_fx)) + chr(10))\n"
            << "                if _synth == 57:\n"
            << "                    _f.write('B\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\t{}\\n'.format(*_dexter_scope['_dexter_sample_events'][_player][_event_index]))\n"
            << "        for _player, _muted in sorted(_mutes.items()):\n"
            << "            _f.write('M\\t{}\\t{}\\n'.format(_player, int(_muted)))\n"
            << "        for _command in _dexter_scope['_dexter_disk_commands']:\n"
            << "            _f.write('D' + chr(9) + chr(9).join(map(str, _command)) + chr(10))\n"
            << "except BaseException:\n"
           << "    with open(" << pythonLiteral(resultPath) << ", 'w', encoding='utf-8') as _f:\n"
           << "        _f.write('ERROR\\n' + traceback.format_exc())\n";
    return script.str();
}

bool parseResult(const std::filesystem::path& resultPath,
                 PythonPattern& pattern,
                 std::size_t userSynthCount,
                 std::string& error) {
    std::ifstream input(resultPath, std::ios::binary);
    if (!input) {
        error = "Python did not produce a result file.";
        return false;
    }
    std::string header;
    std::getline(input, header);
    // Python opens text files with CRLF on Windows; std::getline removes LF only.
    if (!header.empty() && header.back() == '\r')
        header.pop_back();
    if (header == "ERROR") {
        std::ostringstream message;
        message << input.rdbuf();
        error = message.str();
        return false;
    }
    std::istringstream fields(header);
    std::string marker;
    std::size_t updateCount = 0;
    std::size_t muteCount = 0;
    if (!(fields >> marker >> updateCount >> muteCount) || marker != "OK" ||
        updateCount > 8 || muteCount > 8) {
        error = "Python returned an invalid player update header: " + header;
        return false;
    }
    std::size_t diskCount = 0;
    fields >> std::ws;
    if (!fields.eof() && (!(fields >> diskCount) || diskCount > 16)) {
        error = "Python returned an invalid DiskIO command count."; return false;
    }
    pattern.diskCommands.clear();
    pattern.players.clear();
    pattern.players.reserve(updateCount);
    pattern.mutes.clear();
    pattern.mutes.reserve(muteCount);
    std::array<bool, 8> seenPlayers{};
    for (std::size_t playerNumber = 0; playerNumber < updateCount; ++playerNumber) {
        char record = 0;
        int stopped = 0;
        std::size_t eventCount = 0;
        PythonPlayerUpdate update;
        int mousePan = 0;
        int mouseCutoff = 0;
        if (!(input >> record >> update.playerIndex >> stopped >> update.bpm >>
              update.lengthBeats >> eventCount >> update.synthIndex >> update.amplitude >>
              update.pan >> update.cutoffHz >> update.echoBeats >> update.delayTimeSeconds >>
              mousePan >> mouseCutoff >> update.attackSeconds >> update.releaseSeconds >>
              update.ampControlToken >> update.panControlToken >> update.cutoffControlToken >>
              update.echoControlToken >> update.delayTimeControlToken >> update.synthRate >>
              update.swingSeconds >> update.untilCycles) ||
            record != 'P' || update.playerIndex < 0 || update.playerIndex >= 8 ||
            stopped < 0 || stopped > 1 ||
            seenPlayers[static_cast<std::size_t>(update.playerIndex)] || eventCount > 256 ||
            !std::isfinite(update.bpm) || update.bpm < 0.001 ||
             !std::isfinite(update.lengthBeats) || update.lengthBeats <= 0.0 ||
               update.lengthBeats > 4096.0 || update.synthIndex < 0 ||
               static_cast<std::size_t>(update.synthIndex) >=
                    kEmbeddedSynthCount + userSynthCount ||
             !std::isfinite(update.synthRate) || update.synthRate < 0.0 ||
             update.synthRate > 10.0 ||
              (update.synthIndex == 5 && update.synthRate > 1.0) ||
              !std::isfinite(update.swingSeconds) ||
              std::abs(update.swingSeconds * update.bpm / 60.0) >= 0.49 ||
              update.untilCycles < 0 || update.untilCycles > 256 ||
              (update.untilCycles > 0 &&
               update.lengthBeats * update.untilCycles > 4096.0) ||
            !std::isfinite(update.amplitude) || update.amplitude < 0.0 || update.amplitude > 1.0 ||
            !std::isfinite(update.pan) || update.pan < -1.0 || update.pan > 1.0 ||
            !std::isfinite(update.cutoffHz) || update.cutoffHz < 20.0 ||
            update.cutoffHz > 20000.0 || !std::isfinite(update.echoBeats) ||
            update.echoBeats < 0.0 || update.echoBeats > 64.0 ||
            !std::isfinite(update.delayTimeSeconds) || update.delayTimeSeconds < 0.0 ||
            update.delayTimeSeconds > 4.0 || mousePan < 0 || mousePan > 1 ||
            mouseCutoff < 0 || mouseCutoff > 1 ||
            !std::isfinite(update.attackSeconds) || update.attackSeconds < 0.0 ||
            update.attackSeconds > 3.0 || !std::isfinite(update.releaseSeconds) ||
            update.releaseSeconds < 0.0 || update.releaseSeconds > 3.0) {
            std::ostringstream detail;
            detail << "Python returned malformed player update data (record=" << record
                   << ", player=" << update.playerIndex + 1
                   << ", stopped=" << stopped << ", events=" << eventCount
                   << ", BPM=" << update.bpm << ", synth=" << update.synthIndex
                   << "). Check the copied diagnostic for the full exception.";
            error = detail.str();
            return false;
        }
        char chanceRecord = 0;
        int chancePlayer = -1;
        long long chanceSeed = -1;
        long long barLoaf = -1;
        long long barOnMask = -1;
        if (!(input >> chanceRecord >> chancePlayer >> update.chanceProbability >>
              chanceSeed >> barLoaf >> barOnMask) ||
            chanceRecord != 'C' || chancePlayer != update.playerIndex ||
            !std::isfinite(update.chanceProbability) ||
            update.chanceProbability < 0.0 || update.chanceProbability > 1.0 ||
            chanceSeed < 0 || chanceSeed > 4294967295LL ||
            barLoaf < 0 || barLoaf > 32 || barOnMask < 0 ||
            (barLoaf == 0 && barOnMask != 0) ||
            (barLoaf > 0 &&
             (barOnMask == 0 ||
              (static_cast<std::uint64_t>(barOnMask) >>
                   static_cast<unsigned>(barLoaf)) != 0))) {
            error = "Python returned malformed Sardine chance/loaf/on controls.";
            return false;
        }
        update.chanceSeed = static_cast<std::uint32_t>(chanceSeed);
        update.barLoaf = static_cast<std::uint16_t>(barLoaf);
        update.barOnMask = static_cast<std::uint32_t>(barOnMask);
        char tempoRecord = 0;
        int tempoPlayer = -1;
        if (!(input >> tempoRecord >> tempoPlayer >> update.bpmControlToken) ||
            tempoRecord != 'T' || tempoPlayer != update.playerIndex ||
            update.bpmControlToken.size() > 256) {
            error = "Python returned malformed Clock.bpm automation.";
            return false;
        }
        if (update.bpmControlToken != "-") {
            const auto& token = update.bpmControlToken;
            const auto delimiter = token.find(':', 2);
            if (token.size() < 5 || (token[0] != 'v' && token[0] != 'l') ||
                token[1] != ':' || delimiter == std::string::npos ||
                delimiter == 2 || delimiter + 1 == token.size() ||
                token.find(':', delimiter + 1) != std::string::npos) {
                error = "Python returned malformed Clock.bpm automation.";
                return false;
            }
        }
        char effectRecord = 0;
        int effectPlayer = -1;
        if (!(input >> effectRecord >> effectPlayer >> update.reverbIndex >> update.reverbMix >>
              update.reverbRoom >> update.reverbDamp >> update.reverbTime >>
              update.reverbSpread) ||
            effectRecord != 'R' || effectPlayer != update.playerIndex ||
            update.reverbIndex < 0 || update.reverbIndex > 3 ||
            !std::isfinite(update.reverbMix) || update.reverbMix < 0.0 || update.reverbMix > 1.0 ||
            !std::isfinite(update.reverbRoom) || update.reverbRoom < 0.0 || update.reverbRoom > 1.0 ||
            !std::isfinite(update.reverbDamp) || update.reverbDamp < 0.0 || update.reverbDamp > 1.0 ||
            !std::isfinite(update.reverbTime) || update.reverbTime < 0.1 || update.reverbTime > 10.0 ||
            !std::isfinite(update.reverbSpread) || update.reverbSpread < 0.0 || update.reverbSpread > 100.0) {
            error = "Python returned malformed reverb controls.";
            return false;
        }
        char decayRecord = 0;
        int decayPlayer = -1;
        if (!(input >> decayRecord >> decayPlayer >> update.echoDecayBeats >> update.echoDecayControlToken >>
              update.echoMix >> update.echoMixControlToken >> update.echoFeedback >> update.echoFeedbackControlToken >> update.noteDelayControlToken) ||
            decayRecord != 'E' || decayPlayer != update.playerIndex ||
            !std::isfinite(update.echoDecayBeats) || update.echoDecayBeats < 0.0 || update.echoDecayBeats > 64.0 ||
            !std::isfinite(update.echoMix) || update.echoMix < 0.0 || update.echoMix > 1.0 ||
            !std::isfinite(update.echoFeedback) ||
            (update.echoFeedback < 0.0 && update.echoFeedback != -1.0) || update.echoFeedback > 0.95) {
            error = "Python returned malformed FoxDot echo and plugin echo controls.";
            return false;
        }
        char filterRecord = 0;
        int filterPlayer = -1;
        if (!(input >> filterRecord >> filterPlayer >> update.lpfHz >> update.lpr >>
              update.hpfHz >> update.hpr >> update.bpfHz >> update.bpr >>
              update.lpfControlToken >> update.lprControlToken >>
              update.hpfControlToken >> update.hprControlToken >>
              update.bpfControlToken >> update.bprControlToken) ||
            filterRecord != 'F' || filterPlayer != update.playerIndex ||
            !std::isfinite(update.lpfHz) || update.lpfHz < 0.0 || update.lpfHz > 20000.0 ||
            !std::isfinite(update.lpr) || update.lpr < 0.05 || update.lpr > 1.0 ||
            !std::isfinite(update.hpfHz) || update.hpfHz < 0.0 || update.hpfHz > 20000.0 ||
            !std::isfinite(update.hpr) || update.hpr < 0.05 || update.hpr > 1.0 ||
            !std::isfinite(update.bpfHz) || update.bpfHz < 0.0 || update.bpfHz > 20000.0 ||
            !std::isfinite(update.bpr) || update.bpr < 0.05 || update.bpr > 1.0) {
            error = "Python returned malformed filter controls.";
            return false;
        }
        const auto validPlayerEffectControlToken = [](const std::string& token,
                                                       double minimum,
                                                       double maximum) {
            if (token == "-")
                return true;
            if (token.size() > 256 || token.size() < 5 ||
                (token[0] != 'v' && token[0] != 'l') || token[1] != ':')
                return false;
            const auto middle = token.find(':', 2);
            if (middle == std::string::npos || middle == 2 ||
                middle + 1 >= token.size() ||
                token.find(':', middle + 1) != std::string::npos)
                return false;
            const auto parseList = [](const std::string& text, double minimum,
                                      double maximum, bool positive,
                                      std::vector<double>& output) {
                std::istringstream values(text);
                std::string part;
                while (std::getline(values, part, ',')) {
                    if (part.empty() || output.size() >= 8)
                        return false;
                    try {
                        std::size_t used = 0;
                        const double value = std::stod(part, &used);
                        if (used != part.size() || !std::isfinite(value) ||
                            (positive ? value <= 0.0 || value > maximum
                                      : value < minimum || value > maximum))
                            return false;
                        output.push_back(value);
                    } catch (...) {
                        return false;
                    }
                }
                return !output.empty();
            };
            std::vector<double> values;
            std::vector<double> durations;
            return parseList(token.substr(2, middle - 2), minimum, maximum, false,
                             values) &&
                   parseList(token.substr(middle + 1), 0.0, 4096.0, true,
                             durations) &&
                   values.size() == durations.size();
        };
        char modulationRecord = 0;
        int modulationPlayer = -1;
        if (!(input >> modulationRecord >> modulationPlayer >> update.bpnoise >>
              update.swell >> update.shape >> update.drive >>
              update.bpnoiseControlToken >> update.swellControlToken >>
              update.shapeControlToken >> update.driveControlToken >>
              update.bits >> update.bitsControlToken >>
              update.crush >> update.crushControlToken) ||
            modulationRecord != 'G' || modulationPlayer != update.playerIndex ||
            !std::isfinite(update.bpnoise) || update.bpnoise < 0.0 ||
            update.bpnoise > 10.0 || !std::isfinite(update.swell) ||
            update.swell < 0.0 || update.swell > 4.0 ||
            !std::isfinite(update.shape) || update.shape < 0.0 ||
            update.shape > 4.0 || !std::isfinite(update.drive) ||
            update.drive < 0.0 || update.drive > 4.0 ||
            !std::isfinite(update.bits) || update.bits < 0.0 ||
            update.bits > 24.0 ||
            !std::isfinite(update.crush) || update.crush < 1.0 ||
            update.crush > 128.0 ||
            !validPlayerEffectControlToken(update.shapeControlToken, 0.0, 4.0) ||
            !validPlayerEffectControlToken(update.driveControlToken, 0.0, 4.0) ||
            !validPlayerEffectControlToken(update.bitsControlToken, 0.0, 24.0) ||
            !validPlayerEffectControlToken(update.crushControlToken, 1.0, 128.0)) {
            error = "Python returned malformed filter-modulation controls.";
            return false;
        }
        char synthRecord = 0;
        int synthPlayer = -1;
        if (!(input >> synthRecord >> synthPlayer) ||
            synthRecord != 'S' || synthPlayer != update.playerIndex) {
            error = "Python returned malformed synth controls.";
            return false;
        }
        constexpr std::array<double, 7> minimum{0, 0, 0, 0, 0.01, -2000, 0.01};
        constexpr std::array<double, 7> maximum{100, 10, 100, 1, 3, 2000, 8};
        for (std::size_t index = 0; index < update.synthControls.size(); ++index) {
            auto& value = update.synthControls[index];
            if (!(input >> value) || !std::isfinite(value) ||
                value < minimum[index] || value > maximum[index]) {
                error = "Python returned an invalid synth control.";
                return false;
            }
        }
        char playerEffectRecord = 0;
        int playerEffectPlayer = -1;
        if (!(input >> playerEffectRecord >> playerEffectPlayer >>
              update.playerEffectUserIndex) ||
            playerEffectRecord != 'A' ||
            playerEffectPlayer != update.playerIndex ||
            update.playerEffectUserIndex < -1 ||
            (update.playerEffectUserIndex >= 0 &&
             static_cast<std::size_t>(update.playerEffectUserIndex) >=
                 userSynthCount)) {
            error = "Python returned an invalid persistent player-effect selection.";
            return false;
        }
        char effectValuesRecord = 0;
        int effectValuesPlayer = -1;
        if (!(input >> effectValuesRecord >> effectValuesPlayer) ||
            effectValuesRecord != 'W' ||
            effectValuesPlayer != update.playerIndex) {
            error = "Python returned malformed persistent-effect control values.";
            return false;
        }
        for (auto& value : update.playerEffectControlValues) {
            if (!(input >> value) || !std::isfinite(value) ||
                value < 0.0 || value > 1.0) {
                error = "Python returned invalid normalized player-effect controls.";
                return false;
            }
        }
        char effectControlsRecord = 0;
        int effectControlsPlayer = -1;
        if (!(input >> effectControlsRecord >> effectControlsPlayer) ||
            effectControlsRecord != 'X' ||
            effectControlsPlayer != update.playerIndex) {
            error = "Python returned malformed persistent-effect controls.";
            return false;
        }
        const auto validEffectControlToken = [](const std::string& token) {
            if (token == "-")
                return true;
            if (token.size() > 256 || token.size() < 5 ||
                (token[0] != 'v' && token[0] != 'l') || token[1] != ':')
                return false;
            const auto middle = token.find(':', 2);
            if (middle == std::string::npos || middle == 2 ||
                middle + 1 >= token.size() ||
                token.find(':', middle + 1) != std::string::npos)
                return false;
            const auto validNumberList = [](const std::string& text, double minimum,
                                            double maximum, bool positive,
                                            std::size_t& count) {
                if (text.empty() || text.front() == ',' || text.back() == ',' ||
                    text.find(",,") != std::string::npos)
                    return false;
                std::istringstream values(text);
                std::string part;
                count = 0;
                while (std::getline(values, part, ',')) {
                    if (part.empty() || count >= 8)
                        return false;
                    try {
                        std::size_t used = 0;
                        const double value = std::stod(part, &used);
                        if (used != part.size() || !std::isfinite(value) ||
                            (positive ? value <= 0.0 || value > maximum
                                      : value < minimum || value > maximum))
                            return false;
                    } catch (...) {
                        return false;
                    }
                    ++count;
                }
                return count > 0;
            };
            std::size_t valueCount = 0;
            std::size_t durationCount = 0;
            return validNumberList(token.substr(2, middle - 2), 0.0, 1.0,
                                  false, valueCount) &&
                   validNumberList(token.substr(middle + 1), 0.0, 4096.0,
                                   true, durationCount) &&
                   (durationCount == 1 || durationCount == valueCount);
        };
        for (auto& token : update.effectControlTokens) {
            if (!(input >> token) || !validEffectControlToken(token)) {
                error = "Python returned malformed persistent effect-control modulation.";
                return false;
            }
            if (token != "-" && update.playerEffectUserIndex < 0) {
                error = "Python returned persistent-effect modulation without an effect selection.";
                return false;
            }
        }
        update.playerEffects.clear();
        if (update.playerEffectUserIndex >= 0)
            update.playerEffects.push_back({
                update.playerEffectUserIndex, update.playerEffectControlValues,
                update.effectControlTokens});
        char chainRecord = 0;
        int chainPlayer = -1;
        std::size_t extraEffectCount = 0;
        if (!(input >> chainRecord >> chainPlayer >> extraEffectCount) ||
            chainRecord != 'J' || chainPlayer != update.playerIndex ||
            extraEffectCount >= kMaxPlayerEffectChain ||
            (extraEffectCount > 0 && update.playerEffectUserIndex < 0)) {
            error = "Python returned an invalid persistent effect-chain header.";
            return false;
        }
        for (std::size_t slot = 0; slot < extraEffectCount; ++slot) {
            PythonPlayerEffect effect;
            if (!(input >> effect.userSynthIndex) ||
                effect.userSynthIndex < 0 ||
                static_cast<std::size_t>(effect.userSynthIndex) >= userSynthCount) {
                error = "Python returned an invalid chained effect selection.";
                return false;
            }
            for (auto& value : effect.controlValues) {
                if (!(input >> value) || !std::isfinite(value) ||
                    value < 0.0 || value > 1.0) {
                    error = "Python returned invalid chained effect controls.";
                    return false;
                }
            }
            for (auto& token : effect.controlTokens) {
                if (!(input >> token) || !validEffectControlToken(token)) {
                    error = "Python returned malformed chained effect modulation.";
                    return false;
                }
            }
            update.playerEffects.push_back(std::move(effect));
        }
        const auto validNoteEffectControlToken =
            [](const std::string& token, double minimum, double maximum) {
                if (token == "-")
                    return true;
                if (token.size() > 256 || token.size() < 5 ||
                    (token[0] != 'v' && token[0] != 'l') || token[1] != ':')
                    return false;
                const auto middle = token.find(':', 2);
                if (middle == std::string::npos || middle == 2 ||
                    middle + 1 >= token.size() ||
                    token.find(':', middle + 1) != std::string::npos)
                    return false;
                const auto parseList = [](const std::string& text,
                                          double low, double high,
                                          bool durations,
                                          std::size_t& count) {
                    if (text.empty() || text.front() == ',' ||
                        text.back() == ',' || text.find(",,") != std::string::npos)
                        return false;
                    std::istringstream values(text);
                    std::string part;
                    count = 0;
                    while (std::getline(values, part, ',')) {
                        if (part.empty() || count >= 8)
                            return false;
                        try {
                            std::size_t used = 0;
                            const double value = std::stod(part, &used);
                            if (used != part.size() || !std::isfinite(value) ||
                                (durations ? value <= 0.0 || value > 4096.0
                                           : value < low || value > high))
                                return false;
                        } catch (...) {
                            return false;
                        }
                        ++count;
                    }
                    return count > 0;
                };
                std::size_t valueCount = 0;
                std::size_t durationCount = 0;
                return parseList(token.substr(2, middle - 2), minimum,
                                 maximum, false, valueCount) &&
                       parseList(token.substr(middle + 1), 0.0, 4096.0,
                                 true, durationCount) &&
                       (durationCount == 1 || durationCount == valueCount);
            };
        char noteEffectRecord = 0;
        int noteEffectPlayer = -1;
        if (!(input >> noteEffectRecord >> noteEffectPlayer) ||
            noteEffectRecord != 'Y' || noteEffectPlayer != update.playerIndex) {
            error = "Python returned malformed note-effect time-var controls.";
            return false;
        }
        constexpr std::array<double, 12> noteEffectMinimum{
            0.0, -1.0, -1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, -1.0, -24.0};
        constexpr std::array<double, 12> noteEffectMaximum{
            10.0, 4.0, 4.0, 1.0, 4.0, 1.0, 32.0, 40.0, 1.0, 32.0, 7.0, 24.0};
        for (std::size_t index = 0;
             index < update.noteEffectControlTokens.size(); ++index) {
            auto& token = update.noteEffectControlTokens[index];
            if (!(input >> token)) {
                error = "Python returned an incomplete note-effect control list.";
                return false;
            }
            if (!validNoteEffectControlToken(
                    token, noteEffectMinimum[index], noteEffectMaximum[index])) {
                error = "Python returned invalid note-effect time-var control " +
                        std::to_string(index) + ": " + token;
                return false;
            }
        }
        if (update.synthIndex == 57) {
            char controlRecord = 0;
            int controlPlayer = -1;
            auto validControlToken = [](const std::string& token) {
                if (token == "-") return true;
                if (token.size() > 256 || token.size() < 5 ||
                    (token[0] != 'v' && token[0] != 'l') || token[1] != ':')
                    return false;
                const auto delimiter = token.find(':', 2);
                return delimiter != std::string::npos && delimiter > 2 &&
                       delimiter + 1 < token.size() &&
                       token.find(':', delimiter + 1) == std::string::npos;
            };
            if (!(input >> controlRecord >> controlPlayer >> update.sampleControlToken >>
                  update.sampleRateControlToken) ||
                controlRecord != 'Q' || controlPlayer != update.playerIndex ||
                !validControlToken(update.sampleControlToken) ||
                !validControlToken(update.sampleRateControlToken)) {
                error = "Python returned malformed sample time-var controls.";
                return false;
            }
        }
        seenPlayers[static_cast<std::size_t>(update.playerIndex)] = true;
        update.stopped = stopped != 0;
        update.mousePan = mousePan != 0;
        update.mouseCutoff = mouseCutoff != 0;
        update.events.reserve(eventCount);
        for (std::size_t i = 0; i < eventCount; ++i) {
            char noteRecord = 0;
            PythonNoteEvent event;
            if (!(input >> noteRecord >> event.beat >> event.durationBeats >>
                  event.midiNote >> event.velocity >> event.attackSeconds >>
                  event.releaseSeconds >> event.delayBeats >> event.vibDepth >>
                  event.slide >> event.slideFrom >> event.slideDelay >>
                  event.bend >> event.bendDelay >> event.chop >> event.spin >>
                  event.cut >> event.coarse >> event.formant >> event.pshift >>
                  event.eventEffectUserSynthIndex) ||
                noteRecord != 'N' ||
                !std::isfinite(event.beat) || !std::isfinite(event.durationBeats) ||
                !std::isfinite(event.delayBeats) || event.delayBeats < 0.0 || event.delayBeats > 15.0 ||
                !std::isfinite(event.vibDepth) || event.vibDepth < 0.0 ||
                event.vibDepth > 10.0 ||
                !std::isfinite(event.slide) || event.slide < -1.0 ||
                event.slide > 4.0 ||
                !std::isfinite(event.slideFrom) || event.slideFrom < -1.0 ||
                event.slideFrom > 4.0 ||
                !std::isfinite(event.slideDelay) || event.slideDelay < 0.0 ||
                event.slideDelay > 1.0 ||
                !std::isfinite(event.bend) || event.bend < -1.0 ||
                event.bend > 4.0 ||
                !std::isfinite(event.bendDelay) || event.bendDelay < 0.0 ||
                event.bendDelay > 1.0 ||
                !std::isfinite(event.chop) || event.chop < 0.0 ||
                event.chop > 32.0 ||
                !std::isfinite(event.spin) || event.spin < 0.0 ||
                event.spin > 40.0 ||
                !std::isfinite(event.cut) || event.cut < 0.0 ||
                event.cut > 1.0 ||
                !std::isfinite(event.coarse) || event.coarse < 0.0 ||
                event.coarse > 32.0 ||
                !std::isfinite(event.formant) || event.formant < -1.0 ||
                event.formant > 7.0 ||
                !std::isfinite(event.pshift) || event.pshift < -24.0 ||
                event.pshift > 24.0 ||
                event.eventEffectUserSynthIndex < -1 ||
                (event.eventEffectUserSynthIndex >= 0 &&
                 static_cast<std::size_t>(event.eventEffectUserSynthIndex) >=
                     userSynthCount) ||
                event.beat < 0.0 || event.beat > update.lengthBeats ||
                event.durationBeats < 0.02 ||
                event.durationBeats > std::min(4096.0, update.lengthBeats * 8.0) + 1e-9 ||
                event.midiNote < 0 || event.midiNote > 127 ||
                event.velocity < 1 || event.velocity > 127 ||
                !std::isfinite(event.attackSeconds) || event.attackSeconds < 0.0 ||
                event.attackSeconds > 3.0 || !std::isfinite(event.releaseSeconds) ||
                event.releaseSeconds < 0.0 || event.releaseSeconds > 3.0) {
                error = "Python returned a malformed note event.";
                return false;
            }
            for (auto& controlValue : event.eventEffectControlValues) {
                if (!(input >> controlValue) || !std::isfinite(controlValue) ||
                    controlValue < 0.0 || controlValue > 1.0) {
                    error = "Python returned invalid per-note effect controls.";
                    return false;
                }
            }
            if (update.synthIndex == 57) {
                char bufferRecord = 0;
                int gate = 0;
                int loop = 0;
                if (!(input >> bufferRecord >> event.sampleSymbol >> event.sampleIndex >>
                      event.sampleRate >> gate >> event.sampleStart >> event.sampleEnd >>
                      loop) || bufferRecord != 'B' ||
                    event.sampleSymbol < 33 || event.sampleSymbol > 126 ||
                    event.sampleIndex < -1000000 || event.sampleIndex > 1000000 ||
                    !std::isfinite(event.sampleRate) ||
                    std::abs(event.sampleRate) < 0.0625 || std::abs(event.sampleRate) > 8.0 ||
                    (gate != 0 && gate != 1) ||
                    !std::isfinite(event.sampleStart) ||
                    !std::isfinite(event.sampleEnd) ||
                    event.sampleStart < 0.0 || event.sampleEnd > 1.0 ||
                    event.sampleStart >= event.sampleEnd ||
                    (loop != 0 && loop != 1)) {
                    error = "Python returned a malformed sample event.";
                    return false;
                }
                event.sampleGate = gate != 0;
                event.sampleLoop = loop != 0;
                if (event.sampleLoop && !event.sampleGate) {
                    error = "Python returned a looping sample without a bounded gate.";
                    return false;
                }
            }
            update.events.push_back(event);
        }
        std::sort(update.events.begin(), update.events.end(),
                  [](const PythonNoteEvent& a, const PythonNoteEvent& b) {
                      return a.beat < b.beat;
                  });
        std::vector<std::pair<double, int>> boundaries;
        boundaries.reserve(update.events.size() * 4);
        int activeNotes = 0;
        for (const auto& event : update.events) {
            double startBeat = std::fmod(event.beat + event.delayBeats, update.lengthBeats);
            if (startBeat < 0.0) startBeat += update.lengthBeats;
            const double fullCycles = std::floor(event.durationBeats / update.lengthBeats);
            if (fullCycles > 8.0 || activeNotes + static_cast<int>(fullCycles) > 8) {
                std::ostringstream message;
                message << "Player " << (update.playerIndex + 1)
                        << " exceeds 8-note polyphony across overlapping sustains.";
                error = message.str();
                return false;
            }
            activeNotes += static_cast<int>(fullCycles);
            const double remainder =
                event.durationBeats - fullCycles * update.lengthBeats;
            if (remainder <= 1e-9)
                continue;
            const double endBeat = startBeat + remainder;
            if (endBeat <= update.lengthBeats) {
                boundaries.emplace_back(startBeat, 1);
                boundaries.emplace_back(endBeat, -1);
            } else {
                boundaries.emplace_back(0.0, 1);
                boundaries.emplace_back(endBeat - update.lengthBeats, -1);
                boundaries.emplace_back(startBeat, 1);
                boundaries.emplace_back(update.lengthBeats, -1);
            }
        }
        std::sort(boundaries.begin(), boundaries.end(),
                  [](const auto& a, const auto& b) {
                      if (a.first != b.first) return a.first < b.first;
                      return a.second < b.second;
                  });
        for (const auto& boundary : boundaries) {
            activeNotes += boundary.second;
            if (activeNotes > 8) {
                std::ostringstream message;
                message << "Player " << (update.playerIndex + 1)
                        << " exceeds 8-note polyphony near beat " << boundary.first << '.';
                error = message.str();
                return false;
            }
        }
        pattern.players.push_back(std::move(update));
    }
    std::array<bool, 8> seenMutes{};
    for (std::size_t i = 0; i < muteCount; ++i) {
        char record = 0;
        int playerIndex = -1;
        int muted = -1;
        if (!(input >> record >> playerIndex >> muted) || record != 'M' ||
            playerIndex < 0 || playerIndex >= 8 || muted < 0 || muted > 1 ||
            seenMutes[static_cast<std::size_t>(playerIndex)]) {
            error = "Python returned an invalid player mute control (record, player index, or mute state).";
            return false;
        }
        seenMutes[static_cast<std::size_t>(playerIndex)] = true;
        pattern.mutes.push_back(PythonPlayerMute{playerIndex, muted != 0});
    }
    for (std::size_t i = 0; i < diskCount; ++i) {
        PythonDiskCommand command;
        char record = 0;
        int loop = 0;
        std::string pathHex;
        if (!(input >> record >> command.mode >> command.slot >> command.rate >>
              command.amp >> command.pan >> command.seconds >> loop >> pathHex) ||
            record != 'D' || command.mode < 0 || command.mode > 4 ||
            command.slot < 0 || command.slot > 7 || (loop != 0 && loop != 1) ||
            !std::isfinite(command.rate) || command.rate < 0.0625 || command.rate > 8 ||
            !std::isfinite(command.amp) || command.amp < 0 || command.amp > 2 ||
            !std::isfinite(command.pan) || command.pan < -1 || command.pan > 1 ||
            !std::isfinite(command.seconds) ||
            (command.mode == 3 && (command.seconds < .05 || command.seconds > 3600)) ||
            (command.mode != 3 && command.seconds != 0)) {
            error = "Python returned an invalid DiskIO command."; return false;
        }
        command.loop = loop != 0;
        if (command.mode == 1 || command.mode == 2) {
            if (pathHex.empty() || pathHex.size() > 4096 || pathHex.size() % 2) {
                error = "Python returned an invalid disk input path."; return false;
            }
            auto digit = [](char c) {
                return c >= '0' && c <= '9' ? c - '0' :
                       c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            };
            for (std::size_t pos = 0; pos < pathHex.size(); pos += 2) {
                const auto high = digit(pathHex[pos]), low = digit(pathHex[pos + 1]);
                if (high < 0 || low < 0 || high * 16 + low < 32) {
                    error = "Python returned invalid disk path encoding."; return false;
                }
                command.path.push_back(static_cast<char>(high * 16 + low));
            }
        } else if (pathHex != "-") {
            error = "Python returned a path for a non-reading disk command."; return false;
        }
        pattern.diskCommands.push_back(std::move(command));
    }
    return true;
}
} // namespace

PortablePython& PortablePython::instance() {
    static PortablePython runtime;
    return runtime;
}

bool PortablePython::initialize(const std::filesystem::path& runtimeDirectory,
                                std::string& error) {
    LifecycleTrace trace("Python initialize including lock", this);
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(gPythonMutex);
    if (initialized_)
        return true;

    const auto dllPath = runtimeDirectory / L"python312.dll";
    gPython.module = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!gPython.module) {
        error = "Could not load bundled Python 3.12 (python312.dll).";
        return false;
    }

    if (!loadFunction(gPython.module, "Py_SetPythonHome", gPython.setPythonHome) ||
        !loadFunction(gPython.module, "Py_InitializeEx", gPython.initializeEx) ||
        !loadFunction(gPython.module, "Py_IsInitialized", gPython.isInitialized) ||
        !loadFunction(gPython.module, "PyEval_SaveThread", gPython.evalSaveThread) ||
        !loadFunction(gPython.module, "PyRun_SimpleStringFlags", gPython.runSimpleStringFlags) ||
        !loadFunction(gPython.module, "PyGILState_Ensure", gPython.gilStateEnsure) ||
        !loadFunction(gPython.module, "PyGILState_Release", gPython.gilStateRelease)) {
        error = "The bundled Python runtime is missing required embedding functions.";
        return false;
    }

    static std::wstring pythonHome;
    pythonHome = runtimeDirectory.wstring();
    gPython.setPythonHome(pythonHome.c_str());
    gPython.initializeEx(0);
    if (!gPython.isInitialized()) {
        error = "Python failed to initialize from the bundled runtime.";
        return false;
    }

    // The interpreter must not retain the GIL on the thread that initialized
    // it; editor-triggered evaluations may come from a different host thread.
    gPython.evalSaveThread();
    initialized_ = true;
    return true;
#else
    (void)runtimeDirectory;
    error = "The portable embedded Python runtime is currently packaged for Windows.";
    return false;
#endif
}

bool PortablePython::evaluate(const std::filesystem::path& runtimeDirectory,
                             const std::string& namespaceName,
                             const std::string& source,
                              const std::vector<std::string>& userSynthNames,
                             PythonPattern& pattern,
                             std::string& error) {
    if (source.size() > 64 * 1024) {
        error = "Code is limited to 64 KB in this prototype.";
        return false;
    }
    if (!initialize(runtimeDirectory, error))
        return false;

#if defined(_WIN32)
    const auto number = gScriptNumber.fetch_add(1, std::memory_order_relaxed);
    const auto base = std::filesystem::temp_directory_path() /
                      (L"dexter-python-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                       std::to_wstring(number));
    const auto sourcePath = base.wstring() + L".py";
    const auto resultPath = base.wstring() + L".out";

    {
        std::ofstream sourceFile(sourcePath, std::ios::binary);
        if (!sourceFile) {
            error = "Could not create a temporary Python source file.";
            return false;
        }
        sourceFile.write(source.data(), static_cast<std::streamsize>(source.size()));
    }

    const auto sourceUtf8 = pathAsUtf8(sourcePath);
    const auto resultUtf8 = pathAsUtf8(resultPath);
    const auto script = makeEvaluationScript(sourceUtf8, resultUtf8, namespaceName,
                                             userSynthNames);
    int result = -1;
    {
        std::lock_guard<std::mutex> lock(gPythonMutex);
        const int gil = gPython.gilStateEnsure();
        result = gPython.runSimpleStringFlags(script.c_str(), nullptr);
        gPython.gilStateRelease(gil);
    }

    bool success = false;
    if (result == 0)
        success = parseResult(resultPath, pattern, userSynthNames.size(), error);
    else
        error = "Python could not evaluate the editor script.";

    std::error_code ignored;
    std::filesystem::remove(sourcePath, ignored);
    std::filesystem::remove(resultPath, ignored);
    return success;
#else
    (void)namespaceName;
    (void)source;
    (void)pattern;
    return false;
#endif
}

bool PortablePython::checkCompatibility(const std::filesystem::path& runtimeDirectory,
                                       const std::string& source,
                                       const std::vector<std::string>& userSynthNames,
                                       std::string& report,
                                       std::string& error) {
    if (source.size() > 64 * 1024) {
        error = "Code is limited to 64 KB in this prototype.";
        return false;
    }
    if (!initialize(runtimeDirectory, error))
        return false;

#if defined(_WIN32)
    const auto number = gScriptNumber.fetch_add(1, std::memory_order_relaxed);
    const auto base = std::filesystem::temp_directory_path() /
                      (L"dexter-check-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                       std::to_wstring(number));
    const auto sourcePath = base.wstring() + L".py";
    const auto reportPath = base.wstring() + L".txt";
    {
        std::ofstream sourceFile(sourcePath, std::ios::binary);
        if (!sourceFile) {
            error = "Could not create a temporary Python source file for compatibility checking.";
            return false;
        }
        sourceFile.write(source.data(), static_cast<std::streamsize>(source.size()));
    }

    const auto script = makeCompatibilityScript(pathAsUtf8(sourcePath),
                                                pathAsUtf8(reportPath),
                                                userSynthNames);
    int result = -1;
    {
        std::lock_guard<std::mutex> lock(gPythonMutex);
        const int gil = gPython.gilStateEnsure();
        result = gPython.runSimpleStringFlags(script.c_str(), nullptr);
        gPython.gilStateRelease(gil);
    }

    bool success = false;
    if (result == 0) {
        std::ifstream reportFile(reportPath, std::ios::binary);
        if (reportFile) {
            std::ostringstream output;
            output << reportFile.rdbuf();
            report = output.str();
            success = !report.empty();
        }
        if (!success)
            error = "The compatibility checker did not produce a report.";
    } else {
        error = "The embedded Python compatibility checker could not run.";
    }

    std::error_code ignored;
    std::filesystem::remove(sourcePath, ignored);
    std::filesystem::remove(reportPath, ignored);
    return success;
#else
    (void)source;
    (void)report;
    error = "The portable embedded Python runtime is currently packaged for Windows.";
    return false;
#endif
}
