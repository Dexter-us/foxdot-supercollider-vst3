import ast
import builtins
import dis
import math
import os
import types
import re
import struct
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: test-pattern-api.py python_runtime.cpp Portable-directory')

repo_root = os.path.abspath(os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[1])), '..', '..'))

def read_cpp_source(path):
    # Some existing C++ comments in the merged test branch contain legacy
    # single-byte characters. These checks only inspect source text, so keep
    # the test portable across Windows code pages without changing the source.
    with open(path, encoding='utf-8', errors='replace') as source_file:
        return source_file.read()


source = read_cpp_source(sys.argv[1])
processor_path = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), 'processor.cpp')
processor_source = read_cpp_source(processor_path)
runtime_header_path = os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[1])), 'python_runtime.h')
runtime_header_source = read_cpp_source(runtime_header_path)
sclang_runtime_path = os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[1])), 'sclang_runtime_win32.cpp')
sclang_runtime_source = read_cpp_source(sclang_runtime_path)
sardine_gate_header_path = os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[1])), 'sardine_pattern_gates.h')
sardine_gate_header_source = read_cpp_source(sardine_gate_header_path)
editor_path = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])),
                           'win32_code_editor.cpp')
editor_source = read_cpp_source(editor_path)
editor_host_path = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])),
                                'win32_code_editor.h')
editor_host_source = read_cpp_source(editor_host_path)
catalog_source_path = os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[1])), 'editor_catalog.h')
catalog_source = read_cpp_source(catalog_source_path)
cmake_path = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])),
                          'CMakeLists.txt')
cmake_source = open(cmake_path, encoding='utf-8').read()
version_template_path = os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[1])), 'plugin_version.h.in')
version_template = open(version_template_path, encoding='utf-8').read()
if ('VERSION 0.4.192' not in cmake_source or
        '@PROJECT_VERSION@' not in version_template or
        'DEXTER_PLUGIN_VERSION' not in processor_source or
        'DEXTER_PLUGIN_VERSION' not in editor_source):
    raise AssertionError('Plugin, panel, and generated module versions are not unified.')
for wow_fragment in (
        'bool smoothNoise = false;',
        'double noisePeriod = 0.0;',
        "token[0] != 'w'",
        "const bool smoothNoise = token[0] == 'w';",
        'result.smoothNoise = smoothNoise;',
        'if (control.smoothNoise)',
        '951.135664',
        'bnoise(position * 2.3)'):
    if wow_fragment not in processor_source:
        raise AssertionError('Native smooth-noise wow control is missing: ' +
                             wow_fragment)
for live_panel_fragment in (
        'L"Players"', 'L"Synths"', 'L"Effects"', 'L"Samples"',
        'refreshPlayersPanel()', 'renderLiveEffectsPanel()',
        'renderSamplePanel()', 'Open Samples Folder', 'panelVersion_',
        'setEditorPlayerMuted(', 'stopEditorPlayer(',
        'WM_CONTEXTMENU', 'TrackPopupMenu', 'kContextCatalogSynthBaseId',
        'kContextCatalogEffectBaseId', 'setEditorPlayerControl(',
        'setEditorPlayerEffectControl(', 'effectControlValues',
        'playerEffectControlSpecs(', 'expandedEffectChains_',
        'selectedEffectSlotIndex()', 'setPlayerEffectControl(',
        'commitPlayerEffectControlToSource(', 'playerControlRanges_',
        'effectControlRangeText(', 'effectControlUnit(',
        '"fx_frequency"', '"fx_drive"', '"fx_stages"', '"fx_bits"',
        'fx_frequency fx_q fx_stages fx_drive fx_bits',
        'SCLOrkPhazerFX', 'SCLOrkDecimatorFX',
        'SCLOrkCrossoverDistortionFX',
        'LimutGatedVerb', 'LimutShifter', 'LimutShimmer',
        'LimutCompressor', 'LimutVocoder',
        'TB_ENDTRACK', 'commitPlayerControlToSource(',
        'kSciPositionFromPointClose',
        'beginCatalogDrag(', 'finishCatalogDrag(', 'showCatalogAbout(',
        'makeInstrumentCatalog(', 'makeEffectCatalog(', 'applyCatalogEntry(',
        'renderCatalogEntries()', 'entry.category', 'entry.subcategory',
        'collapsedCatalogCategories_', 'catalogCategoryHeaderRows_',
        'collapsedCatalogSubcategories_', 'catalogSubcategoryHeaderRows_',
        'return "Waveforms";',
        'selectPlayerAtEditorCaret()', 'SCN_UPDATEUI',
        'catalogCategoryCollapsed(', 'catalogSubcategoryCollapsed(',
        'collapsedCatalogCategories_.emplace(key, true)',
        'collapsedCatalogSubcategories_.emplace(key, true)',
        'sampleCategoryForFilename(', 'Drums & Percussion',
        'portableSynthGrouping(',
        'onPanelListSelectionChanged()', 'L"[+] "', 'L"[-] "',
        'firstConfiguredEffectSlot(', 'panelItemForEffect(headerPlayer, firstSlot)',
        'instrumentCategory(', 'effectCategory(',
        '179-editor-catalog-about.html',
        'L"Bits"', 'L"Crush"', 'EditorPlayerControl::BitDepth',
        'EditorPlayerControl::CrushRate', 'editorPlayerControlAvailable(',
        'capturePanelScrollAnchor()', 'restorePanelScrollAnchor(',
        'LB_GETTOPINDEX', 'LB_SETTOPINDEX',
        'Active player effect status', 'activeEffectSummaryExpanded_',
        'L"PShift"', 'EditorPlayerControl::PShift', '"pshift";',
        'index == 4 ? "bits"', 'index == 5 ? "crush"',
        'if (state.coarseEnabled)', 'if (state.formantEnabled)',
        'L" Coarse"', 'L" Formant"',
        'if (state.pshiftEnabled)', 'if (state.bitcrusherEnabled)',
        'L" PShift"', 'L" Bits/Crush"'):
    if live_panel_fragment not in (
            editor_source + editor_host_source + catalog_source):
        raise AssertionError('Live engine panel is missing: ' + live_panel_fragment)
for catalog_grouping in (
        'return {"Drums & Percussion", "Kick"};',
        'return {"Drums & Percussion", "Snare"};',
        'return {"Drums & Percussion", "Hi-hat"};',
        'return {"Drums & Percussion", "Toms"};',
        'return {"Drums & Percussion", "Jazz Kit"};',
        'return {"Drums & Percussion", "Cymbals"};',
        'return {"Bass", "Acid bass"};',
        'return {"Bass", "Sub bass"};',
        'return {"Bass", "Synth bass"};'):
    if catalog_grouping not in catalog_source:
        raise AssertionError('Portable SynthDef catalog grouping is missing: ' +
                             catalog_grouping)
if ('Precompiled percussion' in catalog_source or
        'Precompiled bass' in catalog_source):
    raise AssertionError('Portable SynthDefs still use generic precompiled buckets.')
if ('beginCatalogDrag' not in editor_source or
        'finishCatalogDrag' not in editor_source or
        'About this item' not in editor_source):
    raise AssertionError('Catalog drag/right-click About actions are missing.')
for catalog_grouping in (
        'return {"Drums & Percussion", "Kick"};',
        'return {"Drums & Percussion", "Snare"};',
        'return {"Drums & Percussion", "Hi-hat"};',
        'return {"Drums & Percussion", "Toms"};',
        'return {"Drums & Percussion", "Jazz Kit"};',
        'return {"Drums & Percussion", "Cymbals"};',
        'return {"Bass", "Acid bass"};',
        'return {"Bass", "Sub bass"};',
        'return {"Bass", "Synth bass"};'):
    if catalog_grouping not in catalog_source:
        raise AssertionError('Portable SynthDef catalog grouping is missing: ' +
                             catalog_grouping)
if ('Precompiled percussion' in catalog_source or
        'Precompiled bass' in catalog_source):
    raise AssertionError('Portable SynthDefs still use generic precompiled buckets.')
if 'portableSynthDisplayLabel(name)' not in catalog_source:
    raise AssertionError('Portable SynthDef catalog display labels are missing.')
jazz_synth_names = (
    'JazzKitKick', 'JazzKitBrushKick', 'JazzKitBrushTap', 'JazzKitBrushSweep',
    'JazzKitClosedHat', 'JazzKitOpenHat', 'JazzKitRide',
    'JazzKitCrossStick', 'JazzKitRackTom', 'JazzKitFloorTom')
for jazz_synth_name in jazz_synth_names:
    jazz_binary_path = os.path.join(
        os.path.abspath(sys.argv[2]), 'SynthDefs',
        jazz_synth_name + '.scsyndef')
    jazz_controls_path = os.path.join(
        os.path.abspath(sys.argv[2]), 'SynthDefs',
        jazz_synth_name + '.controls')
    if not os.path.isfile(jazz_binary_path) or not os.path.isfile(
            jazz_controls_path):
        raise AssertionError('Jazz Kit SynthDef or control sidecar is missing: ' +
                             jazz_synth_name)
    jazz_binary = open(jazz_binary_path, 'rb').read()
    if not jazz_binary.startswith(b'SCgf') or jazz_synth_name.encode(
            'ascii') not in jazz_binary:
        raise AssertionError('Jazz Kit SynthDef is not a compiled named SC graph: ' +
                             jazz_synth_name)
    if b'dexterAutoFree' not in jazz_binary:
        raise AssertionError('Jazz Kit SynthDef lacks the host lifecycle control: ' +
                             jazz_synth_name)
    jazz_control_text = open(
        jazz_controls_path, encoding='utf-8').read()
    if len(jazz_control_text.encode('utf-8')) > 8192:
        raise AssertionError('Jazz Kit controls sidecar exceeds the loader limit: ' +
                             jazz_synth_name)
    for control_line in jazz_control_text.splitlines():
        control_line = control_line.split('#', 1)[0].strip()
        if not control_line:
            continue
        control_fields = control_line.split()
        if len(control_fields) not in (4, 5):
            raise AssertionError('Malformed Jazz Kit control sidecar line: ' +
                                 jazz_synth_name + ': ' + control_line)
        control_name = control_fields[0]
        control_min, control_max, control_default = map(
            float, control_fields[1:4])
        if not (control_min < control_max and
                control_min <= control_default <= control_max):
            raise AssertionError('Out-of-range Jazz Kit control: ' +
                                 jazz_synth_name + ': ' + control_name)
        if control_name.encode('ascii') not in jazz_binary:
            raise AssertionError('Jazz Kit control missing from SynthDef: ' +
                                 jazz_synth_name + ': ' + control_name)
catalog_help = os.path.join(os.path.abspath(sys.argv[2]),
                            '179-editor-catalog-about.html')
if not os.path.isfile(catalog_help):
    raise AssertionError('Bundled editor catalog About page is missing.')
bitcrusher_example = os.path.join(
    os.path.abspath(sys.argv[2]), '180-live-bitcrusher-sliders.py')
if not os.path.isfile(bitcrusher_example):
    raise AssertionError('Live Bits/Crush slider example is missing.')
for example_name in ('181-finite-pattern-generator-pack.py',
                     '182-live-pshift-slider.py',
                     '183-seeded-random-pattern-helpers.py',
                     '184-live-player-control-discovery.py',
                     '185-pattern-flatten-chords.py',
                     '186-airwindows-echojam-on-chords.py',
                      '187-effect-chain-control-sliders.py',
                       '188-renardo-clock-clear.py',
                        '189-renardo-seed-default.py',
                        '190-renardo-probability-fibonacci-patterns.py',
                         '191-pattern-comparison-operators.py',
                            '192-pattern-transform-recursive.py',
                             '193-true-copy-convert-data.py',
                               '194-pattern-metadata-helpers.py',
                                 '195-limut-pingpong-duck.py',
                                  '196-limut-disperser-fuzz.py',
                         '201-sclork-insert-effects.py',
                         '211-limut-wow-tg-patterns.py',
                         '212-unit-aware-time-patterns.py',
                          '213-triangle-click-check.py',
                          '214-jazz-brush-kit.py',
                      '215-sardine-span-until.py',
                      '216-sardine-chance-loaf-on.py'):
    if not os.path.isfile(os.path.join(os.path.abspath(sys.argv[2]),
                                       example_name)):
        raise AssertionError('Portable example is missing: ' + example_name)
for sardine_fragment in (
        'int untilCycles = 0;',
        'next.untilCycles = static_cast<std::uint16_t>(',
        'if (pattern.untilCycles > 0)',
        'lastCycle = std::min(',
        'update.swingSeconds >> update.untilCycles',
        '_until_cycles, _chance_probability',
        '_swing_seconds, _until_cycles, _chance_probability'):
    if sardine_fragment not in source + runtime_header_source + processor_source:
        raise AssertionError('Sardine finite-until scheduling is missing: ' +
                             sardine_fragment)
for snapshot_fragment in (
        'editorPlayerState_[player].store',
        'editorPlayerState_[player].load',
        'publishEditorPlayerState(player)',
        '1u << 24',
        '1u << 25',
        '1u << 26',
        '1u << 27',
        'pendingEditorControls_',
        'editorAmplitudeCurrent_',
        'clampEditorPlayerControl',
        'editorAmpOverride',
        'editorPanOverride',
        'editorCutoffOverride',
        'editorPlayerValues_[player][3]',
        'editorPlayerValues_[player][4]',
        'editorPlayerValues_[player][5]',
        'editorPlayerValues_[player][6]',
        'editorPshiftCurrent = smoothEditorPlayerControl(',
        'pattern.noteEffectControls[11] = {};',
        'live.mask & 16u',
        'live.mask & 32u'):
    if snapshot_fragment not in processor_source:
        raise AssertionError('Live player snapshot is missing: ' + snapshot_fragment)
for airwindows_layout_fragment in (
    'isTapeDelay ? 9u : isEchojam ? 7u :',
    'isLimutPingPong ? 5u : isLimutDuck ? 7u :',
    'isLimutDisperser ? 6u : isLimutFuzz ? 5u :',
    'isLimutShifter ? 5u : isLimutShimmer ? 7u :',
    'isLimutGatedVerb ? 9u :',
    'isSCLOrkDecimator ? 7u :',
    'isSCLOrkCrossover ? 6u : 9u;',
    'isSCLOrkPhazer',
    'isSCLOrkDecimator',
    'isSCLOrkCrossover',
    'std::array<int, 7>{{2, 3, 4, 5, 6, -1, -1}}',
    'std::array<int, 7>{{2, 3, 4, 5, -1, -1, -1}}',
    'std::array<float, 7>{{0.0f, 80.0f, 1.0f, 0.5f,',
    'std::array<float, 7>{{0.0f, 0.5f, 0.0f, 200.0f,',
    'userDefinition.outControl = 0;',
    'userDefinition.inputBusControl = 1;',
    'std::array<int, 7>{{2, 3, 4, 5, 6, 8, 7}}',
    'std::array<int, 7>{{2, 3, 4, -1, -1, 6, 5}}',
    'userDefinition.name == "LimutPingPong"',
    'userDefinition.name == "LimutDuck"',
    'userDefinition.name == "LimutDisperser"',
    'userDefinition.name == "LimutFuzz"',
    'userDefinition.name == "LimutShifter"',
    'userDefinition.name == "LimutShimmer"',
    'userDefinition.name == "LimutGatedVerb"',
    'userDefinition.name == "LimutCompressor"',
    'std::array<int, 7>{{2, 3, 4, 5, 6, 7, -1}}',
    'std::array<float, 7>{{0.0f, -60.0f, 0.0f, 0.0f,',
        'std::array<int, 7>{{2, 3, 4, 5, -1, -1, -1}}',
        'std::array<int, 7>{{2, 3, 4, -1, -1, -1, -1}}',
        'std::array<float, 7>{{0.0f, 30.0f, 0.1f,',
        'std::array<float, 7>{{1.0f, 18000.0f, 10.0f,',
        'std::array<float, 7>{{0.0f, 0.0f, 200.0f,',
        'std::array<float, 7>{{1.0f, 36.0f, 16000.0f,',
):
    if airwindows_layout_fragment not in processor_source:
        raise AssertionError(
            'Persistent Airwindows control ABI is missing or out of sync: ' +
            airwindows_layout_fragment
        )
for insert_fragment in (
        'name == "Chorus" || name == "Flanger" || name == "Phaser" || name == "Tremolo"',
        'std::array<float, 7>{{0.0f, 0.05f, 0.0f, 0.005f',
        'std::array<float, 7>{{1.0f, 5.0f, 1.0f, 0.04f',
        'std::array<float, 7>{{0.0f, 0.05f, 0.0f, 0.0005f',
        'std::array<float, 7>{{1.0f, 4.0f, 1.0f, 0.015f',
        '0.0f, 0.05f, 0.0f, 0.005f, 0.0f, 0.0f, 200.0f',
        '1.0f, 5.0f, 1.0f, 0.04f, 1.0f, 0.75f, 16000.0f',
        '0.0f, 0.05f, 0.0f, 0.0005f, 0.0f, 0.0f, 200.0f',
        '1.0f, 4.0f, 1.0f, 0.015f, 1.0f, 0.95f, 16000.0f',
        '0.0f, 0.05f, 0.0f, 150.0f, 0.0f, 0.0f, 200.0f',
        '1.0f, 8.0f, 1.0f, 1800.0f, 1.0f, 0.85f, 16000.0f',
        '0.0f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 200.0f',
        '1.0f, 20.0f, 1.0f, 1.0f, 1.0f, 1.0f, 16000.0f',
        'userDefinition.effectControlIndices ='):
    if insert_fragment not in re.sub(r'\s+', ' ', processor_source):
        raise AssertionError('Chorus/Flanger insert configuration is missing: ' +
                             insert_fragment)
if "'fx_rate', 1, 0.05, 5.0, 0.8" not in source or \
        "'fx_spread', 4, 0.0, 1.0, 0.7" not in source or \
        "'fx_tone', 6, 200.0, 16000.0, 12000.0" not in source or \
        "'fx_center', 3, 150.0, 1800.0, 650.0" not in source or \
        "'fx_shape', 3, 0.0, 1.0, 0.0" not in source or \
        "'fx_phase', 5, 0.0, 1.0, 0.0" not in source:
    raise AssertionError('Modulation insert controls are missing or out of sync.')
start_marker = 'static constexpr char api[] = R"PY(' + chr(10)
end_marker = ')PY";'
start = source.find(start_marker)
if start < 0:
    raise SystemExit('Could not find the embedded Python API in python_runtime.cpp')
start += len(start_marker)
end = source.find(end_marker, start)
if end < 0:
    raise SystemExit('Could not find the end of the embedded Python API')
api = source[start:end]
checker_start_marker = 'static constexpr char compatibilityChecker[] = R"PY(' + chr(10)
checker_start = source.find(checker_start_marker)
if checker_start < 0:
    raise SystemExit('Could not find the embedded FoxDot compatibility checker')
checker_start += len(checker_start_marker)
checker_end = source.find(')PY";', checker_start)
if checker_end < 0:
    raise SystemExit('Could not find the end of the compatibility checker')
compatibility_checker = source[checker_start:checker_end]

# Reconstruct and compile the exact streamed Python wrapper used by C++.
wrapper_start = source.find('std::string makeEvaluationScript(')
wrapper_end = source.find('return script.str();', wrapper_start)
if wrapper_start < 0 or wrapper_end < 0:
    raise SystemExit('Could not find makeEvaluationScript in python_runtime.cpp')
wrapper_cpp = source[wrapper_start:wrapper_end]

def make_wrapper(source_path, result_path, user_synth_names=()):
    parts = []
    placeholders = {'namespaceName': 'test_scope', 'api': api,
                    'sourcePath': source_path, 'resultPath': result_path,
                    'userSynthNames': tuple(user_synth_names)}
    for token in re.findall(r'<<\s*("(?:\\.|[^"\\])*"|pythonLiteral\([^)]*\)|pythonStringTuple\([^)]*\))', wrapper_cpp):
        if token.startswith('"'):
            parts.append(ast.literal_eval(token))
        elif token.startswith('pythonStringTuple('):
            parts.append(repr(placeholders['userSynthNames']))
        else:
            argument = token[len('pythonLiteral('):-1]
            if argument not in placeholders:
                raise SystemExit('Unknown generated-wrapper placeholder: ' + argument)
            parts.append(repr(placeholders[argument]))
    return ''.join(parts).replace(
        'int(time.monotonic_ns() & 0xffffffff)', '123456789')

safe_names = ('abs', 'all', 'any', 'bool', 'dict', 'divmod', 'enumerate', 'filter',
              'float', 'int', 'isinstance', 'len', 'list', 'map', 'max', 'min',
              'range', 'reversed', 'round', 'set', 'slice', 'sorted', 'str',
               'sum', 'tuple', 'zip', 'ValueError', 'TypeError', 'IndexError',
               'OverflowError',
               'AttributeError',
              'ZeroDivisionError', '__build_class__', 'ord', 'chr')
namespace = {
    '_dexter_events': [], '_dexter_bpm': 120.0, '_dexter_bpm_control': None,
    '_dexter_length': 1.0,
    '_dexter_slots': 0, '_dexter_scale': (0, 2, 4, 5, 7, 9, 11),
    '_dexter_root': 60, '_dexter_root_control': None,
    '_dexter_updates': {}, '_dexter_mutes': {},
    '_dexter_shuffle_seed': 123456789,
    '_dexter_dis': dis,
    '_dexter_math': math,
    '_dexter_function_type': types.FunctionType,
    '_dexter_builtin_module': builtins,
    '_dexter_pure_map_builtins': {
        name: getattr(builtins, name)
        for name in ('abs', 'bool', 'float', 'int', 'len', 'max', 'min',
                     'round', 'str')},
    '_dexter_default_touched': False, '__name__': 'dexter_test',
    '__builtins__': {name: getattr(builtins, name) for name in safe_names},
}
exec(api, namespace, namespace)
runtime_effect_controls = {
    name: {spec[0] for spec in specs}
    for name, specs in namespace['_PLAYER_EFFECT_SPECS'].items()
}
compatibility_tree = ast.parse(compatibility_checker)
compatibility_effect_controls = next(
    ast.literal_eval(statement.value)
    for statement in compatibility_tree.body
    if isinstance(statement, ast.Assign) and any(
        isinstance(target, ast.Name) and
        target.id == '_COMPAT_PLAYER_EFFECT_CONTROLS'
        for target in statement.targets))
assert runtime_effect_controls == compatibility_effect_controls, (
    'Static compatibility-check effect controls must match runtime controls.')
assert namespace['reverse']([1, 2, 3]) == [3, 2, 1]
assert namespace['rotate']([1, 2, 3], 1) == [3, 1, 2]
assert namespace['rotate']([1, 2, 3], -1) == [2, 3, 1]
assert namespace['repeat']([1, None], 3) == [1, None, 1, None, 1, None]
assert namespace['palindrome']([1, 2, 3, 4]) == [1, 2, 3, 4, 3, 2]
assert namespace['palindrome']([1, None, [3, 5]]) == [1, None, [3, 5], None]
assert namespace['stutter']([1, None, [3, 5]], 2) == [1, 1, None, None, [3, 5], [3, 5]]
assert namespace['interleave']([1, 2, 3], [None, 9], [4]) == [1, None, 4, 2, 9, 3]
assert namespace['transpose']([60, None, [64, 67]], 12) == [72, None, [76, 79]]
euclidean = namespace['euclid'](3, 8, 36, 1)
assert len(euclidean) == 8 and euclidean.count(36) == 3
randChoices = namespace['PRand'](namespace['P'][0, 2, 4, 7],
                                 length=64, seed=12345)
assert len(randChoices) == 64 and all(value in (0, 2, 4, 7) for value in randChoices)
assert randChoices == namespace['PRand'](
    namespace['P'][0, 2, 4, 7], length=64, seed=12345)
assert namespace['PRand'](5, length=64, seed=8) == namespace['PRand'](
    0, 5, length=64, seed=8)
integerRange = namespace['PRand'](-3, 4, length=64, seed=9)
assert all(isinstance(value, int) and -3 <= value <= 4 for value in integerRange)
randomSamples = namespace['PRand'](
    namespace['P']['x', 'o', '-', ' '], length=64, seed=99)
assert all(value in ('x', 'o', '-', ' ') for value in randomSamples)
white = namespace['PWhite'](-1.0, 1.0, length=256, seed=123)
assert len(white) == 256 and all(-1.0 <= value <= 1.0 for value in white)
assert white == namespace['PWhite'](-1.0, 1.0, length=256, seed=123)
assert list(white) != list(namespace['PWhite'](
    -1.0, 1.0, length=256, seed=124))
assert abs(sum(white) / len(white)) < 0.15
assert namespace['PWhite'](0.5, 0.5, length=8, seed=2) == [0.5] * 8
walk = namespace['PWalk'](max=7, step=namespace['P'][1, 2, 1],
                          start=0, length=128, seed=123)
assert len(walk) == 128 and walk[0] == 0
assert walk == namespace['PWalk'](
    max=7, step=namespace['P'][1, 2, 1], start=0, length=128, seed=123)
assert all(-9 <= value <= 9 for value in walk)
boundedWalk = namespace['PWalk'](max=3, step=1, start=0, length=256, seed=9)
assert all(-3 <= value <= 3 for value in boundedWalk)
assert namespace['PWalk'](max=-2, step=-1, start=1, length=4, seed=3)[0] == 1
namespace['Seed'].default = 5
seededDefaultRandA = namespace['PRand'](0, 100, length=32)
seededDefaultRandB = namespace['PRand'](0, 100, length=32)
assert seededDefaultRandA == seededDefaultRandB
assert seededDefaultRandA == namespace['PRand'](
    0, 100, length=32, seed=5)
seededDefaultWhiteA = namespace['PWhite'](-1, 1, length=32)
seededDefaultWhiteB = namespace['PWhite'](-1, 1, length=32)
assert seededDefaultWhiteA == seededDefaultWhiteB
assert seededDefaultWhiteA == namespace['PWhite'](
    -1, 1, length=32, seed=5)
assert list(seededDefaultRandA) != list(namespace['PRand'](
    0, 100, length=32, seed=99))
namespace['Seed'].default = None
assert namespace['Seed'].default is None
unseededDefaultA = namespace['PRand'](0, 100, length=32)
unseededDefaultB = namespace['PRand'](0, 100, length=32)
assert list(unseededDefaultA) != list(unseededDefaultB)
for invalid_seed in (True, 1.5, '5', -1, 0x100000000):
    try:
        namespace['Seed'].default = invalid_seed
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('Seed.default accepted invalid value: ' +
                             repr(invalid_seed))
namespace['Seed'].default = None
chainMap = {0: [1, 2], 1: [0], 2: [2, 9]}
chain = namespace['PChain'](chainMap, length=128, seed=123)
assert len(chain) == 128 and chain[0] == 0
assert chain == namespace['PChain'](chainMap, length=128, seed=123)
for previous, current in zip(chain, chain[1:]):
    if previous in chainMap:
        assert current in chainMap[previous]
    else:
        assert current == previous
terminalChain = namespace['PChain']({0: [9]}, length=8, seed=6)
assert terminalChain == [0, 9, 9, 9, 9, 9, 9, 9]
sampleChain = namespace['PChain'](
    {'x': ['o', '-'], 'o': ['x'], '-': ['x']},
    length=32, seed=22)
assert len(sampleChain) == 32 and sampleChain[0] == 'x'
assert all(value in ('x', 'o', '-') for value in sampleChain)
assert namespace['PChain']({5: [5]}, length=6) == [5] * 6
assert namespace['PDelta'](
    namespace['P'][1, -2, 0.5], start=60, length=7) == [
        60, 61, 59, 59.5, 60.5, 58.5, 59]
assert namespace['PDelta'](2, start=-1, length=4) == [-1, 1, 3, 5]
assert namespace['PDelta'](
    namespace['P'][1, 2], start=0, length=6) == [0, 1, 3, 4, 6, 7]
assert namespace['PSquare']()[:10] == [0, 1, 4, 9, 16, 25, 36, 49, 64, 81]
assert len(namespace['PSquare']()) == 256
assert namespace['PSquare'](length=1) == [0]
assert namespace['PSq']() == [1, 4, 9]
assert namespace['PSq'](-3, 2, 4) == [9, 4, 1, 0]
assert namespace['PRange'](5) == [0, 1, 2, 3, 4]
assert namespace['PRange'](1, 6, 2) == [1, 3, 5]
assert namespace['PRange'](5, 0) == [5, 4, 3, 2, 1]
assert namespace['PRange'](3, 3) == [3]
assert namespace['ramp'](1, 10, 2) == [1, 3, 5, 7, 9]
assert namespace['ramp'](5, 0) == [5, 4, 3, 2, 1, 0]
floatRamp = namespace['ramp'](0.0, 1.0, 0.3)
assert len(floatRamp) == 4 and all(
    abs(value - expected) < 1e-12
    for value, expected in zip(floatRamp, [0.0, 0.3, 0.6, 0.9]))
assert namespace['ramp'](1.0, 0.0, 0.25) == [
    1.0, 0.75, 0.5, 0.25, 0.0]
assert namespace['ramp'](4, 4, 0.5) == [4]
assert namespace['ramp'](0, 255, 1)[-1] == 255
assert namespace['PStep'](4, 1) == [0, 0, 0, 1]
assert namespace['PStep'](4, 'x', '-') == ['-', '-', '-', 'x']
assert namespace['PEuclid'](3, 8) == [1, 0, 0, 1, 0, 0, 1, 0]
assert namespace['PEuclid2'](3, 8, ' ', 'x') == [
    'x', ' ', ' ', 'x', ' ', ' ', 'x', ' ']
binary10 = namespace['P10'](32, seed=123)
assert len(binary10) == 32 and set(binary10) <= {0, 1}
assert binary10 == namespace['P10'](32, seed=123)
assert len(namespace['P10'](256, seed=1)) == 256
assert namespace['PBern'](32, 0.0, seed=5) == [0] * 32
assert namespace['PBern'](32, 1.0, seed=5) == [1] * 32
assert namespace['PBern'](64, 0.4, seed=99) == namespace['PBern'](
    64, 0.4, seed=99)
sinePattern = namespace['PSine']()
assert len(sinePattern) == 16 and abs(sinePattern[0]) < 1e-12
assert abs(sinePattern[4] - 1.0) < 1e-12
assert abs(sinePattern[8]) < 1e-12
assert abs(sinePattern[12] + 1.0) < 1e-12
assert namespace['PSine'](256)[0] == 0.0
assert namespace['PTri'](4) == [0, 1, 2, 3, 2, 1]
assert namespace['PTri'](5, 0, 2) == [5, 3, 1, 3]
assert namespace['PTri'](0, 1) == [0]
psummed = namespace['PSum'](5, 4)
assert psummed == [1.0, 0.75, 0.75, 0.75, 0.75]
assert abs(sum(namespace['PSum'](3, 8)) - 8.0) < 1e-9
assert len(namespace['PSum'](256, 32)) == 256
assert namespace['PSum'](4, 0) == [0.0, 0.0, 0.0, 0.0]
assert abs(sum(namespace['PSum'](17, 13.25, lim=0.015625)) -
           13.25) < 0.02
assert namespace['PIndex'](8) == list(range(8))
assert namespace['PIndex'](1) == [0]
assert len(namespace['PIndex']()) == 256
assert namespace['PZ12']([1, 0], [1, 0.5], length=16) == [
    1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1]
assert namespace['PZ12']([3, 7], [1, 1], length=8) == [
    3, 7, 3, 7, 3, 7, 3, 7]
assert namespace['PZ12']([3, 7], [0, 1], length=4) == [7, 7, 7, 7]
assert namespace['PZ12']([0, 1], [1, 0.5], length=256) == namespace[
    'PZ12']([0, 1], [1, 0.5], length=256)
assert len(namespace['PZ12']()) == 256
fibonacci = namespace['PFibMod'](16)
assert fibonacci == [0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610]
assert fibonacci % 12 == [0, 1, 1, 2, 3, 5, 8, 1, 9, 10, 7, 5, 0, 5, 5, 10]
assert namespace['PFibMod'](1) == [0]
assert namespace['PFibMod'](31)[30] == 832040
for invalid_generator in (
        lambda: namespace['PZ12']([0]),
        lambda: namespace['PZ12']([0, 1, 2]),
        lambda: namespace['PZ12']([0, 0]),
        lambda: namespace['PZ12'](['x', 'o']),
        lambda: namespace['PZ12']([0, 1], [1]),
        lambda: namespace['PZ12']([0, 1], [-1, 0]),
        lambda: namespace['PZ12']([0, 1], [0, 0]),
        lambda: namespace['PZ12']([0, 1], [1, float('inf')]),
        lambda: namespace['PZ12']([0, 1], [1, 0.5], length=0),
        lambda: namespace['PZ12']([0, 1], [1, 0.5], length=257),
        lambda: namespace['PFibMod'](0),
        lambda: namespace['PFibMod'](32)):
    try:
        invalid_generator()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('bounded Renardo generator accepted invalid input')
pxrand = namespace['PxRand'](
    namespace['P'][0, 2, 4, 7], length=128, seed=37)
assert len(pxrand) == 128 and set(pxrand) <= {0, 2, 4, 7}
assert pxrand == namespace['PxRand'](
    namespace['P'][0, 2, 4, 7], length=128, seed=37)
assert all(left != right for left, right in zip(pxrand, pxrand[1:]))
assert all(left != right for left, right in zip(
    namespace['PxRand'](-4, 4, length=256, seed=31),
    namespace['PxRand'](-4, 4, length=256, seed=31)[1:]))
samplePxRand = namespace['PxRand']('xo-', length=64, seed=9)
assert set(samplePxRand) <= {'x', 'o', '-'}
assert all(left != right for left, right in
           zip(samplePxRand, samplePxRand[1:]))
assert namespace['PxRand'](namespace['P'][5], length=1, seed=2) == [5]
assert namespace['PwRand'](
    namespace['P'][0, 2, 4], namespace['P'][0, 1], length=16,
    seed=12) == [2] * 16
weighted = namespace['PwRand'](
    namespace['P'][0, 7], namespace['P'][1, 3], length=256, seed=42)
assert len(weighted) == 256 and set(weighted) == {0, 7}
assert weighted == namespace['PwRand'](
    namespace['P'][0, 7], namespace['P'][1, 3], length=256, seed=42)
assert namespace['PwRand'](
    namespace['P']['x', 'o'], namespace['P'][0, 1], length=32,
    seed=6) == ['o'] * 32
assert len(namespace['PwRand'](
    namespace['_FoxDotFinitePattern'](list(range(256))), 256,
    length=256, seed=1)) == 256
assert namespace['P'][0, 2, 2, 4].undup() == [0, 2, 4]
assert namespace['P'][0, 2, 4].norm() == [0.0, 0.5, 1.0]
pvarPitched = namespace['Pvar'](
    [namespace['P'][0, 2], namespace['P'][4, 6]], [1, 1])
assert pvarPitched.expand(0.5) == [0, 2, 4, 6]
pvarVariableDur = namespace['Pvar'](
    [namespace['P'][0, 2, 4, 6], namespace['P'][7, 5, 3, 1]], [3, 3])
assert pvarVariableDur.expand(namespace['P'][0.5, 1.0]) == [
    0, 2, 4, 6, 7, 5, 3, 1]
pvarVarDur = namespace['Pvar'](
    [namespace['P'][0, 2, 4], namespace['P'][7, 5, 3]], [2, 2])
assert pvarVarDur.expand(namespace['var']([0.5, 1.0], [1, 1])) == [
    0, 2, 4, 7, 5, 3]
pvarLinvarDur = namespace['Pvar'](
    [namespace['P'][0, 2], namespace['P'][7, 5]], [1, 1])
assert pvarLinvarDur.expand(
    namespace['linvar']([0.25, 1.25], [0.5, 0.5])) == [0, 2, 7, 5]
pvarGroupedDur = namespace['Pvar'](
    [namespace['P'][0, 2], namespace['P'][4, 6]], [3, 3])
pvarGroupedRhythm = namespace['PRhythm']([1, (3, 8)])
assert pvarGroupedDur.expand(pvarGroupedRhythm) == [0, 2, 4, 6]
pvarGroupedSampleDur = namespace['Pvar'](['x-', 'o-'], [3, 3])
assert pvarGroupedSampleDur.expand(pvarGroupedRhythm) == list('x-o-')
pvarBroadcast = namespace['Pvar'](
    [namespace['P'][0, 1], namespace['P'][2]], 1)
assert pvarBroadcast.expand(0.5) == [0, 1, 2, 2]
assert namespace['Pvar'](
    patterns=[namespace['P'][0], namespace['P'][2]], dur=0.5
).expand(0.5) == [0, 2]
pvarSamples = namespace['Pvar'](
    ['x-o-', 'o--x'], [2, 2])
assert pvarSamples.kind == 'sample'
assert pvarSamples.expand(0.5) == list('x-o-o--x')
for pvarFactory in (
        lambda: namespace['Pvar']([], 1),
        lambda: namespace['Pvar']([namespace['P'][0]], []),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P'][1]], [1, 2, 3]),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P']['x']], 1),
        lambda: namespace['Pvar']([namespace['P'][0]], float('inf')),
        lambda: namespace['Pvar']([namespace['P'][0]] * 2, [4096, 4096]),
        lambda: namespace['Pvar']([namespace['P'][0]] * 9, 1),
        lambda: namespace['Pvar']([[0] * 257], 1),
        lambda: pvarPitched.expand(0.3),
        lambda: pvarPitched.expand(namespace['P'][0.5, 1.0]),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P'][1]], [1, 1]
        ).expand(namespace['P'][0.5, 1.0]),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P'][1]], [3, 2]
        ).expand(namespace['P'][0.5, 1.0]),
        lambda: namespace['Pvar'](
            [namespace['P'][0], namespace['P'][1]], [2, 2]
        ).expand(namespace['var']([0.5, 1.0], [1.0, 1.5])),
        lambda: pvarGroupedDur.expand(namespace['P'][
            namespace['_FoxDotDurationGroup']([1.0, 9.0])]),
        lambda: pvarGroupedDur.expand(namespace['P'][
            namespace['_FoxDotDurationGroup']([0.5, 0.75])]),
        lambda: pvarVarDur.expand(
            namespace['var']([0.05, 1.0], [1.0, 1.0])),
        lambda: namespace['Pvar'](
            [namespace['P'][0]] * 8, 1).expand(0.001)):
    try:
        pvarFactory()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('Pvar accepted an invalid phrase, duration, or expansion')
for generator in (
        lambda: namespace['PRand']([], length=2),
        lambda: namespace['PRand'](2.5, length=2),
        lambda: namespace['PRand'](4, 2, length=2),
        lambda: namespace['PRand'](1, length=0),
        lambda: namespace['PRand'](1, length=257),
        lambda: namespace['PRand'](1, seed=True),
        lambda: namespace['PWhite'](2.0, 1.0),
        lambda: namespace['PWhite'](0.0, float('inf')),
        lambda: namespace['PWhite'](0.0, 1.0, length=257),
        lambda: namespace['PWalk'](max=True),
        lambda: namespace['PWalk'](
            step=namespace['_FoxDotFinitePattern']([]), length=3),
        lambda: namespace['PWalk'](step=namespace['P'][1, None], length=3),
        lambda: namespace['PWalk'](step=namespace['P']['x', 'o'], length=3),
        lambda: namespace['PWalk'](step=float('inf')),
        lambda: namespace['PWalk'](length=257),
        lambda: namespace['PWalk'](seed=True),
        lambda: namespace['PChain']([]),
        lambda: namespace['PChain']({}),
        lambda: namespace['PChain']({True: [0]}),
        lambda: namespace['PChain']({0: []}),
        lambda: namespace['PChain']({0: ['x']}),
        lambda: namespace['PChain']({0: [object()]}),
        lambda: namespace['PChain']({0: [0]}, length=257),
        lambda: namespace['PChain']({0: [0]}, seed=True),
        lambda: namespace['PChain']({i: [i] for i in range(33)}),
        lambda: namespace['PChain'](
            {0: list(range(257))}),
        lambda: namespace['PDelta'](
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['PDelta'](namespace['P']['x', 'o']),
        lambda: namespace['PDelta'](namespace['P'][1, None]),
        lambda: namespace['PDelta'](1, start=float('inf')),
        lambda: namespace['PDelta'](1, length=257),
        lambda: namespace['PDelta'](1000000, start=1, length=2),
        lambda: namespace['PSquare'](length=0),
        lambda: namespace['PSquare'](length=257),
        lambda: namespace['PSquare'](length=1.5),
        lambda: namespace['PIndex'](length=0),
        lambda: namespace['PIndex'](length=257),
        lambda: namespace['PIndex'](True),
        lambda: namespace['PxRand']([], length=2),
        lambda: namespace['PxRand'](namespace['P'][1, 1], length=2),
        lambda: namespace['PxRand'](3, 3, length=2),
        lambda: namespace['PxRand'](1, length=257),
        lambda: namespace['PxRand'](namespace['P'][0, 1], seed=True),
        lambda: namespace['PxRand'](namespace['P'][0, 1], stop=2),
        lambda: namespace['PwRand']([], [1]),
        lambda: namespace['PwRand'](namespace['P'][0, 2], []),
        lambda: namespace['PwRand'](namespace['P'][0, 2], [0, 0]),
        lambda: namespace['PwRand'](namespace['P'][0, 2], [1, 1.5]),
        lambda: namespace['PwRand'](namespace['P'][0, 2], [-1, 1]),
        lambda: namespace['PwRand'](namespace['P'][0, 2], [257, 1]),
        lambda: namespace['PwRand'](namespace['P'][0, 1], 1, length=257),
        lambda: namespace['PwRand'](namespace['P'][0, 1], 1, seed=True),
        lambda: namespace['PSq'](c=0),
        lambda: namespace['PSq'](c=257),
        lambda: namespace['PSq'](b=float('inf')),
        lambda: namespace['PSq'](b=32),
        lambda: namespace['PSq'](-2, 0.5, 2),
        lambda: namespace['PRange'](0, 257),
        lambda: namespace['PRange'](0, 8, 0),
        lambda: namespace['PRange'](True, 4),
        lambda: namespace['PRange'](0, 1000000),
        lambda: namespace['ramp'](0, 257, 1),
        lambda: namespace['ramp'](0, 1, 0),
        lambda: namespace['ramp'](False, 1),
        lambda: namespace['ramp'](0, float('inf'), 0.5),
        lambda: namespace['ramp'](0, 1, 1e-300),
        lambda: namespace['ramp'](0, 1000001, 1),
        lambda: namespace['PStep'](0, 1),
        lambda: namespace['PStep'](257, 1),
        lambda: namespace['PStep'](4, 'x'),
        lambda: namespace['PEuclid'](9, 8),
        lambda: namespace['PEuclid'](1, 257),
        lambda: namespace['PEuclid'](1, 8.0),
        lambda: namespace['PEuclid2'](2, 5, 0, 'x'),
        lambda: namespace['P10'](0),
        lambda: namespace['P10'](257),
        lambda: namespace['P10'](True),
        lambda: namespace['P10'](4, seed=True),
        lambda: namespace['PBern'](257),
        lambda: namespace['PBern'](4, 1.1),
        lambda: namespace['PBern'](4, float('nan')),
        lambda: namespace['PSine'](0),
        lambda: namespace['PSine'](257),
        lambda: namespace['PTri'](0, 130),
        lambda: namespace['PTri'](0, 4, 0),
        lambda: namespace['PSum'](0, 1),
        lambda: namespace['PSum'](4, -1),
        lambda: namespace['PSum'](4, float('inf')),
        lambda: namespace['PSum'](4, 1, lim=0),
        lambda: namespace['PSum'](4, 1, lim=1.1)):
    try:
        generator()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('bounded pattern generator should reject invalid bounds')
stepVar = namespace['var']([0.2, 0.8], [2, 2])
linearVar = namespace['linvar']([-1, 1], 4)
assert stepVar.wire() == 'v:0.2,0.8:2.0,2.0'
assert linearVar.wire() == 'l:-1.0,1.0:4.0,4.0'
defaultVar = namespace['var']([1, 0])
assert defaultVar.durations == [4.0, 4.0]
assert stepVar.at(0) == 0.2 and stepVar.at(2) == 0.8
assert abs(linearVar.at(2)) < 1e-9 and linearVar.at(4) == 1.0
fox_literal = namespace['P'][0, 2, None, (3, 5), 7]
assert fox_literal == [0, 2, None, (3, 5), 7]
assert fox_literal + 2 == [2, 4, None, (5, 7), 9]
assert 2 + namespace['P'][1, None, (3, 5)] == [3, None, (5, 7)]
assert namespace['P'][4, 8] - namespace['P'][1, 2, 3] == [3, 6, 1, 7, 2, 5]
assert 10 - namespace['P'][1, None, (2, 4)] == [9, None, (8, 6)]
pattern = namespace['P'][1, None, (3, 5), 4]
assert pattern.reverse() == [4, (3, 5), None, 1]
assert pattern.rotate(1) == [None, (3, 5), 4, 1]
assert pattern.rotate(-1) == [4, 1, None, (3, 5)]
assert namespace['P'][0, 1, 2, 3].stutter([1, 3]) == [0, 1, 1, 1, 2, 3, 3, 3]
assert namespace['P'][1, None, 3].repeat(2) == [1, None, 3, 1, None, 3]
nestedRepeatSource = namespace['P'][
    0, namespace['PGroup'](1, 3),
    namespace['P'][4, namespace['PGroup'](5, 7)]]
nestedDuplicate = nestedRepeatSource.duplicate(2)
nestedIter = nestedRepeatSource.iter(2)
assert nestedDuplicate == list(nestedRepeatSource) * 2
assert nestedIter == list(nestedRepeatSource) * 2
assert isinstance(nestedDuplicate[1], namespace['_FoxDotChord'])
assert isinstance(nestedDuplicate[2], namespace['_FoxDotFinitePattern'])
assert isinstance(nestedIter[1], namespace['_FoxDotChord'])
assert isinstance(nestedIter[2], namespace['_FoxDotFinitePattern'])
assert namespace['P'][1, 2].duplicate(0) == []
assert namespace['P'][1, 2].iter(0) == []
assert namespace['P'][1, 2].duplicate(2) == namespace['P'][1, 2].repeat(2)
assert namespace['P'][1, 2].iter(2) == namespace['P'][1, 2].repeat(2)
for bounded_repeat in (
        lambda: namespace['P'][1].duplicate(-1),
        lambda: namespace['P'][1].iter(257),
        lambda: namespace['P'][1, 2].duplicate(1.5),
        lambda: namespace['P'][*range(129)].iter(2)):
    try:
        bounded_repeat()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('duplicate/iter must enforce integer and expansion bounds')
assert namespace['P'][0, 1, 2].stretch(5) == [0, 1, 2, 0, 1]
assert namespace['P']['x', 'o'].stretch(namespace['P'][2, 3]) == [
    'x', 'o', 'x', 'o', 'x']
assert namespace['P'][1, 2].loop(2) == [1, 2, 1, 2]
assert namespace['P'][1, 2].loop(namespace['P'][1, 2]) == [
    1, 2, 1, 2, 1, 2]
assert namespace['P'][1, 2].loop(0) == []
assert namespace['P'][1, 2].stretch(0) == []
shuffleCounterBeforeMethodTests = namespace['_dexter_shuffle_counter']
randomCounterBeforeSelectorTests = namespace['_dexter_random_counter']
assert namespace['P'][0, 2, 4, 7].invert() == [7, 5, 3, 0]
assert namespace['P'][0, None, 4].invert() == [4, None, 0]
assert namespace['P'][-4, -2, 0].invert() == [4, 2, 0]
assert namespace['P'][0, 0].invert() == [0, 0]
invertedChords = namespace['P'][
    0, namespace['PGroup'](0, 4, 7), 7].invert()
assert invertedChords == [7, (7, 3, 0), 0]
assert isinstance(invertedChords[1], namespace['_FoxDotChord'])
assert namespace['_FoxDotFinitePattern']([]).invert() == []
assert namespace['P'][0, 1, 2, 3].arp(namespace['P'][0, 2]) == [
    0, 2, 1, 3, 2, 4, 3, 5]
arpeggiatedChords = namespace['P'][
    namespace['PGroup'](0, 4, 7), 2].arp(namespace['P'][0, 2])
assert arpeggiatedChords == [(0, 4, 7), (2, 6, 9), 2, 4]
assert namespace['P'][0, None].arp(namespace['P'][0, 2]) == [
    0, 2, None, None]
assert namespace['P'][0, 1, 2, 3].splice(
    namespace['P'][4, 5, 6, 7], namespace['P'][8, 9]) == [
        0, 4, 8, 1, 5, 9, 2, 6, 8, 3, 7, 9]
assert namespace['P']['x', '-', 'o'].splice('  ', 'x-') == [
    'x', ' ', 'x', '-', ' ', '-', 'o', ' ', 'x',
    'x', ' ', '-', '-', ' ', 'x', 'o', ' ', '-']
assert namespace['P'][0, namespace['PGroup'](0, 3, 7)].splice(
    namespace['P'][4, 5]) == [
        0, 4, (0, 3, 7), 5]
assert namespace['P'][3, 1, 2, 1].sort() == [1, 1, 2, 3]
assert namespace['P'][-2, 4, 0].sort() == [-2, 0, 4]
assert namespace['P']['x', 'o', '-', 'x'].sort() == ['-', 'o', 'x', 'x']
assert namespace['_FoxDotFinitePattern']([]).sort() == []
assert namespace['P'][1, 1, 2, 1, 1, 3, 3].unduplicate() == [
    1, 2, 1, 3]
assert namespace['P'][0, 2, 4, 6, 8, 10].compress(
    namespace['P'][1, 0, 1]) == [0, 4, 6, 10]
assert namespace['P'][0, 2, 4, 6, 8, 10].compress(
    namespace['P'][1, 0]) == [0, 4, 8]
assert namespace['P'][0, 2, 4].compress(1) == [0, 2, 4]
assert namespace['P'][0, 2, 4].compress(0) == []
assert namespace['P']['x', '-', 'o', '-'].compress(
    namespace['P'][1, 0]) == ['x', 'o']
assert namespace['P'][0, 1, 2].select(
    namespace['P'][1, 0]) == [0, 2, 1]
assert namespace['P'][0, 1, 2].select(
    namespace['P'][1, 1]) == [0, 1, 2]
assert namespace['P'][0, 1, 2].select(1) == [0, 1, 2]
assert namespace['P'][0, 1, 2].select(0) == []
assert namespace['P'][0, 1, 2].select(
    namespace['P'][0, 0]) == []
assert namespace['P'][0, namespace['PGroup'](2, 4), 6].select(
    namespace['P'][1, 0]) == [0, 6, namespace['PGroup'](2, 4)]
assert len(namespace['PRange'](0, 256).compress(1)) == 256
assert len(namespace['PRange'](0, 256).select(
    namespace['P'][1, 0])) == 128
sampleSource = namespace['P'][0, 2, 4, 5, 7, 9]
sampledNotes = sampleSource.sample(4, seed=12345)
assert sampledNotes == sampleSource.sample(4, seed=12345)
assert len(sampledNotes) == 4 and len(set(sampledNotes)) == 4
assert all(note in sampleSource for note in sampledNotes)
assert namespace['P'][1, 1, 2].sample(
    3, seed=7).sort() == [1, 1, 2]
assert namespace['P'][0, 1, 2].sample(0, seed=7) == []
assert namespace['_FoxDotFinitePattern']([]).sample(0, seed=7) == []
for invalid_sample in (
        lambda: sampleSource.sample(-1),
        lambda: sampleSource.sample(7),
        lambda: sampleSource.sample(True),
        lambda: sampleSource.sample(1.5),
        lambda: namespace['_FoxDotFinitePattern']([]).sample(1)):
    try:
        invalid_sample()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('sample must reject invalid sizes and empty sources')
largeSample = namespace['PRange'](0, 256).sample(256, seed=9876)
assert len(largeSample) == 256 and set(largeSample) == set(range(256))

shuffleSource = namespace['P'][0, 1, 2, 3, 4, 5]
deepShuffled = shuffleSource.deep_shuffle(3, seed=2026)
assert deepShuffled == shuffleSource.deep_shuffle(3, seed=2026)
assert len(deepShuffled) == 18
assert all(sorted(deepShuffled[offset:offset + 6]) == list(shuffleSource)
           for offset in (0, 6, 12))
assert len(set(deepShuffled[:6])) == 6
groupedSource = namespace['P'][
    0, namespace['PGroup'](2, 4, 7), 9]
groupedShuffle = groupedSource.deep_shuffle(5, seed=11)
assert len(groupedShuffle) == 15
assert sum(isinstance(value, namespace['_FoxDotChord'])
           for value in groupedShuffle) == 5
assert all(tuple(value) == (2, 4, 7) for value in groupedShuffle
           if isinstance(value, namespace['_FoxDotChord']))
assert shuffleSource.deep_shuffle(0, seed=8) == []
assert namespace['_FoxDotFinitePattern']([]).deep_shuffle(3, seed=8) == []
for invalid_shuffle in (
        lambda: shuffleSource.deep_shuffle(-1),
        lambda: shuffleSource.deep_shuffle(True),
        lambda: shuffleSource.deep_shuffle(1.5),
        lambda: shuffleSource.deep_shuffle(43),
        lambda: namespace['PRange'](0, 129).deep_shuffle(2),
        lambda: shuffleSource.deep_shuffle(1, seed=-1),
        lambda: shuffleSource.deep_shuffle(1, seed=0x100000000),
        lambda: shuffleSource.deep_shuffle(1, seed=True)):
    try:
        invalid_shuffle()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('deep_shuffle must reject invalid counts/seeds')
for invalid_seed in (
        lambda: sampleSource.sample(1, seed=-1),
        lambda: sampleSource.sample(1, seed=0x100000000),
        lambda: sampleSource.sample(1, seed=True)):
    try:
        invalid_seed()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('sample must reject invalid seeds')
for selector_call in (
        lambda: namespace['P'][0, 2].compress(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][0, 2].select(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][0, 2].select(namespace['var']([1, 0], 1)),
        lambda: namespace['P'][0, 2].select(
            namespace['PRange'](0, 251).compress(
                namespace['PRange'](0, 250)))):
    try:
        selector_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('empty, dynamic, or oversized selectors must fail safely')
try:
    namespace['P'][0, 2].compress(list(range(257)))
except ValueError as error:
    assert 'selector is limited to 256' in str(error)
else:
    raise AssertionError('oversized selector was accepted')
assert namespace['P'][None, None, 1, 1, None, None].unduplicate() == [
    None, 1, None]
assert namespace['P']['x', 'x', '-', '-', 'o', 'x', 'x'].unduplicate() == [
    'x', '-', 'o', 'x']
assert namespace['P'][1, namespace['PGroup'](2, 4), namespace['PGroup'](2, 4),
                      3].unduplicate() == [1, (2, 4), 3]
assert namespace['P'][0, 2, 4, 2].replace(2, 7) == [0, 7, 4, 7]
assert namespace['P'][0, None, 4].replace(None, 2) == [0, 2, 4]
assert namespace['P']['x', '-', 'x'].replace('x', 'o') == ['o', '-', 'o']
replacedChord = namespace['P'][
    namespace['PGroup'](0, 4, 7), 2].replace(4, 5)
assert replacedChord == [(0, 5, 7), 2]
assert isinstance(replacedChord[0], namespace['_FoxDotChord'])
nestedPattern = namespace['_FoxDotFinitePattern']([
    namespace['P'][0, 2], namespace['PGroup'](2, 4)])
replacedNested = nestedPattern.replace(2, 3)
assert replacedNested == [[0, 3], (3, 4)]
assert isinstance(replacedNested[0], namespace['_FoxDotFinitePattern'])
assert isinstance(replacedNested[1], namespace['_FoxDotChord'])
replacementPattern = namespace['P'][8, 9]
replacementResult = namespace['P'][1, 2].replace(2, replacementPattern)
assert replacementResult == [1, [8, 9]]
assert replacementResult[1] is replacementPattern
untouchedPattern = namespace['P'][1, 2]
assert untouchedPattern.replace(3, 4) == [1, 2]
assert untouchedPattern == [1, 2]
assert namespace['_FoxDotFinitePattern']([]).replace(1, 2) == []
assert namespace['P'][0, 1, 2, 3].layer('reverse') == [
    (0, 3), (1, 2), (2, 1), (3, 0)]
assert namespace['P'][0, 1, 2].layer('rotate', 1) == [
    (0, 1), (1, 2), (2, 0)]
assert namespace['P'][0, namespace['PGroup'](0, 4, 7)].layer('reverse') == [
    (0, 0, 4, 7), (0, 4, 7, 0)]
assert namespace['P']['x', 'o', '-'].layer('reverse') == [
    ('x', '-'), ('o', 'o'), ('-', 'x')]
assert namespace['P'][0, 1].layer('replace', 1, 2) == [(0, 0), (1, 2)]
assert namespace['P'][0, 1, 2, 3].trim(2) == [0, 1]
assert namespace['P'][0, 1, 2, 3].trim(-1) == [0, 1, 2]
assert namespace['P'][0, 1, 2, 3].trim(0) == []
assert namespace['P'][0, 1, 2, 3].ltrim(2) == [2, 3]
assert namespace['P'][0, 1, 2, 3].ltrim(-1) == [1, 2, 3]
assert namespace['P'][0, 1, 2, 3].ltrim(0) == [0, 1, 2, 3]
assert namespace['P']['x', '-', 'o'].trim(2) == ['x', '-']
assert namespace['P'][namespace['PGroup'](0, 4, 7), 2].ltrim(1) == [2]
assert namespace['_FoxDotFinitePattern']([]).trim(2) == []
assert namespace['P'][0, 2, 4].normalise() == [0.0, 0.5, 1.0]
assert namespace['P'][-4, 0, 4].normalise() == [0.0, 0.5, 1.0]
assert namespace['P'][5, 5, 5].normalise() == [0.0, 0.0, 0.0]
normalizedPattern = namespace['P'][-10, -5, 0, 5].normalise()
assert all(0.0 <= value <= 1.0 for value in normalizedPattern)
for normalize_call in (
        lambda: namespace['_FoxDotFinitePattern']([]).normalise(),
        lambda: namespace['P'][0, None, 2].normalise(),
        lambda: namespace['P'][namespace['PGroup'](0, 2), 4].normalise(),
        lambda: namespace['P']['x', 'o'].normalise()):
    try:
        normalize_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('normalise should require a non-empty numeric pattern')
chooseCounterBefore = namespace['_dexter_random_counter']
namespace['_dexter_random_counter'] = 0
chosenOnce = namespace['P'][0, 2, 4, 7].choose()
namespace['_dexter_random_counter'] = 0
chosenAgain = namespace['P'][0, 2, 4, 7].choose()
assert chosenOnce == chosenAgain and chosenOnce in (0, 2, 4, 7)
chordChoice = namespace['P'][
    namespace['PGroup'](0, 4, 7), 2].choose()
assert chordChoice in (namespace['PGroup'](0, 4, 7), 2)
sampleChoice = namespace['P']['x', '-', 'o'].choose()
assert sampleChoice in ('x', '-', 'o')
namespace['_dexter_random_counter'] = chooseCounterBefore
for choose_call in (
        lambda: namespace['_FoxDotFinitePattern']([]).choose(),
        lambda: namespace['P'][0, 'x'].choose()):
    try:
        choose_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('choose should require a non-empty finite pattern')
assert namespace['P'][0, 2, 4].zip(namespace['P'][1, 3, 5]) == [
    (0, 1), (2, 3), (4, 5)]
assert namespace['P'][0, 2, 4].zip(namespace['P'][1, 3]) == [
    (0, 1), (2, 3), (4, 1), (0, 3), (2, 1), (4, 3)]
assert namespace['P'][0, 2].zip(namespace['P'][1, 3, 5]) == [
    (0, 1), (2, 3), (0, 5), (2, 1), (0, 3), (2, 5)]
assert namespace['P'][namespace['PGroup'](0, 4), 2].zip(
    namespace['P'][1, 3]) == [(0, 4, 1), (2, 3)]
assert namespace['P']['x', 'o'].zip(namespace['P']['h', 't']) == [
    ('x', 'h'), ('o', 't')]
deepZipped = namespace['P'][
    namespace['P'][1, 2], 3].deepzip(namespace['P'][4, 5])
assert deepZipped == [
    namespace['P'][(1, 4), (2, 4)], (3, 5)]
deepRight = namespace['P'][1, 2].deepzip(
    namespace['P'][namespace['P'][3, 4], 5])
assert deepRight == [
    namespace['P'][(1, 3), (1, 4)], (2, 5)]
deepReversed = namespace['P'][
    namespace['P'][1, 2], 3].deeprzip(namespace['P'][4, 5])
assert deepReversed == [
    namespace['P'][(4, 1), (4, 2)], (5, 3)]
assert namespace['P'][1, 2].zip(3) == [(1, 3), (2, 3)]
zipxSource = namespace['P'][
    1, namespace['PGroup'](2, 3)]
zipxResult = zipxSource.zipx(namespace['P'][4, 5, 6])
assert len(zipxResult) == 6
assert zipxResult[0] == (1, 4)
assert zipxResult[1] == (namespace['PGroup'](2, 3), 5)
assert isinstance(zipxResult[1][0], namespace['_FoxDotChord'])
assert zipxResult[1] != zipxSource.zip(
    namespace['P'][4, 5, 6])[1]
assert namespace['P'][1, 2].zipx(3) == [(1, 3), (2, 3)]
assert namespace['_FoxDotFinitePattern']().zipx(namespace['P'][1, 2]) == []
sampleZipx = namespace['P']['x', 'o'].zipx(
    namespace['P']['-', 'x'])
assert sampleZipx == [('x', '-'), ('o', 'x')]
assert namespace['_sample_symbols_for_item'](sampleZipx[1]) == [
    'o', 'x']
deepZipxPattern = namespace['P'][1]
for _ in range(17):
    deepZipxPattern = namespace['_FoxDotFinitePattern']([deepZipxPattern])
for invalid_zipx_index, invalid_zipx in enumerate((
        lambda: namespace['_FoxDotFinitePattern'](range(17)).zipx(
            namespace['_FoxDotFinitePattern'](range(19))),
        lambda: namespace['P'][
            0, namespace['PGroup'](1, 2, 3, 4, 5)].zipx(
                namespace['P'][0, namespace['PGroup'](6, 7, 8, 9)]),
        lambda: deepZipxPattern.zipx(namespace['P'][1, 2]),
        lambda: namespace['P'][1, 2].zipx(
            namespace['P']['x', 'o']))):
    try:
        invalid_zipx()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError(
            'zipx must enforce pattern, chord, and item bounds: ' +
            str(invalid_zipx_index))
concatSource = namespace['P'][1, namespace['PGroup'](2, 3)]
concatResult = concatSource.concat(namespace['P'][4, 5])
assert concatResult == [1, namespace['PGroup'](2, 3), 4, 5]
assert concatSource == [1, namespace['PGroup'](2, 3)]
assert namespace['P'][1, 2].concat([3, 4]) == [1, 2, 3, 4]
assert namespace['P'][1, 2].concat(range(3, 5)) == [1, 2, 3, 4]
assert namespace['P'][1, 2].concat(3) == [1, 2, 3]
assert namespace['_FoxDotFinitePattern']().concat(
    namespace['P']['x', 'o']) == ['x', 'o']
assert namespace['P']['x', 'o'].concat('-x') == ['x', 'o', '-', 'x']
assert namespace['P'][1, 2].add(10) == namespace['P'][1, 2] + 10
assert namespace['P'][
    1, namespace['PGroup'](2, 3)].add(10) == [
        11, namespace['PGroup'](12, 13)]
assert namespace['P'][-3, namespace['PGroup'](-2, 4), None].abs() == [
    3, namespace['PGroup'](2, 4), None]
assert abs(namespace['P'][-2, 3]) == [2, 3]
assert namespace['P'][1, 2, 3].all()
assert not namespace['P'][1, 0, 3].all()
assert namespace['P'][1, 2, 3].all(lambda value: value > 0)
assert not namespace['_FoxDotFinitePattern']().all()
assert namespace['P'][1, 2, 3].startswith(1)
assert not namespace['P'][1, 2, 3].startswith(2)
assert not namespace['_FoxDotFinitePattern']().startswith(1)
saved_abs_builtin = namespace.get('abs')
saved_all_builtin = namespace.get('all')
namespace['abs'] = lambda value: 0
namespace['all'] = lambda values: False
assert namespace['P'][-3].abs() == [3]
assert namespace['P'][1, 2].all()
if saved_abs_builtin is None:
    del namespace['abs']
else:
    namespace['abs'] = saved_abs_builtin
if saved_all_builtin is None:
    del namespace['all']
else:
    namespace['all'] = saved_all_builtin
for invalid_method_batch in (
        lambda: namespace['P'][1, 2].concat(namespace['P']['x']),
        lambda: namespace['P'][1].concat(range(257)),
        lambda: namespace['P'][1].concat(2).concat(
            namespace['_FoxDotFinitePattern'](range(255))),
        lambda: namespace['P']['x'].add(1),
        lambda: namespace['P'][-1].abs().all(lambda value: value.__class__)):
    try:
        invalid_method_batch()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('concat/add/abs/all bounds or type checks failed')
limit_builtins = namespace['__builtins__']
every_source = namespace['P'][0, 1, 2]
copy_source = namespace['_FoxDotFinitePattern']([
    namespace['_FoxDotFinitePattern']([1, 2]),
    [3, [4]],
    namespace['PGroup'](0, 5)])
copy_result = copy_source.copy()
assert copy_result == copy_source and copy_result is not copy_source
assert copy_result[0] is not copy_source[0]
assert copy_result[1] is not copy_source[1]
assert copy_result[1][1] is not copy_source[1][1]
copy_result[0][0] = 9
copy_result[1][1][0] = 8
assert copy_source[0][0] == 1 and copy_source[1][1][0] == 4
assert copy_result[2] == copy_source[2]
assert copy_result[2] is not copy_source[2]
copy_scalar_source = namespace['P'][1, 2, 3]
copy_scalar_result = copy_scalar_source.copy()
assert copy_scalar_result == [1, 2, 3]
assert copy_scalar_result is not copy_scalar_source
group_source = namespace['P'][0, 2, 4, 7]
as_group_result = group_source.asGroup()
group_result = group_source.group()
assert isinstance(as_group_result, namespace['_FoxDotChord'])
assert as_group_result == (0, 2, 4, 7)
assert isinstance(group_result, namespace['_FoxDotChord'])
assert group_result == as_group_result
assert group_source == [0, 2, 4, 7]
flatten_source = namespace['P'][0, namespace['PGroup'](2, 4), 7]
flattened_chord = flatten_source.flatten()
assert isinstance(flattened_chord, namespace['_FoxDotChord'])
assert flattened_chord == namespace['PGroup'](0, 2, 4, 7)
assert namespace['P'][0, 2, 4].flatten() == namespace['PGroup'](0, 2, 4)
assert namespace['P'][0, namespace['P'][2, 4], 7].flatten() == \
       namespace['PGroup'](0, 2, 4, 7)
assert flatten_source == [0, namespace['PGroup'](2, 4), 7]
for invalid_flatten in (
        lambda: namespace['_FoxDotFinitePattern']().flatten(),
        lambda: namespace['P'][0, 1, 2, 3, 4, 5, 6, 7, 8].flatten(),
        lambda: namespace['P']['x', 'o'].flatten()):
    try:
        invalid_flatten()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('flatten must enforce chord size and note types')
int_source = namespace['_FoxDotFinitePattern']([
    1.9, -1.9, None,
    namespace['_FoxDotFinitePattern']([2.8, -3.8]),
    namespace['PGroup'](4.9, 5)])
int_result = int_source.int()
assert int_result == [1, -1, None, [2, -3], namespace['PGroup'](4, 5)]
assert isinstance(int_result[3], namespace['_FoxDotFinitePattern'])
assert isinstance(int_result[4], namespace['_FoxDotChord'])
assert int_source[0] == 1.9 and int_source[3][0] == 2.8
float_result = int_source.float()
assert float_result == [1.9, -1.9, None, [2.8, -3.8],
                        namespace['PGroup'](4.9, 5.0)]
assert isinstance(float_result[4], namespace['_FoxDotChord'])
str_source = namespace['_FoxDotFinitePattern']([
    0, 1.5, -2, None, namespace['PGroup'](3, 4),
    namespace['_FoxDotFinitePattern'](['x', 'o', '-'])])
str_result = str_source.str()
assert str_result == [
    '0', '1.5', '-2', None, ('3', '4'),
    namespace['_FoxDotFinitePattern'](['x', 'o', '-'])]
assert isinstance(str_result[0], namespace['_FoxDotNumericText'])
assert not isinstance(str_result[4], namespace['_FoxDotChord'])
assert isinstance(str_result[5], namespace['_FoxDotFinitePattern'])
assert str_source[0] == 0 and str_source[4] == namespace['PGroup'](3, 4)
sample_str_result = namespace['P']['x', 'o', '-'].str()
assert sample_str_result == ['x', 'o', '-']
assert all(namespace['_sample_symbols_for_item'](item) is not None
           for item in sample_str_result)
numeric_str_result = namespace['P'][0, 2, 4].str()
assert numeric_str_result == ['0', '2', '4']
assert all(isinstance(item, namespace['_FoxDotNumericText'])
           for item in numeric_str_result)
assert all(namespace['_sample_symbols_for_item'](item) is None
           for item in numeric_str_result)
assert namespace['_sample_symbols_for_item']('2') == ['2']
try:
    numeric_str_result.asGroup()
except (TypeError, ValueError):
    pass
else:
    raise AssertionError('numeric text must not become chord pitch data')
assert namespace['_FoxDotFinitePattern']().int() == []
assert namespace['_FoxDotFinitePattern']().float() == []
mutation_source = namespace['P'][0, 2, 4]
mutation_shared = mutation_source
mutation_copy = mutation_source.copy()
assert mutation_source.append(6) is None
assert mutation_shared == [0, 2, 4, 6]
assert mutation_copy == [0, 2, 4]
nested_append_input = [7, 9]
assert mutation_source.append(nested_append_input) is None
nested_append_input[0] = 99
assert mutation_source[-1] == [7, 9]
assert mutation_source[-1] is not nested_append_input
extend_source = namespace['P'][1]
extend_shared = extend_source
assert extend_source.extend([3, 5]) is None
assert extend_shared == [1, 3, 5]
assert extend_source.extend(namespace['P'][7, 9]) is None
assert extend_source == [1, 3, 5, 7, 9]
sample_mutation = namespace['P']['x']
assert sample_mutation.extend('o-') is None
assert sample_mutation == ['x', 'o', '-']
full_pattern = namespace['_FoxDotFinitePattern'](range(256))
full_pattern_alias = full_pattern
for invalid_mutation in (
        lambda: full_pattern.append(256),
        lambda: extend_source.extend(iter([11, 12])),
        lambda: namespace['P'][0].extend(range(257)),
        lambda: namespace['P'][0].extend(range(256)),
        lambda: namespace['P'][0].append('x'),
        lambda: namespace['P']['x'].append(1),
        lambda: namespace['P'][0].append(float('inf')),
        lambda: namespace['P'][0].append(namespace['PGroup'](*range(9))),
        lambda: namespace['P'][0].append(namespace['P'][0, 1].str()[0])):
    try:
        invalid_mutation()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError(
            'append/extend must enforce mutation type and size bounds')
assert len(full_pattern_alias) == 256
boundary_pattern = namespace['_FoxDotFinitePattern'](range(255))
assert boundary_pattern.extend([255]) is None
assert len(boundary_pattern) == 256 and boundary_pattern[-1] == 255
try:
    boundary_pattern.append(256)
except ValueError:
    pass
else:
    raise AssertionError('append must reject the 257th top-level item')
insert_source = namespace['P'][0, 2, 4]
insert_shared = insert_source
insert_copy = insert_source.copy()
assert insert_source.insert(1, 1) is None
assert insert_shared == [0, 1, 2, 4]
assert insert_copy == [0, 2, 4]
assert insert_source.insert(-1, 3) is None
assert insert_source == [0, 1, 2, 3, 4]
assert insert_source.insert(-99, -1) is None
assert insert_source[0] == -1
assert insert_source.insert(999, 5) is None
assert insert_source[-1] == 5
nested_insert_source = namespace['P'][0, 2]
nested_insert_input = [5, 7]
assert nested_insert_source.insert(1, nested_insert_input) is None
nested_insert_input[0] = 99
assert nested_insert_source[1] == [5, 7]
assert nested_insert_source[1] is not nested_insert_input

pop_source = namespace['P'][0, 2, 4]
pop_shared = pop_source
pop_copy = pop_source.copy()
assert pop_source.pop() == 4
assert pop_shared == [0, 2]
assert pop_copy == [0, 2, 4]
assert pop_source.pop(0) == 0
assert pop_shared == [2]

remove_source = namespace['P'][2, 2, 4]
remove_shared = remove_source
remove_copy = remove_source.copy()
assert remove_source.remove(2) is None
assert remove_shared == [2, 4]
assert remove_copy == [2, 2, 4]
atomic_pattern = namespace['P'][0, 2, 4]
atomic_alias = atomic_pattern
empty_pattern = namespace['_FoxDotFinitePattern']()
invalid_mutations = (
    (atomic_pattern, lambda: atomic_pattern.insert(1.5, 2), TypeError),
    (atomic_pattern, lambda: atomic_pattern.insert(True, 2), TypeError),
    (atomic_pattern, lambda: atomic_pattern.insert(0, 'x'), TypeError),
    (namespace['P']['x'], lambda: namespace['P']['x'].insert(0, 2), TypeError),
    (atomic_pattern, lambda: atomic_pattern.pop(1.5), TypeError),
    (atomic_pattern, lambda: atomic_pattern.pop(True), TypeError),
    (empty_pattern, lambda: empty_pattern.pop(), IndexError),
    (atomic_pattern, lambda: atomic_pattern.pop(3), IndexError),
    (atomic_pattern, lambda: atomic_pattern.remove(9), ValueError),
    (atomic_pattern, lambda: atomic_pattern.remove(object()), TypeError),
    (full_pattern, lambda: full_pattern.insert(0, 256), ValueError),
)
for target, invalid_mutation, error_type in invalid_mutations:
    before = list(target)
    try:
        invalid_mutation()
    except error_type:
        pass
    else:
        raise AssertionError(
            'insert/pop/remove must reject invalid indices, values, or bounds')
    assert target == before
assert atomic_alias == [0, 2, 4]
assert len(full_pattern_alias) == 256
boundary_insert = namespace['_FoxDotFinitePattern'](range(255))
assert boundary_insert.insert(255, 255) is None
assert len(boundary_insert) == 256 and boundary_insert[-1] == 255
try:
    boundary_insert.insert(0, 256)
except ValueError:
    pass
else:
    raise AssertionError('insert must reject the 257th top-level item')

set_source = namespace['P'][0, 2, 4]
set_shared = set_source
set_copy = set_source.copy()
assert set_source.set(1, 6) is set_source
assert set_shared == [0, 6, 4]
assert set_copy == [0, 2, 4]
set_source.set(-1, namespace['PGroup'](7, 9))
assert set_source[-1] == namespace['PGroup'](7, 9)
nested_set_input = [3, 5]
set_source.set(0, nested_set_input)
nested_set_input[0] = 99
assert set_source[0] == [3, 5]
assert set_source[0] is not nested_set_input

assert namespace['P']['x', 'o', '-'].string() == 'xo-'
assert namespace['P'][0, None, namespace['PGroup'](2, 4), [5, 7]].string() == \
    '0 24(57)'
assert namespace['P'][namespace['P'][1, 2],
                       namespace['PGroup'](3, 5)].string() == '(12)35'
assert namespace['P'][1, 2].str().string() == '12'
assert namespace['P']['|x1|'].string() == '|x1|'
assert len(namespace['_FoxDotFinitePattern'](range(256)).string()) > 0
assert len(namespace['P']['x' * 4096].string()) == 4096
try:
    namespace['P']['x' * 4097].string()
except ValueError:
    pass
else:
    raise AssertionError('string must enforce its output-length bound')
invalid_string_pattern = namespace['P'][object()]
try:
    invalid_string_pattern.string()
except TypeError:
    pass
else:
    raise AssertionError('string must reject unsupported object values')
try:
    namespace['_FoxDotFinitePattern'](range(257)).string()
except ValueError:
    pass
else:
    raise AssertionError('string must reject more than 256 top-level items')
too_deep_string = namespace['P'][1]
for _ in range(17):
    too_deep_string = namespace['_FoxDotFinitePattern']([too_deep_string])
try:
    too_deep_string.string()
except ValueError:
    pass
else:
    raise AssertionError('string must enforce its nesting bound')

items_source = namespace['P'][3, [5, 7], None]
item_pairs = items_source.items()
assert isinstance(item_pairs, list)
assert item_pairs == [(0, 3), (1, [5, 7]), (2, None)]
assert item_pairs[1][1] is not items_source[1]
items_source.set(0, 9)
assert item_pairs[0] == (0, 3)
assert namespace['_FoxDotFinitePattern']().items() == []
assert len(namespace['_FoxDotFinitePattern'](range(256)).items()) == 256
try:
    namespace['_FoxDotFinitePattern'](range(257)).items()
except ValueError:
    pass
else:
    raise AssertionError('items must reject more than 256 top-level values')

getitem_source = namespace['P'][10, 20, 30]
assert getitem_source.getitem(0) == 10
assert getitem_source.getitem(3) == 10
assert getitem_source.getitem(-1) == 30
assert getitem_source.getitem((2, 1)) == [30, 20]
assert getitem_source.getitem(namespace['P'][0, 2, 4, 6]) == [
    10, 30, 20, 10]
assert getitem_source.getitem(slice(1, 3)) == [20, 30]
nested_getitem = namespace['P'][0, namespace['P'][2, 4]]
assert [nested_getitem.getitem(index) for index in (0, 1, 3, 5)] == [
    0, 2, 4, 2]
invalid_getitem_calls = (
    lambda: getitem_source.getitem(True),
    lambda: getitem_source.getitem(1.5),
    lambda: namespace['_FoxDotFinitePattern']().getitem(0),
    lambda: getitem_source.getitem(
        namespace['_FoxDotFinitePattern'](range(257))),
)
for invalid_getitem in invalid_getitem_calls:
    try:
        invalid_getitem()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('getitem must reject invalid or oversized indices')

comparison_source = namespace['P'][0, 1, 0, 1]
assert comparison_source.eq(namespace['P'][0, 1]) == [1, 1, 1, 1]
assert comparison_source.ne(namespace['P'][0, 0]) == [0, 1, 0, 1]
assert comparison_source.eq(1) == [0, 1, 0, 1]
assert namespace['P']['x', 'o'].eq('x') == [1, 0]
assert namespace['_FoxDotFinitePattern']().eq([]) == []
assert list(namespace['P'][1, 2, 3] == 2) == [0, 1, 0]
assert list(namespace['P'][1, 2, 3] != 2) == [1, 0, 1]
assert list(namespace['P'][1, 3, 5] < 4) == [1, 1, 0]
assert list(namespace['P'][1, 3, 5] <= 3) == [1, 1, 0]
assert list(namespace['P'][1, 3, 5] > 3) == [0, 0, 1]
assert list(namespace['P'][1, 3, 5] >= 3) == [0, 1, 1]
assert list(namespace['P'][1, 4] < namespace['P'][2, 3, 0]) == [
    1, 0, 0, 0, 1, 0]
assert list(namespace['P'][None, 2] == namespace['P'][None, 3]) == [1, 0]
assert list(namespace['P'][None, 2] < namespace['P'][None, 3]) == [
    None, 1]
assert list(namespace['P']['x', 'o'] == 'x') == [1, 0]
assert list(namespace['P'][0, 1.5, -2].str() ==
            namespace['P']['0', '1.5', '-2']) == [1, 1, 1]
assert list(namespace['P'][
    namespace['PGroup'](1, 4), 2] >
    namespace['P'][namespace['PGroup'](0, 5), 3]) == [
        namespace['PGroup'](1, 0), 0]
assert bool(namespace['P'][1, 2] == [1, 2])
assert not bool(namespace['P'][1, 2] == [1, 3])
assert list(~namespace['P'][1, namespace['PGroup'](2, 3)]) == [
    namespace['PGroup'](3, 2), 1]
for invalid_comparison in (
        lambda: namespace['P'][1] < 'x',
        lambda: namespace['P'][1, 2] == [],
        lambda: namespace['P'][1] == object(),
        lambda: namespace['_FoxDotFinitePattern']([
            namespace['_FoxDotChord'](1, 2)]) == namespace[
                '_FoxDotFinitePattern']([
                    namespace['_FoxDotChord'](1, 2, 3)]),
        lambda: namespace['PRange'](16) < namespace['PRange'](17),
        lambda: namespace['_FoxDotFinitePattern'](range(257)) == 0):
    try:
        invalid_comparison()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('comparison operators must enforce value and size bounds')
assert list(~namespace['P'][1, 2, 3]) == [3, 2, 1]
try:
    comparison_source.eq([])
except ValueError:
    pass
else:
    raise AssertionError('eq must reject an empty cycled pattern')

rotate_source = namespace['P'][1, 2, 3]
rotate_alias = rotate_source
rotate_copy = rotate_source.copy()
assert rotate_source.i_rotate(1) is rotate_source
assert rotate_alias == [2, 3, 1]
assert rotate_copy == [1, 2, 3]
rotate_source.i_rotate(-1)
assert rotate_source == [1, 2, 3]
reverse_source = namespace['P'][1, 2, 3]
assert reverse_source.i_reverse() is reverse_source
assert reverse_source == [3, 2, 1]
sort_source = namespace['P'][3, 1, 2]
sort_alias = sort_source
sort_copy = sort_source.copy()
assert sort_source.i_sort() is sort_source
assert sort_alias == [1, 2, 3]
assert sort_copy == [3, 1, 2]
unsortable = namespace['P'][None, 1]
try:
    unsortable.i_sort()
except TypeError:
    pass
else:
    raise AssertionError('i_sort must reject incomparable values')
assert unsortable == [None, 1]
shuffle_source = namespace['P'][1, 2, 3, 4]
shuffle_alias = shuffle_source
shuffle_copy = shuffle_source.copy()
assert shuffle_source.i_shuf() is shuffle_source
assert sorted(shuffle_source) == [1, 2, 3, 4]
assert shuffle_alias is shuffle_source
assert shuffle_copy == [1, 2, 3, 4]

setitem_source = namespace['P'][1, 2, 3]
setitem_alias = setitem_source
setitem_copy = setitem_source.copy()
assert setitem_source.setitem(1, 8) is None
assert setitem_alias == [1, 8, 3]
assert setitem_copy == [1, 2, 3]
try:
    setitem_source.setitem(9, 4)
except IndexError:
    pass
else:
    raise AssertionError('setitem must reject an out-of-range index')

slice_source = namespace['P'][0, 10, 20]
assert slice_source.getslice(1, 3) == [10, 20]
assert slice_source.getslice(-2, None) == [10, 20, 0, 10, 20]
assert slice_source.getslice(0, 3, 2) == [0, 20]
assert namespace['_FoxDotFinitePattern']().getslice(0, 0) == []
for invalid_slice_args in ((0, 3, 0), (0.5, 2, 1), (0, 300, 1)):
    try:
        slice_source.getslice(*invalid_slice_args)
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('getslice must enforce integer and output bounds')

alt_source = namespace['P'][0, 2, 4]
alt_shared = alt_source
alt_input = [7, 9, 11]
alternate = alt_source.alt(alt_input)
alt_input[0] = 99
assert alternate == [7, 9, 11]
assert alternate is not alt_source and alternate is not alt_shared
assert alt_source == [0, 2, 4] and alt_shared == [0, 2, 4]
alternate.append(12)
assert alt_source == [0, 2, 4]
assert namespace['P'][7].alt(9) == [9]
assert namespace['P'][0].alt('xo-') == ['x', 'o', '-']
assert len(namespace['P'][0].alt(range(256))) == 256
assert namespace['P'][0].alt([]) == []
set_boundary = namespace['_FoxDotFinitePattern'](range(256))
assert set_boundary.set(0, 7) is set_boundary
assert len(set_boundary) == 256 and set_boundary[0] == 7

set_atomic = namespace['P'][0, 2, 4]
set_alias = set_atomic
invalid_set_calls = (
    lambda: set_atomic.set(1.5, 7),
    lambda: set_atomic.set(True, 7),
    lambda: set_atomic.set(5, 7),
    lambda: set_atomic.set(-5, 7),
    lambda: set_atomic.set(0, 'x'),
    lambda: set_atomic.set(0, namespace['PGroup'](*range(9))),
    lambda: alt_source.alt(range(257)),
    lambda: alt_source.alt([0, 'x']),
    lambda: alt_source.alt(object()),
    lambda: alt_source.alt(namespace['P'][0, 1].str()[0]),
)
for invalid_set_call in invalid_set_calls:
    before_set = list(set_atomic)
    before_alt = list(alt_source)
    try:
        invalid_set_call()
    except (TypeError, ValueError, IndexError):
        pass
    else:
        raise AssertionError('set/alt must reject invalid inputs and bounds')
    assert set_atomic == before_set and set_alias == before_set
    assert alt_source == before_alt and alt_shared == before_alt
too_deep_cast = namespace['P'][1]
for _ in range(17):
    too_deep_cast = namespace['_FoxDotFinitePattern']([too_deep_cast])
for invalid_cast in (
        lambda: namespace['_FoxDotFinitePattern'](range(257)).int(),
        lambda: namespace['_FoxDotFinitePattern'](
            [namespace['_FoxDotFinitePattern'](range(129)),
             namespace['_FoxDotFinitePattern'](range(128))]).float(),
        lambda: too_deep_cast.int(),
        lambda: namespace['P'][True].int(),
        lambda: namespace['P']['x'].float(),
        lambda: namespace['P'][float('inf')].int(),
        lambda: namespace['P'][float('nan')].float(),
        lambda: namespace['P'][float('nan')].str(),
        lambda: namespace['_FoxDotFinitePattern'](range(257)).str(),
        lambda: too_deep_cast.str()):
    try:
        invalid_cast()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError(
            'int/float must enforce numeric and finite structure bounds')
for invalid_group in (
        lambda: namespace['_FoxDotFinitePattern']().asGroup(),
        lambda: namespace['P'][0, 1, 2, 3, 4, 5, 6, 7, 8].group(),
        lambda: namespace['P'][0, None].asGroup(),
        lambda: namespace['P']['x', 'o'].group(),
        lambda: namespace['P'][namespace['PGroup'](0, 2), 4].asGroup()):
    try:
        invalid_group()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError(
            'asGroup/group must enforce finite, flat, bounded note input')
too_deep_copy = namespace['P'][1]
for _ in range(17):
    too_deep_copy = namespace['_FoxDotFinitePattern']([too_deep_copy])
for invalid_copy_index, invalid_copy in enumerate((
        lambda: namespace['_FoxDotFinitePattern'](range(257)).copy(),
        lambda: too_deep_copy.copy())):
    try:
        invalid_copy()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError(
            'copy must enforce input and nesting bounds: ' +
            str(invalid_copy_index))
assert every_source.every(3, 'reverse') == [0, 1, 2, 0, 1, 2, 2, 1, 0]
assert every_source.every(2, 'rotate', 1) == [0, 1, 2, 1, 2, 0]
assert namespace['P'][1, 3].every(2, 'stutter', 2) == [
    1, 3, 1, 1, 3, 3]
assert namespace['P'][1, 2].every(2, 'mirror') == [1, 2, 2, 1]
every_shuffle = namespace['P'][1, 2, 3].every(2, 'shuffle')
assert len(every_shuffle) == 6 and every_shuffle[:3] == [1, 2, 3]
assert sorted(every_shuffle[3:]) == [1, 2, 3]
assert namespace['P']['x', 'o', '-'].every(2, 'reverse') == [
    'x', 'o', '-', '-', 'o', 'x']
assert every_source == [0, 1, 2]
for invalid_every_index, invalid_every in enumerate((
        lambda: every_source.every(0, 'reverse'),
        lambda: every_source.every(257, 'reverse'),
        lambda: every_source.every(True, 'reverse'),
        lambda: every_source.every(2, 'append', 3),
        lambda: every_source.every(2, 'reverse', unexpected=True),
        lambda: every_source.every(2, 'rotate', 1.5),
        lambda: namespace['P'][1].every(257, 'reverse'),
        lambda: namespace['P'][0, 1].every(200, 'stutter', 2))):
    try:
        invalid_every()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError(
            'every must enforce transform and output bounds: ' +
            str(invalid_every_index))
assert namespace['P'][0, 1, 2, 3].limit(
    limit_builtins['sum'], 10) == [0, 1, 2, 3, 0, 1, 2, 3]
assert namespace['P'][1, 2, 3].limit(
    limit_builtins['sum'], 8) == [1, 2, 3, 1, 2]
assert namespace['P'][8, 9].limit(limit_builtins['sum'], 0) == []
assert namespace['P'][1, 2, 3].limit(
    limit_builtins['len'], 3) == [1, 2, 3]
assert namespace['P'][1, 4, 7, 2].limit(
    limit_builtins['max'], 7) == [1, 4, 7]
for invalid_limit_index, invalid_limit in enumerate((
        lambda: namespace['P'][1].limit(limit_builtins['sum'], 257),
        lambda: namespace['P'][-1].limit(limit_builtins['sum'], 1),
        lambda: namespace['P'][1, 2].limit(
            limit_builtins['max'], 3),
        lambda: namespace['P'][1].limit(limit_builtins['min'], 2),
        lambda: namespace['P'][1].limit(lambda values: len(values), 2),
        lambda: namespace['P']['x'].limit(limit_builtins['sum'], 1),
        lambda: namespace['_FoxDotFinitePattern'](
            [namespace['PGroup'](1, 2)]).limit(
            limit_builtins['sum'], 2),
        lambda: namespace['_FoxDotFinitePattern'](range(257)).limit(
            limit_builtins['len'], 2),
        lambda: namespace['P']().limit(limit_builtins['sum'], 2))):
    try:
        invalid_limit()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError(
            'limit must enforce numeric, aggregator, and output bounds: ' +
            str(invalid_limit_index))
assert namespace['P'][1, 3, 4].map({1: 6, 4: 2}) == [6, 0, 2]
assert namespace['P'][
    namespace['P'][1, 3],
    namespace['PGroup'](0, 4)].map({1: 7, 3: 2, 0: 4, 4: 6}) == [
        [7, 2], namespace['PGroup'](4, 6)]
assert namespace['P']['x', 'o'].map({'x': 'o', 'o': 'x'}) == ['o', 'x']
assert namespace['P'][0, None].map({}) == [0, 0]
assert namespace['P'][1].map({1: (2, 4)}) == [(2, 4)]
assert namespace['P'][-3, -1, 2].map(abs) == [3, 1, 2]
assert namespace['P'][-3, -1, 2].map(func=abs) == [3, 1, 2]
assert namespace['P'][0, -2, 3].map(lambda value: value * 2) == [0, -4, 6]
assert namespace['P'][1, 2].map(mapping={1: 9}) == [9, 0]
callableMappedGroup = namespace['P'][
    namespace['P'][1, -4], 2].map(lambda value: abs(value) + 1)
assert callableMappedGroup == [[2, 5], 3]
assert isinstance(callableMappedGroup[0], namespace['_FoxDotFinitePattern'])
transformedNested = namespace['P'][
    -1,
    namespace['PGroup'](-2, 3),
    namespace['P'][4, namespace['PGroup'](-5, 6)]
].transform(abs)
assert transformedNested == [
    1, namespace['PGroup'](2, 3), [4, namespace['PGroup'](5, 6)]]
assert transformedNested.transform(
    lambda value: value + 1) == [
        2, namespace['PGroup'](3, 4), [5, namespace['PGroup'](6, 7)]]
assert namespace['P'][1.2, 2.9].transform(int) == [1, 2]
assert namespace['P'][1, -2].transform(str) == ['1', '-2']
assert namespace['_FoxDotFinitePattern'](range(256)).transform(
    lambda value: value + 1) == list(range(1, 257))
assert namespace['P'][1, 2, 3].map({1: 8}) == [8, 0, 0]
assert namespace['P'][1, 2, 3].submap({1: 8}) == [8, 2, 3]
assert namespace['P'][namespace['P'][1, 2],
                      namespace['PGroup'](3, 4)].submap(
                          {1: 8, 3: 9}) == [[8, 2], (9, 4)]
assert isinstance(namespace['P'][1, 2].submap({1: 8})[0], int)
assert namespace['_FoxDotFinitePattern'](range(256)).map(
    lambda value: value + 1) == list(range(1, 257))
assert namespace['_FoxDotFinitePattern'](range(256)).submap(
    {0: 9, 255: 8})[1:255] == list(range(1, 255))
shadowedMapperGlobals = {
    '__builtins__': {'abs': lambda value: value},
    'abs': lambda value: value,
}
shadowedMapper = types.FunctionType(
    (lambda value: abs(value)).__code__, shadowedMapperGlobals)
for invalid_mapper in (
        lambda: namespace['P'][0, 1].map([1, 2]),
        lambda: namespace['P'][0, 1].map(lambda value: object()),
        lambda: namespace['P'][0, 1].map(lambda value: value.__class__),
        lambda: namespace['P'][0, 1].map(lambda value: value ** 2),
        lambda: namespace['P'][0, 1].map(shadowedMapper),
        lambda: namespace['P'][*range(257)].map(abs),
        lambda: namespace['P'][0, 1].submap([0, 1]),
        lambda: namespace['P'][0, 1].submap(dict.fromkeys(range(257), 0))):
    try:
        invalid_mapper()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('map/submap must validate callables, values, and bounds')
for invalid_transform in (
        lambda: namespace['P'][0, 1].transform({0: 2}),
        lambda: namespace['P'][0, 1].transform(lambda value: value ** 2),
        lambda: namespace['P'][0, 1].transform(lambda value: object()),
        lambda: namespace['P'][0].transform(lambda value: ((),) * 256),
        lambda: namespace['_FoxDotFinitePattern'](range(257)).transform(abs),
        lambda: namespace['_FoxDotFinitePattern']([
            namespace['_FoxDotFinitePattern']([
                namespace['_FoxDotFinitePattern']()])
            for _ in range(256)]).transform(abs)):
    try:
        invalid_transform()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('transform must enforce callable and size bounds')
deepMapPattern = namespace['P'][0]
for _ in range(17):
    deepMapPattern = namespace['_FoxDotFinitePattern']([deepMapPattern])
for method in ('map', 'submap'):
    try:
        (deepMapPattern.map(abs) if method == 'map'
         else deepMapPattern.submap({}))
    except ValueError as error:
        assert '16 nested pattern levels' in str(error)
    else:
        raise AssertionError(method + ' must enforce its nesting bound')
try:
    deepMapPattern.transform(abs)
except ValueError as error:
    assert '16 nested pattern levels' in str(error)
else:
    raise AssertionError('transform must enforce its nesting bound')
sharedNested = namespace['P'][2, 3]
trueCopySource = namespace['P'][1, sharedNested,
                                namespace['PGroup'](4, 5)]
trueCopyResult = trueCopySource.true_copy()
assert trueCopyResult is not trueCopySource
assert trueCopyResult[1] is sharedNested
assert trueCopyResult[2] is trueCopySource[2]
trueCopyResult[0] = 9
assert trueCopySource[0] == 1
sharedNested.append(6)
assert trueCopyResult[1][-1] == 6 and trueCopySource[1][-1] == 6
replacementData = [sharedNested, namespace['PGroup'](7, 8)]
replacementCopy = trueCopySource.true_copy(replacementData)
assert len(replacementCopy) == 2 and replacementCopy[0] is sharedNested
replacementCopy.append(10)
assert len(replacementData) == 2
convertedPattern = namespace['P'][
    1.8,
    namespace['PGroup'](2.9, 4.1),
    namespace['P'][5.7, 6.2]
].convert_data(int)
assert convertedPattern == [
    1, namespace['PGroup'](2, 4), [5, 6]]
assert convertedPattern.convert_data() == [
    1.0, namespace['PGroup'](2.0, 4.0), [5.0, 6.0]]
assert namespace['P'][1, -2].convert_data(str) == ['1', '-2']
assert namespace['_FoxDotFinitePattern']().true_copy() == []
assert namespace['_FoxDotFinitePattern']().convert_data(int) == []
methodNames = namespace['P'][0].get_methods()
assert len(methodNames) == len(set(methodNames))
assert {'get_data', 'get_methods', 'help', 'new', 'make',
        'get_behaviour', 'transform', 'convert_data'}.issubset(
            set(methodNames))
assert '__class__' not in methodNames
assert '256 nodes' in namespace['P'][0].help()
assert namespace['P'][0].get_behaviour() is None
liveDataPattern = namespace['P'][1, 2]
assert liveDataPattern.get_data() is liveDataPattern
liveDataPattern.get_data().append(3)
assert liveDataPattern == [1, 2, 3]
wrappedData = namespace['_FoxDotFinitePattern']([
    namespace['P'][4, 5]])
assert wrappedData.get_data() is wrappedData[0]
templatePattern = namespace['P'][0]
assert templatePattern.new([3, 5]) == [3, 5]
assert templatePattern.new('x-o') == ['x-o']
assert templatePattern.new(sharedNested) == [2, 3, 6]
assert templatePattern.new(7) == [7]
singleNestedForMake = namespace['_FoxDotFinitePattern']([
    namespace['P'][2, 4]])
assert singleNestedForMake.make() is singleNestedForMake
assert singleNestedForMake == [2, 4]
multipleNestedForMake = namespace['_FoxDotFinitePattern']([
    namespace['P'][2, 4], 6])
assert multipleNestedForMake.make() is multipleNestedForMake
assert multipleNestedForMake == [[2, 4], 6]
assert namespace['P']['x-o'].make() == ['x-o']
too_many_true_copy_nodes = namespace['_FoxDotFinitePattern']([
    namespace['_FoxDotFinitePattern']([
        namespace['_FoxDotFinitePattern']()])
    for _ in range(256)])
for invalid_copy_conversion in (
        lambda: namespace['P'][0].true_copy(object()),
        lambda: namespace['P'][0].true_copy([object()]),
        lambda: namespace['P'][0].true_copy(range(257)),
        lambda: too_many_true_copy_nodes.true_copy(),
        lambda: namespace['P'][0].new(range(257)),
        lambda: namespace['_FoxDotFinitePattern'](range(257)).make(),
        lambda: namespace['P'][0].convert_data(lambda value: value ** 2),
        lambda: namespace['P'][0].convert_data(int, 2),
        lambda: namespace['_FoxDotFinitePattern'](range(257)).convert_data(int)):
    try:
        invalid_copy_conversion()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError(
            'true_copy/convert_data must enforce their callable and data bounds')
try:
    namespace['P'][None].convert_data(int)
except TypeError:
    pass
else:
    raise AssertionError(
        'convert_data(int) should preserve upstream conversion errors on rests')
assert namespace['P'][0, 1, 2, 3].pivot(0) == [0, 3, 2, 1]
assert namespace['P'][0, 1, 2, 3].pivot(1) == [2, 1, 0, 3]
assert namespace['P'][0, 1, 2, 3].pivot(3) == [2, 1, 0, 3]
assert namespace['P'][0, 1, 2, 3].pivot(
    namespace['P'][0, 2]) == [0, 3, 2, 1, 0, 3, 2, 1]
for zip_call in (
        lambda: namespace['P'][0, 1].zip(
            namespace['P'][2, 3], dtype=namespace['PGroup']),
        lambda: namespace['P'][*range(16)].zip(
            namespace['P'][*range(17)]),
        lambda: namespace['P'][*range(16)].deepzip(
            namespace['P'][*range(17)])):
    try:
        zip_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('zip helpers must enforce safe output bounds')
deepPattern = namespace['P'][0]
for _ in range(17):
    deepPattern = namespace['_FoxDotFinitePattern']([deepPattern])
try:
    deepPattern.deepzip(namespace['P'][1])
except ValueError as error:
    assert '16 nested pattern levels' in str(error)
else:
    raise AssertionError('deepzip must enforce the nested-depth bound')
for pattern_call in (
        lambda: namespace['P'][0, 1].map([1, 2]),
        lambda: namespace['P'][0].map({0: tuple(range(257))}),
        lambda: namespace['P'][*range(129)].map({
            value: (value, value + 1) for value in range(129)}),
        lambda: namespace['P'][0, 1].pivot(1.5),
        lambda: namespace['P'][*range(17)].pivot(
            namespace['P'][*range(17)])):
    try:
        pattern_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('map/pivot must validate inputs and enforce bounds')
for slice_call in (
        lambda: namespace['P'][0, 1].trim(1.5),
        lambda: namespace['P'][0, 1].ltrim(True)):
    try:
        slice_call()
    except TypeError:
        pass
    else:
        raise AssertionError('trim/ltrim require integer sizes')
for helper in (
        lambda: namespace['P']['x', 'o'].arp(namespace['P'][0, 2]),
        lambda: namespace['P'][0, 2].arp(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][0, 1].arp(
            namespace['P'][*range(129)]),
        lambda: namespace['P'][0, 1].splice(),
        lambda: namespace['P'][0, 1].splice(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][0, 1].splice(
            namespace['P']['x', 'o'])):
    try:
        helper()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('arp/splice should reject unsupported or unbounded input')
try:
    namespace['P'][2, None, 1].sort()
except TypeError:
    pass
else:
    raise AssertionError('sort should retain FoxDot ordering requirements')
deepPattern = namespace['P'][1]
for _ in range(18):
    deepPattern = namespace['_FoxDotFinitePattern']([deepPattern])
try:
    deepPattern.replace(1, 2)
except ValueError as error:
    assert '16 nested pattern levels' in str(error)
else:
    raise AssertionError('replace must enforce its nested-pattern depth bound')
for layer_call in (
        lambda: namespace['P'][0, 1].layer('layer'),
        lambda: namespace['P'][0, 1].layer('__class__'),
        lambda: namespace['P'][0, 1].layer(namespace['reverse']),
        lambda: namespace['P'][*range(255)].layer('stretch', 256)):
    try:
        layer_call()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('layer should enforce its transform allowlist and output bound')
try:
    namespace['P'][*range(86)].splice(
        namespace['P'][*range(86)], namespace['P'][*range(86)])
except ValueError as error:
    assert '256 items' in str(error)
else:
    raise AssertionError('splice must enforce the 256-item output cap')
shuffleSource = namespace['P'][(0, 4), None, 7, 2]
shuffleBefore = list(shuffleSource)
shuffleCopies = shuffleSource.shuffle(2)
assert len(shuffleCopies) == 2 * len(shuffleSource)
for chunkStart in (0, len(shuffleSource)):
    assert sorted(map(repr, shuffleCopies[chunkStart:chunkStart + len(shuffleSource)])) == \
        sorted(map(repr, shuffleBefore))
assert list(shuffleSource) == shuffleBefore
sampleShuffle = namespace['P']['x', 'o', ' ', '-'].shuffle(2)
assert len(sampleShuffle) == 8
assert sorted(sampleShuffle[:4]) == [' ', '-', 'o', 'x']
assert sorted(sampleShuffle[4:]) == [' ', '-', 'o', 'x']
assert namespace['P'][1, 2].shuffle(0) == []
trueSource = namespace['P'][
    namespace['P'][1, 2], 3, namespace['PGroup'](4, 5)]
trueShuffle = trueSource.true_shuffle(2)
assert len(trueShuffle) == 8
trueExpanded = [1, 2, 3, namespace['PGroup'](4, 5)]
for chunkStart in (0, 4):
    assert sorted(map(repr, trueShuffle[chunkStart:chunkStart + 4])) == \
        sorted(map(repr, trueExpanded))
shuffleSeedCounter = namespace['_dexter_shuffle_counter']
namespace['_dexter_shuffle_counter'] = 0
deterministicTrue = namespace['P'][0, 2, 4, 7].true_shuffle(2)
namespace['_dexter_shuffle_counter'] = 0
assert deterministicTrue == namespace['P'][0, 2, 4, 7].true_shuffle(2)
namespace['_dexter_shuffle_counter'] = 0
deterministicGroups = namespace['P'][0, 2, 4, 7].shufflets(2)
namespace['_dexter_shuffle_counter'] = 0
assert deterministicGroups == namespace['P'][0, 2, 4, 7].shufflets(2)
namespace['_dexter_shuffle_counter'] = shuffleSeedCounter
assert len(namespace['P'][1, 2, 3].true_shuffle(
    namespace['P'][1, 2])) == 9
trueSampleShuffle = namespace['P']['x', '-', 'o'].true_shuffle(2)
assert len(trueSampleShuffle) == 6
assert sorted(trueSampleShuffle[:3]) == ['-', 'o', 'x']
assert sorted(trueSampleShuffle[3:]) == ['-', 'o', 'x']
shuffleGroups = namespace['P'][0, 2, 4, 7].shufflets(3)
assert len(shuffleGroups) == 3
assert all(isinstance(group, namespace['_FoxDotChord'])
           and sorted(group) == [0, 2, 4, 7] for group in shuffleGroups)
assert len(namespace['P'][0, 2, 4].shufflets(
    namespace['P'][1, 2])) == 3
assert namespace['P'][0, 2].true_shuffle(0) == []
assert namespace['P'][0, 2].shufflets(0) == []
for random_method in (
        lambda: namespace['P'][0, 1].true_shuffle(True),
        lambda: namespace['P'][0, 1].true_shuffle(1.5),
        lambda: namespace['P'][0, 1].true_shuffle(
            namespace['P'][*range(257)]),
        lambda: namespace['P']['x', 'o'].shufflets(1),
        lambda: namespace['P'][0, None].shufflets(1),
        lambda: namespace['P'][*range(9)].shufflets(1),
        lambda: namespace['P'][0, 1].shufflets(-1)):
    try:
        random_method()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('true_shuffle/shufflets must enforce input bounds')
namespace['_dexter_shuffle_counter'] = shuffleCounterBeforeMethodTests
assert namespace['P'][1, 2, 3].palindrome() == [1, 2, 3, 3, 2, 1]
assert namespace['P'][1, namespace['P'][2, 3], 4].mirror() == [
    4, [3, 2], 1]
assert namespace['P'][1, 2, 3, 4].accum() == [0, 1, 3, 6]
assert namespace['P'][1, 2, 3].accum(5) == [0, 1, 3, 6, 7]
assert namespace['P'][1, 2].accum(namespace['P'][2, 3]) == [
    0, 1, 0, 1, 3]
assert namespace['P'][0, 1, 2, 3, 4].swap(2) == [1, 0, 3, 2, 4]
assert namespace['P'][0, 1, 2, 3, 4, 5].swap(
    namespace['P'][2, 3]) == [1, 0, 3, 2, 5, 4, 2, 1, 0, 5, 4, 3]
assert namespace['P']['x', namespace['P']['o', ' ']].mirror() == [
    [' ', 'o'], 'x']
for helper in (
        lambda: namespace['P'][1, 2].accum(257),
        lambda: namespace['P'][1, None, 2].accum(),
        lambda: namespace['P'][1, 2].swap(0),
        lambda: namespace['P'][1, 2].swap(1.5),
        lambda: namespace['P'][1, 2].stretch(True),
        lambda: namespace['P'][1, 2].stretch(1.5),
        lambda: namespace['P'][1, 2].stretch(257),
        lambda: namespace['P'][1, 2].stretch(namespace['P'][200, 100]),
        lambda: namespace['P'][1, 2].stretch(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][1, 2].loop(True),
        lambda: namespace['P'][1, 2].loop(-1),
        lambda: namespace['P'][1, 2].loop(1.5),
        lambda: namespace['P'][1, 2].loop(129),
        lambda: namespace['P'][1, 2].loop(
            namespace['_FoxDotFinitePattern']([])),
        lambda: namespace['P'][1, 2].shuffle(True),
        lambda: namespace['P'][1, 2].shuffle(-1),
        lambda: namespace['P'][1, 2].shuffle(1.5),
        lambda: namespace['P'][1, 2].shuffle(129),
        lambda: namespace['P']['x', 'o'].invert(),
        lambda: namespace['P'][0, [1, 'x']].invert(),
        lambda: namespace['P'][0, (1, 2, 3, 4, 5, 6, 7, 8, 9)].invert()):
    try:
        helper()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('bounded pattern helper should reject invalid input')
try:
    namespace['P'][*range(129)].repeat(2)
except ValueError as error:
    assert '256 expanded items' in str(error)
else:
    raise AssertionError('pattern repeat must enforce the 256-item cap')
left = namespace['P'][0, None, (2, 4)]
right = namespace['P'][5, 7]
assert left | right == [0, None, (2, 4), 5, 7]
assert [9] | namespace['P'][1, 2] == [9, 1, 2]
assert namespace['P']['x', ' '] | namespace['P']['o', '-'] == ['x', ' ', 'o', '-']
assert namespace['P']['x'] | 'o ' == ['x', 'o', ' ']
assert 'o ' | namespace['P']['x'] == ['o', ' ', 'x']
emptyPattern = namespace['_FoxDotFinitePattern']([])
assert emptyPattern | namespace['P']['x'] == ['x']
assert emptyPattern | namespace['P'][1] == [1]
try:
    namespace['P']['x'] | namespace['P'][1]
except TypeError as error:
    assert 'sample symbols or space-rests on both sides' in str(error)
else:
    raise AssertionError('sample/numeric P concatenation should be rejected')
assert left & right == [(0, 5), 7, (2, 4, 5), (0, 7), 5, (2, 4, 7)]
assert namespace['P'][None] & namespace['P'][None] == [None]
assert namespace['P']['x', ' '] & namespace['P']['o', '-', ' '] == [
    ('x', 'o'), '-', 'x', 'o', ('x', '-'), ' ']
sampleChord = namespace['P']['x']
for symbol in 'abcdefg':
    sampleChord = sampleChord & namespace['P'][symbol]
assert sampleChord == [tuple('xabcdefg')]
try:
    sampleChord & namespace['P']['h']
except ValueError as error:
    assert 'at most 8 symbols' in str(error)
else:
    raise AssertionError('sample-symbol chord width must be capped at 8')
try:
    namespace['P'][*('x' * 16)] & namespace['P'][*('o' * 17)]
except ValueError as error:
    assert '256' in str(error)
else:
    raise AssertionError('sample-symbol P zipping must cap LCM expansion at 256')
try:
    namespace['P']['x'] & namespace['P'][1]
except TypeError as error:
    assert 'cannot mix numeric' in str(error)
else:
    raise AssertionError('sample-symbol and numeric P patterns must not zip together')
assert namespace['PAlt'](namespace['P']['x', ' '], namespace['P']['o', '-']) == [
    'x', 'o', ' ', '-']
assert namespace['PAlt'](
    namespace['P']['x', ' '], namespace['P']['o', '-', ' ']) == [
        'x', 'o', ' ', '-', 'x', ' ', ' ', 'o', 'x', '-', ' ', ' ']
assert namespace['PAlt'](
    namespace['P']['x'], namespace['P']['o'], namespace['P']['-']) == ['x', 'o', '-']
assert namespace['PStretch'](namespace['P']['x', ' '], 5) == [
    'x', ' ', 'x', ' ', 'x']
assert namespace['PStretch']('xo', 0) == []
assert namespace['PStutter'](namespace['P']['x', 'o']) == [
    'x', 'x', 'o', 'o']
assert namespace['PStutter'](
    namespace['P']['x', 'o'], namespace['P'][1, 3, 2]) == [
        'x', 'o', 'o', 'o', 'x', 'x', 'o', 'x', 'x', 'x', 'o', 'o']
assert namespace['PStutter'](
    namespace['P']['x', 'o', ' '], namespace['P'][1, 2]) == [
        'x', 'o', 'o', ' ', 'x', 'x', 'o', ' ', ' ']
shufSource = namespace['P']['x', 'o', 'x', ' ', '-']
shufBefore = list(shufSource)
shufResult = namespace['PShuf'](shufSource)
assert sorted(shufResult) == sorted(shufBefore) and list(shufSource) == shufBefore
assert list(shufResult) == [' ', 'x', '-', 'x', 'o']
shufSecondResult = namespace['PShuf'](shufSource)
assert list(shufSecondResult) == ['x', 'x', 'o', ' ', '-']
shufStringResult = namespace['PShuf']('x o-')
assert len(shufStringResult) == 4 and sorted(shufStringResult) == [' ', '-', 'o', 'x']
assert namespace['PJoin']([
    namespace['P']['x', ' '], namespace['P']['o', '-']]) == ['x', ' ', 'o', '-']
assert namespace['PJoin']([
    namespace['P']['x'], [], namespace['P']['o']]) == ['x', 'o']
assert namespace['PJoin'](namespace['P']['x', 'o']) == ['x', 'o']
assert len(namespace['PJoin'](['x' * 128, 'o' * 128])) == 256
assert namespace['PAlt'](namespace['P'][0, 2], namespace['P'][4, 6, 7]) == [
    0, 4, 2, 6, 0, 7, 2, 4, 0, 6, 2, 7]
assert namespace['PAlt'](
    namespace['P'][0, (2, 4), None], namespace['P'][7, 5, 3]) == [
        0, 7, (2, 4), 5, None, 3]
assert namespace['PStretch'](namespace['P'][0, (2, 4), None], 7) == [
    0, (2, 4), None, 0, (2, 4), None, 0]
assert namespace['PStutter'](
    namespace['P'][0, 2, 4], namespace['P'][1, 3]) == [
        0, 2, 2, 2, 4, 0, 0, 0, 2, 4, 4, 4]
numericShuffleSource = namespace['P'][0, (2, 4), None, 7, 2]
numericShuffleBefore = list(numericShuffleSource)
numericShuffle = namespace['PShuf'](numericShuffleSource)
assert sorted(map(repr, numericShuffle)) == sorted(map(repr, numericShuffleBefore))
assert list(numericShuffleSource) == numericShuffleBefore
assert namespace['PJoin']([
    namespace['P'][0, (2, 4)], namespace['P'][None, 7]]) == [
        0, (2, 4), None, 7]
assert len(namespace['PJoin']([
    namespace['P'][*range(128)], namespace['P'][*range(128, 256)] ])) == 256
for badJoin in (lambda: namespace['PJoin']([namespace['P'][0, 1], namespace['P']['x']]),
                lambda: namespace['PJoin'](['x' * 128, 'o' * 129]),
                lambda: namespace['PJoin']([[0] * 128, [1] * 129]),
                lambda: namespace['PJoin'](['x'] * 257),
                lambda: namespace['PJoin'](5)):
    try:
        badJoin()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('PJoin must reject unsupported or oversized sample patterns')
assert namespace['PBeat']('x xxx x') == [1.0, 0.5, 0.5, 1.0, 0.5]
assert namespace['PBeat']('x xx', 1, 0.25) == [0.25, 0.25, 0.5]
assert namespace['PBeat']('x xxx x', 6) == [1.0, 0.5, 0.5, 1.0, 0.5]
assert namespace['PBeat']('x xxx x', -1) == [0.5, 1.0, 0.5, 0.5, 1.0]
assert namespace['PBeat']('x x', dur=namespace['P'][0.25, 0.5]) == [0.5, 0.5]
assert namespace['PBeat']('') == [0.5]
assert namespace['PDur'](3, 8) == [0.75, 0.75, 0.5]
assert namespace['PDur'](5, 16) == [0.75, 0.75, 0.75, 0.75, 1.0]
assert namespace['PDur'](0, 8) == [2.0]
assert namespace['PDur'](3, 2) == [0.125, 0.125, 0.25]
assert namespace['PDur'](3, 8, 1, 0.25) == [0.75, 0.5, 0.75]
assert namespace['PDur'](3, 8, 4) == [0.75, 0.75, 0.5]
assert namespace['PDelay'](3, 8) == [0.0, 0.75, 1.5]
assert namespace['PDelay'](5, 16) == [0.0, 0.75, 1.5, 2.25, 3.0]
assert namespace['PDelay'](3, 8, 1, 0.25) == [0.0, 0.75, 1.25]
assert namespace['PStrum']() == [
    1.0, 0.5, 0.5, 0.5, 0.5, 0.5, 1.5, 0.5, 0.5, 0.5, 0.5, 1.0]
assert namespace['PStrum'](0) == [1.0, 0.5, 1.5, 1.0]
assert len(namespace['PStrum'](15)) == 34
assert [round(value, 6) for value in namespace['PQuicken']()] == [
    0.0, 0.166667, 0.333333, 0.388889, 0.444444, 0.462963,
    0.481481, 0.487654, 0.493827, 0.495885, 0.497942, 0.498628]
assert [round(value, 6) for value in namespace['PQuicken'](0.35, 2, 4)] == [
    0.0, 0.175, 0.2625, 0.30625]
assert len(namespace['PQuicken'](0.5, 17, 16)) == 256
assert namespace['PZip']([0, 1, 2], [3, 4]) == [
    (0, 3), (1, 4), (2, 3), (0, 4), (1, 3), (2, 4)]
assert namespace['PZip'](
    namespace['P'][0, 1], namespace['P'][3, 4],
    namespace['P'][5, 6, 7]) == [
        (0, 3, 5), (1, 4, 6), (0, 3, 7),
        (1, 4, 5), (0, 3, 6), (1, 4, 7)]
assert namespace['PZip'](2, [4, 5]) == [(2, 4), (2, 5)]
assert namespace['PZip']([None, 1], [3, 4]) == [
    (None, 3), (1, 4)]
assert namespace['PZip2']([0, 1, 2], [1, 2], rule=lambda a, b: a != b) == [
    (0, 1), (1, 2), (2, 1), (0, 2)]
assert namespace['PZip2']([0, 1], [3, 4]) == [(0, 3), (1, 4)]
assert namespace['PZip2']([1], [1], rule=lambda a, b: False) == []
assert namespace['PZip'](namespace['P']["x", " "],
                         namespace['P']["o", "-", " "]) == [
    ("x", "o"), "-", "x", "o", ("x", "-"), " "]
assert namespace['PZip'](
    namespace['PZip'](namespace['P']["x", "o"],
                      namespace['P']["-", " "]),
    namespace['P']["a"]) == [("x", "-", "a"), ("o", "a")]
assert namespace['PZip2'](
    namespace['P']["x", "o", "-"],
    namespace['P']["x", "-", " "],
    rule=lambda a, b: a != b) == [("o", "-"), "-"]
assert namespace['PZip'](
    [("x", "o", "a", "b")], [("c", "d", "e", "f")]) == [
        ("x", "o", "a", "b", "c", "d", "e", "f")]
assert namespace['PZip'](*[
    namespace['P'][symbol] for symbol in "abcdefgh"]) == [tuple("abcdefgh")]
assert namespace['PZip2'](
    namespace['P']["x"], namespace['P']["o"],
    rule=lambda a, b: False) == []
assert namespace['PRhythm']([2, (3, 8)]) == [2.0, (2.0, 0.75, 1.5)]
assert namespace['PRhythm']([(3, 8)]) == [0.75, 0.75, 0.5]
assert namespace['PRhythm']([2, (3, 8, 1, 0.25)]) == [
    2.0, (2.0, 0.75, 1.25)]
assert namespace['PRhythm'](namespace['P'][1, (3, 8)]) == [
    1.0, (2.0, 0.75, 1.5)]
assert namespace['PRhythm']([1, [0.25, 0.25]]) == [
    1.0, (0.5, 0.25)]
assert namespace['PRhythm']([0.75, [0.25, 0.25], (3, 8)]) == [
    0.75, (0.5, 0.25), (2.0, 0.75, 1.5)]
assert namespace['PRhythm']([0.5, [0.25, [0.125, 0.125]]]) == [
    0.5, (0.5, 0.25, 0.375)]
deepRhythm = [1]
for _ in range(9):
    deepRhythm = [deepRhythm]
for badRhythm in (
        lambda: namespace['PBeat'](123),
        lambda: namespace['PBeat']('x' * 257),
        lambda: namespace['PBeat']('x', dur=0),
        lambda: namespace['PBeat']('x', dur=namespace['P'][0.25, 0]),
        lambda: namespace['PDur'](3.0, 8),
        lambda: namespace['PDur'](1, 0),
        lambda: namespace['PDur'](-1, 8),
        lambda: namespace['PDur'](256, 129),
        lambda: namespace['PDur'](0, 256, dur=17),
        lambda: namespace['PDelay'](300, 128),
        lambda: namespace['PStrum'](-1),
        lambda: namespace['PStrum'](16),
        lambda: namespace['PStrum'](4.0),
        lambda: namespace['PQuicken'](0),
        lambda: namespace['PQuicken'](9),
        lambda: namespace['PQuicken'](0.5, 1, 4),
        lambda: namespace['PQuicken'](0.5, 17, 17),
        lambda: namespace['PQuicken'](0.5, 3.0, 4),
        lambda: namespace['PZip']([1]),
        lambda: namespace['PZip']([], [1]),
        lambda: namespace['PZip']([1] * 16, [2] * 17),
        lambda: namespace['PZip']([1] * 257, [2]),
        lambda: namespace['PZip']([1], 'xo'),
        lambda: namespace['PZip']([1], [tuple(range(9))]),
        lambda: namespace['PZip'](["x"] * 16, ["o"] * 17),
        lambda: namespace['PZip'](*[[symbol] for symbol in "abcdefghi"]),
        lambda: namespace['PZip']([("x", "o", "a", "b")], [("c", "d", "e", "f", "g")]),
        lambda: namespace['PZip']([1], namespace['P']["x"]),
        lambda: namespace['PZip'](["x", 1], ["o"]),
        lambda: namespace['PZip2']([1] * 16, [2] * 17),
        lambda: namespace['PZip2'](["x"] * 16, ["o"] * 17),
        lambda: namespace['PZip2']([1], [2], rule=0),
        lambda: namespace['PRhythm']([]),
        lambda: namespace['PRhythm']([0]),
        lambda: namespace['PRhythm']([True]),
        lambda: namespace['PRhythm']([1, (3,)]),
        lambda: namespace['PRhythm']([1, (3.0, 8)]),
        lambda: namespace['PRhythm']([1, (3, 64)]),
        lambda: namespace['PRhythm']([[9, 1]]),
        lambda: namespace['PRhythm']([[4096, 1]]),
        lambda: namespace['PRhythm']([[1] * 257]),
        lambda: namespace['PRhythm']([deepRhythm]),
        lambda: namespace['PRhythm']([1] * 257)):
    try:
        badRhythm()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('rhythm helpers must reject invalid or unbounded inputs')
assert namespace['PStretch'](namespace['P']['x', 'o'], 256) == (
    list(namespace['P']['x', 'o']) * 128)
assert len(namespace['PStutter'](['x'] * 16, 16)) == 256
for badCall in (
        lambda: namespace['PStretch'](namespace['P'][0, 'x'], 4),
        lambda: namespace['PStutter'](namespace['P'][0, 'x']),
        lambda: namespace['PShuf'](namespace['P'][0, 'x']),
        lambda: namespace['PStretch'](namespace['P']['x'], 257),
        lambda: namespace['PStretch']([0], 257),
        lambda: namespace['PShuf']('x' * 257),
        lambda: namespace['PShuf']([0] * 257),
        lambda: namespace['PStutter'](['x'] * 17, 16),
        lambda: namespace['PStutter']([0] * 17, 16),
        lambda: namespace['PStutter'](namespace['P']['x'], namespace['P'][1, 1.5]),
        lambda: namespace['PStutter'](namespace['P']['x'], [1] * 257),
        lambda: namespace['PStutter'](
            namespace['P'][*range(17)], namespace['P'][*([1] * 19)])):
    try:
        badCall()
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('PStretch/PStutter must reject invalid or oversized inputs')
for badPatterns in ((namespace['P'][0, 1], namespace['P']['x']),
                    (namespace['P']['x'], namespace['P'][1]),
                    (namespace['_FoxDotFinitePattern']([]), namespace['P']['x'])):
    try:
        namespace['PAlt'](*badPatterns)
    except (TypeError, ValueError) as error:
        assert ('patterns cannot be empty' in str(error) or
                'cannot mix numeric note patterns with sample-symbol patterns' in str(error))
    else:
        raise AssertionError('PAlt must reject empty or mixed-type patterns')
try:
    namespace['PAlt'](['x'] * 16, ['o'] * 17)
except ValueError as error:
    assert '256 expanded items' in str(error)
else:
    raise AssertionError('PAlt must enforce the 256-symbol cap')
try:
    namespace['PAlt']([0] * 16, [1] * 17)
except ValueError as error:
    assert '256 expanded items' in str(error)
else:
    raise AssertionError('numeric PAlt must enforce the 256-item cap')
try:
    namespace['PAlt']('x' * 257, namespace['P']['o'])
except ValueError as error:
    assert 'input patterns are limited to 256 items' in str(error)
else:
    raise AssertionError('PAlt must enforce the per-input limit')
try:
    namespace['P'][*range(129)] | namespace['P'][*range(129)]
except ValueError as error:
    assert '256' in str(error)
else:
    raise AssertionError('P concatenation must enforce its size cap')
try:
    ['x'] * 256 | namespace['P']['o']
except ValueError as error:
    assert '256 items' in str(error)
else:
    raise AssertionError('sample P concatenation must enforce its size cap')
try:
    namespace['P'][*range(17)] & namespace['P'][*range(19)]
except ValueError as error:
    assert '256' in str(error)
else:
    raise AssertionError('P zipping must enforce its expansion cap')
try:
    namespace['P'][(1, 2, 3, 4, 5),] & namespace['P'][(6, 7, 8, 9),]
except ValueError as error:
    assert '8 notes' in str(error)
else:
    raise AssertionError('zipped chords must stay within 8 notes')


try:
    namespace['P'][*range(17)] + namespace['P'][*range(19)]
except ValueError as error:
    assert '256 expanded items' in str(error)
else:
    raise AssertionError('expanded P patterns must stay within the event cap')
assert namespace['P'][2, None, (3, 4)] * 3 == [6, None, (9, 12)]
assert 2 * namespace['P'][1, None, (3, 4)] == [2, None, (6, 8)]
assert namespace['P'][8, 9] / namespace['P'][2, 3] == [4, 3]
assert 10 / namespace['P'][2, 4] == [5, 2.5]
assert namespace['P'][8, 9] // 2 == [4, 4]
assert namespace['P'][8, 9] % 3 == [2, 0]
assert namespace['P'][2, 3] ** 2 == [4, 9]
assert namespace['P'][2, 3] ^ 3 == [8, 27]
assert namespace['P'][8, 9] / namespace['P'][2, 0] == [4, 0]
assert namespace['P'][8, 9] % namespace['P'][3, 0] == [2, 0]
try:
    namespace['P'][2] ** 17
except ValueError as error:
    assert 'exponents are limited' in str(error)
else:
    raise AssertionError('P powers must enforce the exponent cap')

try:
    namespace['P'][(1, 2),] + namespace['P'][(3, 4, 5),]
except ValueError as error:
    assert 'matching sizes' in str(error)
else:
    raise AssertionError('mismatched chord groups must be rejected')
assert namespace['P'][slice(0, 8)] == list(range(8))
assert namespace['P'][slice(0, 8, 2)] == [0, 2, 4, 6]
assert namespace['P'][slice(8, 0, -2)] == [8, 6, 4, 2]
assert namespace['P'][slice(0, 8)][slice(2, 6)] + 1 == [3, 4, 5, 6]
try:
    namespace['P'][slice(0, None)]
except ValueError as error:
    assert 'finite stop' in str(error)
else:
    raise AssertionError('P generated ranges must require a finite stop')
try:
    namespace['P'][slice(0, 8, 0)]
except ValueError as error:
    assert 'cannot be zero' in str(error)
else:
    raise AssertionError('P generated ranges must reject a zero step')
try:
    namespace['P'][slice(0, 257)]
except ValueError as error:
    assert '256 generated items' in str(error)
else:
    raise AssertionError('P generated ranges must enforce the 256-item cap')
namespace['play'](namespace['transpose']([60, None, [62, 65]], 12), step=0.5)
assert len(namespace['_dexter_events']) == 3
namespace['_dexter_updates'].clear()
namespace['_dexter_default_touched'] = False
namespace['p1'] >> namespace['sequence']([60, None, 64], step=0.5, duration=0.4,
                                       synth='square', amp=0.6, pan=-0.25,
                                       cutoff=3200, echo=0.3, delay=0.2,
                                       reverb='gverb', room=0.8, mix=0.4,
                                       damp=0.25, revtime=4.0, spread=22)
update = namespace['_dexter_updates'][0]
assert update[0] is False and len(update[3]) == 2
assert update[4] == 3 and update[5] == 0.6 and update[6] == -0.25
assert abs(update[7] - 3200.0) < 1e-9 and abs(update[8] - 0.3) < 1e-9 and abs(update[9]) < 1e-9, repr(update[7:10])
assert abs(update[3][0][6] - 0.2) < 1e-9
assert update[19:25] == (3, 0.4, 0.8, 0.25, 4.0, 22.0)
assert update[10] is False and update[11] is False
assert update[12] == 0.01 and update[13] == 0.04
namespace['p2'] >> namespace['sequence']([0, 2, 4], step=1.0, degrees=True)
assert set(namespace['_dexter_updates']) == {0, 1}
assert namespace['_dexter_updates'][1][3][1][2] == 64
namespace['p1'].stop()
assert namespace['_dexter_updates'][0][0] is True

def run_script(script_text, include_events=False, include_effects=False, include_filters=False, include_decay=False, include_mutes=False, include_modulations=False, include_samples=False, include_sample_controls=False, include_player_effects=False, include_synth_controls=False, include_effect_control_vars=False, include_group_controls=False, include_tempo=False, user_synth_names=(), include_player_effect_values=False, include_effect_chains=False, include_gates=False):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        result_path = os.path.join(directory, 'result.out')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        wrapper = make_wrapper(source_path, result_path, user_synth_names)
        try:
            compile(wrapper, '<generated evaluation wrapper>', 'exec')
        except SyntaxError as error:
            raise SystemExit('Generated Python evaluation wrapper is invalid: ' + str(error))
        wrapper_globals = {'__name__': '__main__'}
        exec(wrapper, wrapper_globals, wrapper_globals)
        lines = open(result_path, encoding='utf-8').read().splitlines()
        assert (lines[0].startswith('OK\t') and
                0 <= int(lines[0].split('\t')[1]) <= 8), '\n'.join(lines[:20])
        player_rows = [line.split('\t') for line in lines if line.startswith('P\t')]
        event_rows = [line.split('\t') for line in lines if line.startswith('N\t')]
        effect_rows = [line.split('\t') for line in lines if line.startswith('R\t')]
        player_effect_rows = [line.split('\t') for line in lines if line.startswith('A\t')]
        player_effect_values = [line.split('\t') for line in lines if line.startswith('W\t')]
        synth_control_rows = [line.split('\t') for line in lines if line.startswith('S\t')]
        effect_control_var_rows = [line.split('\t') for line in lines if line.startswith('X\t')]
        gate_rows = [line.split('\t') for line in lines if line.startswith('C\t')]
        if include_gates:
            return player_rows, gate_rows
        if include_effect_chains:
            return player_rows, [line.split('\t') for line in lines if line.startswith('J\t')]
        filter_rows = [line.split('\t') for line in lines if line.startswith('F\t')]
        modulation_rows = [line.split('\t') for line in lines if line.startswith('G\t')]
        decay_rows = [line.split('\t') for line in lines if line.startswith('E\t')]
        mute_rows = [line.split('\t') for line in lines if line.startswith('M\t')]
        if include_tempo:
            return player_rows, [line.split('\t') for line in lines if line.startswith('T\t')]
        if include_player_effect_values:
            if include_player_effects:
                return player_rows, player_effect_rows, player_effect_values
            return player_rows, player_effect_values
        if include_group_controls:
            return (player_rows, effect_rows, filter_rows, decay_rows,
                    modulation_rows)
        if include_sample_controls:
            return (player_rows, [line.split('\t') for line in lines if line.startswith('B\t')],
                    [line.split('\t') for line in lines if line.startswith('Q\t')])
        if include_player_effects and include_synth_controls:
            if include_effect_control_vars:
                return (player_rows, player_effect_rows, synth_control_rows,
                        effect_control_var_rows)
            return player_rows, player_effect_rows, synth_control_rows
        if include_effect_control_vars:
            return player_rows, effect_control_var_rows
        if include_player_effects:
            return player_rows, player_effect_rows
        if include_synth_controls:
            return player_rows, synth_control_rows
        if include_samples and include_events:
            return (player_rows, [line.split('\t') for line in lines if line.startswith('B\t')],
                    event_rows)
        if include_samples:
            return player_rows, [line.split('\t') for line in lines if line.startswith('B\t')]
        if include_mutes:
            return player_rows, mute_rows
        if include_filters:
            return player_rows, filter_rows
        if include_modulations:
            return player_rows, modulation_rows
        if include_decay:
            return player_rows, decay_rows
        if include_effects:
            return player_rows, effect_rows
        return (player_rows, event_rows) if include_events else player_rows

def run_script_with_schedule_seed(script_text, seed, **options):
    global wrapper_cpp
    original = wrapper_cpp
    marker = 'int(time.monotonic_ns() & 0xffffffff)'
    if marker not in original:
        raise AssertionError('Could not find the runtime shuffle seed in wrapper')
    wrapper_cpp = original.replace(marker, str(int(seed)))
    try:
        options.setdefault('include_events', True)
        return run_script(script_text, **options)
    finally:
        wrapper_cpp = original

newSynthRows = run_script('p1 >> bass([0, 3, 5], dur=0.5)\np2 >> varsaw([0, 4], dur=1, rate=0.25)\np3 >> varsaw([0])\np4 >> dirt([0, 3])\np5 >> jbass([0, 5])\np6 >> noise([0, 2])\n')
assert len(newSynthRows) == 6
assert [int(row[6]) for row in newSynthRows] == [4, 5, 5, 6, 7, 8]
assert [float(row[21]) for row in newSynthRows] == [8.0, 0.25, 0.0, 0.0, 0.0, 0.0]
newFoxDotBatch = run_script('p1 >> charm([0], rate=0.5)\np2 >> spark([0])\np3 >> blip([0], rate=8)\n')
assert [int(row[6]) for row in newFoxDotBatch] == [22, 23, 24]
assert [float(row[21]) for row in newFoxDotBatch] == [0.5, 0.0, 8.0]
resonatorSynthRows = run_script('p1 >> bell([0], rate=1)\np2 >> gong([0])\np3 >> marimba([0])\np4 >> pluck([0])\np5 >> twang([0], rate=1)\np6 >> donk([0])\np7 >> squish([0], rate=1)\n')
assert len(resonatorSynthRows) == 7
assert [int(row[6]) for row in resonatorSynthRows] == [15, 16, 17, 18, 19, 20, 21]
assert [float(row[21]) for row in resonatorSynthRows] == [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0]

def run_error_script(script_text, user_synth_names=()):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        result_path = os.path.join(directory, 'result.out')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        wrapper_globals = {'__name__': '__main__'}
        exec(make_wrapper(source_path, result_path, user_synth_names),
             wrapper_globals, wrapper_globals)
        return open(result_path, encoding='utf-8').read()

def run_example(name, user_synth_names=(), include_player_effects=False,
                include_synth_controls=False, include_effect_control_vars=False,
                include_player_effect_values=False):
    example_path = os.path.join(sys.argv[2], name)
    return run_script(open(example_path, encoding='utf-8').read(),
                      user_synth_names=user_synth_names,
                      include_player_effects=include_player_effects,
                      include_player_effect_values=include_player_effect_values,
                      include_synth_controls=include_synth_controls,
                      include_effect_control_vars=include_effect_control_vars)

example185Rows = run_example('185-pattern-flatten-chords.py')
assert len(example185Rows) == 2
assert [int(row[6]) for row in example185Rows] == [18, 1]

sampleSetupPath = os.path.join(sys.argv[2], 'SAMPLES-SETUP.html')
sampleSetupHtml = open(sampleSetupPath, encoding='utf-8').read()
assert 'Portable/Samples' in sampleSetupHtml and 'properly licensed WAV' in sampleSetupHtml

userSynthBinaryPath = os.path.join(sys.argv[2], 'SynthDefs', 'CustomPluck.scsyndef')
userSynthBinary = open(userSynthBinaryPath, 'rb').read()
assert b'CustomPluck' in userSynthBinary
assert all(name in userSynthBinary for name in (b'brightness', b'sub'))
assert b'DexterPluck' not in userSynthBinary
assert not os.path.exists(os.path.join(sys.argv[2], 'SynthDefs', 'DexterPluck.scsyndef'))
userSynthControls = open(
    os.path.join(sys.argv[2], 'SynthDefs', 'CustomPluck.controls'),
    encoding='utf-8').read()
assert 'brightness 0.0 1.0 0.0' in userSynthControls
assert 'sub 0.0 0.5 0.0' in userSynthControls
userSynthRows = run_example('94-user-synthdef-loader.py', ('CustomPluck',))
assert [int(row[6]) for row in userSynthRows] == [58, 58]
customTonePath = os.path.join(sys.argv[2], 'SynthDefs', 'CustomTone.scsyndef')
customToneBinary = open(customTonePath, 'rb').read()
assert customToneBinary.startswith(b'SCgf')
assert all(name in customToneBinary
           for name in (b'CustomTone', b'blend', b'cutoff', b'resonance'))
customToneControls = open(
    os.path.join(sys.argv[2], 'SynthDefs', 'CustomTone.controls'),
    encoding='utf-8').read()
assert 'blend 0.0 1.0 0.25' in customToneControls
assert 'cutoff 100.0 12000.0 4000.0 log' in customToneControls
customToneRows = run_example(
    '202-custom-synth-live-controls.py', ('CustomTone',),
    include_synth_controls=True)
assert len(customToneRows) == 2
assert customToneRows[0][0][0] == 'P'
assert customToneRows[1][0][0] == 'S'
jazzKitRows = run_example('214-jazz-brush-kit.py', jazz_synth_names)
assert len(jazzKitRows) == 8
assert [int(row[1]) for row in jazzKitRows] == list(range(8))
jazzKitCoreRows = run_example('217-jazz-kit-core-groove.py',
                              jazz_synth_names)
jazzKitCompareRows = run_example('218-jazz-kit-kick-compare.py',
                                 jazz_synth_names)
assert len(jazzKitCoreRows) == 8 and len(jazzKitCompareRows) == 8
jazz_demo_sources = ''.join(
    open(os.path.join(sys.argv[2], filename), encoding='utf-8').read()
    for filename in ('214-jazz-brush-kit.py',
                     '217-jazz-kit-core-groove.py',
                     '218-jazz-kit-kick-compare.py'))
assert all('userSynth("' + name + '"' in jazz_demo_sources
           for name in jazz_synth_names)
assert '{"JazzKitBrushKick", "Brush kick"}' in catalog_source
missingUserSynth = run_error_script(
    'p1 >> userSynth("NotInstalled", [0, 2], dur=0.5)\n')
assert 'not a loaded Portable/SynthDefs entry' in missingUserSynth

def assert_player_polyphony_ok(player_row, event_rows):
    length = float(player_row[4])
    boundaries = []
    active = 0
    for event in event_rows:
        start = (float(event[1]) + float(event[7])) % length
        duration = float(event[2])
        full_cycles = int(duration // length)
        active += full_cycles
        remainder = duration - full_cycles * length
        if remainder <= 1e-9:
            continue
        end = start + remainder
        if end <= length:
            boundaries.extend(((start, 1), (end, -1)))
        else:
            boundaries.extend(((0.0, 1), (end - length, -1),
                               (start, 1), (length, -1)))
    assert active <= 8, ('player exceeds eight-note polyphony', active)
    for beat, delta in sorted(boundaries, key=lambda item: (item[0], item[1])):
        active += delta
        assert 0 <= active <= 8, ('player exceeds eight-note polyphony', beat, active)

initial_live_run = run_script('p1 >> saw([0, 2, 4], dur=0.5)\np2 >> pulse([0, None, 3], dur=0.25)\n')
cut_note_rows, cut_sample_buffers, cut_event_rows = run_script(
    'p1 >> saw(P[0, 2], dur=1, sus=0.8, cut=P[0.5, 1.0])\n'
    'p2 >> play("xo", dur=1, sus=0.9, cut=var([0.25, 0.75], 1))\n',
    include_events=True, include_samples=True)
assert [float(row[16]) for row in cut_event_rows] == [0.5, 1.0, 0.25, 0.75]
assert len(cut_sample_buffers) == 2
for bad_cut in ('-0.1', '1.01', 'True'):
    assert 'cut' in run_error_script(
        'p1 >> saw([0, 2], cut=' + bad_cut + ')').lower()
assert [row[1] for row in initial_live_run] == ['0', '1']
edit_one_player = run_script('p1 >> saw([0, 4, 2], dur=0.5)\n')
assert [row[1] for row in edit_one_player] == ['0']
stop_one_player = run_script('p2.stop()\n')
assert [row[1] for row in stop_one_player] == ['1'] and stop_one_player[0][2] == '1'

def run_compatibility_check(script_text, user_synth_names=()):
    with tempfile.TemporaryDirectory() as directory:
        source_path = os.path.join(directory, 'source.py')
        report_path = os.path.join(directory, 'report.txt')
        with open(source_path, 'w', encoding='utf-8') as target:
            target.write(script_text)
        namespace = {'_compat_source_path': source_path, '_compat_report_path': report_path,
                     '_USER_SYNTH_NAMES': tuple(user_synth_names)}
        exec(compile(compatibility_checker, '<embedded compatibility checker>', 'exec'), namespace, namespace)
        return open(report_path, encoding='utf-8').read()

flatten_checker_report = run_compatibility_check(
    'p1 >> pluck(P[0, PGroup(2, 4), 7].flatten(), dur=2, sus=1.8)\n')
assert 'No unsupported constructs detected' in flatten_checker_report, (
    flatten_checker_report)
editor_added_native_effects = run_compatibility_check(
    'p1 >> triangle([0, 4], amp=0.4, pan=-0.2, cutoff=4200, '
    'echo=0.35, room=0.7, shape=0.2, drive=0.15)\n')
assert 'No unsupported constructs detected' in editor_added_native_effects, (
    'Right-click native effect menu output must pass the compatibility checker.')
run_script('p1 >> triangle([0, 4], amp=0.4, pan=-0.2, cutoff=4200, '
           'echo=0.35, room=0.7, shape=0.2, drive=0.15)\n')
editor_added_loaded_effect = run_compatibility_check(
    'p1 >> saw([0, 2], effects=[("Chorus", {"fx_mix": 0.4})])\n',
    ('Chorus',))
assert 'No unsupported constructs detected' in editor_added_loaded_effect, (
    'Right-click loaded-effect menu output must pass the compatibility checker.')

assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw([0, 2], cut=P[1, 0.5])\n'
    'p2 >> play("xo", cut=var([0.2, 0.8], 2))\n')

coarse_formant_rows, coarse_formant_events = run_script(
    'p1 >> saw([0, 2, 4, 5], dur=1, coarse=P[0, 8, 4, 2])\n'
    'p2 >> pluck([0, 2, 4, 6], dur=1, formant=linvar([0, 7], [2, 2]))\n'
    'p3 >> play("x-o-", dur=1, coarse=var([2, 6], 1), formant=3)\n',
    include_events=True)
assert [float(row[17]) for row in coarse_formant_events[:4]] == [0, 8, 4, 2]
assert [float(row[18]) for row in coarse_formant_events[:4]] == [-1, -1, -1, -1]
assert [float(row[18]) for row in coarse_formant_events[4:8]] == [0, 3.5, 7, 3.5]
assert [float(row[17]) for row in coarse_formant_events[8:]] == [2, 6, 2, 6]
assert [float(row[18]) for row in coarse_formant_events[8:]] == [3, 3, 3, 3]
group_coarse_formant_rows, group_coarse_formant_events = run_script(
    'p1 >> saw([0, 2], dur=1)\n'
    'p2 >> pluck([3, 5], dur=1)\n'
    'g = Group(p1, p2)\n'
    'g.coarse = var([0, 4], 1)\n'
    'g.formant = var([0, 7], 1)\n',
    include_events=True)
assert [float(row[17]) for row in group_coarse_formant_events] == [0, 4, 0, 4]
assert [float(row[18]) for row in group_coarse_formant_events] == [0, 7, 0, 7]
for bad_coarse in ('-0.1', '32.1', 'True'):
    assert 'coarse' in run_error_script(
        'p1 >> saw([0], coarse=' + bad_coarse + ')').lower()
for bad_formant in ('-1.1', '7.1', 'True'):
    assert 'formant' in run_error_script(
        'p1 >> saw([0], formant=' + bad_formant + ')').lower()
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw([0, 2], coarse=var([0, 4], 1), formant=linvar([0, 7], [2, 2]))\n'
    'p2 >> play("x-o-", coarse=2, formant=4)\n')

def read_synthdef_manifest(data, validate_builtin_controls=True, return_controls=False):
    offset = 0

    def read(fmt):
        nonlocal offset
        values = struct.unpack_from(fmt, data, offset)
        offset += struct.calcsize(fmt)
        return values[0] if len(values) == 1 else values

    def read_string():
        nonlocal offset
        length = read('>B')
        value = data[offset:offset + length].decode('utf-8')
        offset += length
        return value

    assert data[:4] == b'SCgf'
    offset = 4
    version = read('>i')
    assert version in (1, 2)
    count_format = '>i' if version == 2 else '>H'
    index_format = '>i' if version == 2 else '>h'
    graph_count = read('>H')
    graphs = {}
    controls = {}
    for _ in range(graph_count):
        name = read_string()
        constant_count = read(count_format)
        offset += constant_count * 4
        parameter_count = read(count_format)
        offset += parameter_count * 4
        parameter_name_count = read(count_format)
        parameter_names = {}
        for _ in range(parameter_name_count):
            parameter_name = read_string()
            parameter_names[parameter_name] = read(count_format)
        if version == 2 and validate_builtin_controls:
            expected_controls = (
                'out freq rate sus unused4 unused5 wetbus wetgain wetcutoff trigger '
                'amp vib depth lforate lfowidth rq fmod blur cutoff atk rel beat_dur'
            ).split()
            if name == 'DexterSample':
                expected_controls.extend(('buf', 'start', 'end', 'loop'))
            assert parameter_names == {key: index for index, key in enumerate(expected_controls)}, (
                name, 'C++ control-number ABI mismatch', parameter_names)
        controls[name] = parameter_names
        ugen_count = read(count_format)
        ugens = []
        for _ in range(ugen_count):
            ugen_name = read_string()
            rate = read('>B')
            input_count = read(count_format)
            output_count = read(count_format)
            read('>h')
            for _ in range(input_count):
                source_index = read(index_format)
                read(index_format)
                assert source_index >= -1 and (source_index == -1 or source_index < len(ugens)), (
                    'SynthDef UGen input refers to an invalid or forward graph node: ' +
                    name + '/' + ugen_name)
            for _ in range(output_count):
                read('>B')
            ugens.append((ugen_name, input_count, rate))
        assert read('>H') == 0, 'unexpected SynthDef variants'
        graphs[name] = ugens
    assert offset == len(data), 'SynthDef parser did not consume the complete file'
    return (version, graphs, controls) if return_controls else (version, graphs)


assert not os.path.exists(os.path.join(
    sys.argv[2], 'SCPlugins', 'AntiAliasingOscillators.scx')), (
        'The unverified SC3.14.1/API-3 module must not be packaged with the 3.15 VST.')
assert not os.path.exists(os.path.join(
    sys.argv[2], 'SynthDefs', 'BlitB3SawProbe.scsyndef')), (
        'The probe SynthDef requires an unavailable UGen and must not block the VST validator.')


def test_embedded_synthdef():
    plugin_dir = os.path.dirname(os.path.abspath(sys.argv[1]))
    repo_root = os.path.abspath(os.path.join(plugin_dir, '..', '..'))
    cmake_text = open(os.path.join(plugin_dir, 'CMakeLists.txt'), encoding='utf-8').read()
    version = re.search(r'project\([^\n]*\bVERSION\s+([0-9.]+)', cmake_text).group(1)
    factory_text = processor_source.split('BEGIN_FACTORY_DEF', 1)[1]
    assert 'DEXTER_PLUGIN_VERSION' in factory_text, 'VST factory must use the generated CMake version'
    editor_text = read_cpp_source(os.path.join(plugin_dir, 'win32_code_editor.cpp'))
    assert 'DEXTER_PLUGIN_VERSION' in editor_text, 'Editor version must use the generated CMake version'
    version_template = open(
        os.path.join(plugin_dir, 'plugin_version.h.in'), encoding='utf-8').read()
    assert '@PROJECT_VERSION@' in version_template, (
        'Generated VST/panel version must match the CMake project version')
    assert "{'D', 'X', 'C', '3'}" in processor_source and 'superColliderSource_' in processor_source, (
        'Plugin state must persist the independent SuperCollider editor buffer.')
    assert 'scHostBridge_.start(' in processor_source and 'PerformOSCMessage(' in processor_source, (
        'Host-driven Worlds need worker-side NRT command dispatch, not an absent audio driver.')
    assert '!World_OpenTCP(' not in processor_source and 'World_OpenUDP(' not in processor_source, (
        'Native network dispatch requires an audio driver; do not use it for this NRT World.')
    assert 'findAvailableServerPort' in processor_source, (
        'Embedded SuperCollider control ports must be selected for loopback use.')
    assert 'SCLANG_SERVER=OFF' in open(
        os.path.join(repo_root, 'tools', 'build-sclang-runtime-windows.ps1'),
        encoding='utf-8').read(), (
            'The bundled sclang helper must be built without a second internal server.')
    sclang_packager = open(
        os.path.join(repo_root, 'tools', 'package-sclang-runtime.cmake'),
        encoding='utf-8').read()
    assert 'file(TO_CMAKE_PATH "${_dependency}" _dependency_path)' in sclang_packager, (
        'The sclang packager must normalize Windows paths before filtering system DLLs.')
    assert 'string(TOLOWER "${_dependency_name}" _dependency_name_lower)' in sclang_packager, (
        'The sclang packager must classify optional Windows imports case-insensitively.')
    assert 'if(_required_unresolved)' in sclang_packager, (
        'The sclang packager must still fail on dependencies outside the explicit OS allowlist.')
    sclang_workflow = open(
        os.path.join(repo_root, '.github', 'workflows', 'windows-vst3.yml'),
        encoding='utf-8').read()
    install_step = sclang_workflow.split(
        '- name: Install headless sclang into the VST3 resources', 1)[1].split(
            '\n      - name: ', 1)[0]
    assert '$LASTEXITCODE' not in install_step, (
        'A successful PowerShell installer must not be rejected by an unset native-command exit code.')
    for optional_windows_dll in (
            'AzureAttestManager', 'AzureAttestNormal', 'HvsiFileTrust',
            'PdmUtilities', 'WTDSENSOR', 'wpaxholder', 'wtdccm'):
        assert optional_windows_dll in sclang_packager, (
            'Missing Windows optional-component dependency filter: ' +
            optional_windows_dll)
    sclang_runtime = open(
        os.path.join(plugin_dir, 'sclang_runtime_win32.cpp'),
        encoding='utf-8').read()
    assert 'opts.reservedNumAudioBusChannels' in sclang_runtime, (
        'The sclang bus allocator must skip the plugin-owned private audio buses.')
    assert 's = Server.new(\\\\DexterVST, NetAddr' in sclang_runtime, (
        'The sclang bootstrap must retain the Server instance for the embedded VST World.')
    assert 'opts.protocol = \\\\udp;' in sclang_runtime and 's.startAliveThread;' in sclang_runtime, (
        'The sclang bootstrap must use the bounded host-driven loopback UDP bridge.')
    assert 'Server.remote' not in sclang_runtime, (
        'Do not assign Server.remote’s asynchronous TCP task as if it were the Server object.')
    assert 's.doWhenBooted' in sclang_runtime and 'DEXTER_SCLANG_READY' in sclang_runtime, (
        'The sclang bootstrap must acknowledge readiness only after the embedded server responds.')
    assert 'SuperCollider .scd' in editor_text and 'switchEditorTab(1)' in editor_text, (
        'The editor must expose a separate SuperCollider source tab.')
    assert 'host_->submitSuperColliderCode(source)' in editor_text, (
        'The .scd Run action must route to the headless sclang bridge.')
    assert 'Unrestricted SuperCollider code' in editor_text, (
        'The editor must warn before executing full SuperCollider language code.')
    generator = os.path.join(repo_root, 'spikes', 'direct-world', 'testdata', 'generate_tone.py')
    header = os.path.join(plugin_dir, 'tone_synthdef.h')
    with tempfile.TemporaryDirectory() as directory:
        generated = os.path.join(directory, 'tone.scsyndef')
        subprocess.run([sys.executable, generator, generated], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        generated_bytes = open(generated, 'rb').read()
        header_text = open(header, encoding='utf-8').read()
        embedded_bytes = bytes(int(value, 16) for value in
                               re.findall(r'0x([0-9a-fA-F]{2})', header_text))
        assert generated_bytes == embedded_bytes, 'embedded SynthDef differs from its generator'
    synthdef_version, graphs = read_synthdef_manifest(generated_bytes)
    assert synthdef_version == 1 and len(graphs) == 28
    core_header = open(os.path.join(plugin_dir, 'foxdot_synthdefs.h'), encoding='utf-8').read()
    core_bytes = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', core_header))
    core_version, core_graphs = read_synthdef_manifest(core_bytes)
    assert core_version == 2 and len(core_graphs) == 26
    sample_header = open(os.path.join(plugin_dir, 'sample_synthdef.h'), encoding='utf-8').read()
    sample_bytes = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', sample_header))
    sample_version, sample_graphs, sample_controls = read_synthdef_manifest(
        sample_bytes, return_controls=True)
    assert sample_version == 2 and set(sample_graphs) == {'DexterSample'}
    assert any(ugen[0] == 'PlayBuf' for ugen in sample_graphs['DexterSample'])
    assert any(ugen[0] == 'Phasor' for ugen in sample_graphs['DexterSample'])
    assert any(ugen[0] == 'BufRd' for ugen in sample_graphs['DexterSample'])
    assert sample_controls['DexterSample']['buf'] == 22
    assert [sample_controls['DexterSample'][name] for name in ('start', 'end', 'loop')] == [23, 24, 25]
    user_def_path = os.path.join(plugin_dir, 'Portable', 'SynthDefs', 'CustomPluck.scsyndef')
    user_version, user_graphs, user_controls = read_synthdef_manifest(
        open(user_def_path, 'rb').read(), validate_builtin_controls=False,
        return_controls=True)
    assert user_version == 2 and set(user_graphs) == {'CustomPluck'}
    assert user_controls['CustomPluck'] == {
        'out': 0, 'freq': 1, 'amp': 2, 'gate': 3, 'atk': 4, 'rel': 5,
        'brightness': 6, 'sub': 7}
    assert any(ugen[0] == 'Out' for ugen in user_graphs['CustomPluck'])
    assert any(ugen[0] == 'SinOsc' for ugen in user_graphs['CustomPluck'])
    assert any(ugen[0] == 'Saw' for ugen in user_graphs['CustomPluck'])
    assert any(ugen[0] == 'XFade2' for ugen in user_graphs['CustomPluck'])
    assert not set(graphs).intersection(core_graphs)
    for graph, ugens in core_graphs.items():
        assert any(name == 'EnvGen' for name, _, _ in ugens), graph
        assert not any(name in ('MdaPiano', 'Decimator', 'PlayBuf') for name, _, _ in ugens), graph
    graphs.update(core_graphs)
    graphs.update(sample_graphs)
    assert ('VarSaw', 3, 2) in graphs['DexterTone'], (
        'the base triangle must use the band-limited symmetric VarSaw')
    assert ('LFTri', 2, 2) not in graphs['DexterTone'], (
        'the base triangle must not use the non-band-limited LFTri')
    graph_names_match = re.search(
        r'static constexpr auto kSynthGraphNames = std::array\{(.*?)\};',
        processor_source, re.S)
    assert graph_names_match, 'could not find the embedded synth graph registry'
    registered_graphs = re.findall(r'"(Dexter[^"]+)"', graph_names_match.group(1))
    assert len(registered_graphs) == len(graphs)
    assert set(registered_graphs) == set(graphs), \
        'the C++ graph registry must match the embedded SynthDef bank'
    synth_names_match = re.search(
        r'_DEXTER_BUILTIN_SYNTHS\s*=\s*\((.*?)\)', api, re.S)
    assert synth_names_match, 'could not find the Python synth selector'
    synth_names = ast.literal_eval('(' + synth_names_match.group(1) + ')')
    assert len(synth_names) == len(registered_graphs) + 3, \
        'four base waveforms share DexterTone; every other player synth needs one graph'
    reserved_names_match = re.search(
        r'kEmbeddedFoxDotSynthNames\s*=\s*\{(.*?)\};', processor_source, re.S)
    assert reserved_names_match, 'could not find the C++ reserved synth-name list'
    reserved_names = tuple(re.findall(r'"([^"]+)"', reserved_names_match.group(1)))
    assert reserved_names == synth_names, \
        'C++ must reject every filename that collides with a built-in FoxDot synth'
    assert ('len(_DEXTER_BUILTIN_SYNTHS) +' in api and
            '_dexter_user_synth_names.index(pattern.synth)' in api)
    assert ('kVoiceSlotsPerPlayer =' in processor_source and
            'dexter::kMaxEventEffectTailVoicesPerPlayer' in processor_source)
    assert 'beginNoteEventEffectTail' in processor_source
    assert 'updateEventEffectTail' in processor_source
    assert 'kAudioBusChannelCount' in processor_source
    assert 'validFirstUserAudioBus(firstUserAudioBus)' in sclang_runtime_source
    assert 'kPlayerCount * kVoiceSlotsPerPlayer' in processor_source
    assert 'kAudioVoiceCount * kMaxCachedSynthGraphsPerVoice' in processor_source
    assert 'kPlayerCount * (kReverbGraphCount + 1) +' in processor_source
    assert ('kPlayerCount * kMaxPlayerEffectChain +' in processor_source and
            'kAudioVoiceCount * kMaxNoteEventEffectChain + 1' in processor_source)
    assert 'createNoteEventEffectGraph' in processor_source
    assert 'kEventEffectInputBusBase' in processor_source
    assert 'Graph_Calc(voice.eventEffectGraphs[stage])' in processor_source
    assert 'kMaxNoteEventEffectChain * 2 + stage * 2' in processor_source
    assert 'for (auto* graph : voice.eventEffectGraphs)' in processor_source
    assert 'eventEffectControlValues' in processor_source
    assert 'kAirwindowsInputBusBase' in processor_source
    assert 'playerEffectGraphs_[player]' in processor_source
    assert 'Graph_Calc(graph)' in processor_source
    assert 'kCalculatedWorldNodeCapacity < 2048 ? 2048 : kCalculatedWorldNodeCapacity' in processor_source
    assert 'static_assert(kWorldNodeCapacity >= 2048' in processor_source
    assert 'scPoolCapacity(kSynthGraphCount + kReverbGraphCount)' in processor_source
    assert 'scPoolCapacity(kCalculatedWorldNodeCapacity < 2048 ? 2048 : kCalculatedWorldNodeCapacity)' in processor_source
    assert 'prepareSynthGraphs(updates, updateMask, userSynthNames,' in processor_source
    assert 'static int synthOutputChannels(GraphDef* definition)' in processor_source
    assert 'mergeSynthOutputChannels' in processor_source
    assert 'needs consistent Out UGens with either one mono or two stereo channels' in processor_source
    assert 'kStereoSynthOutputBaseBus' in processor_source
    assert 'sourceRight = world_->mAudioBus' in processor_source
    assert 'stereoBalanceGains(voice.pan)' in processor_source
    assert 'Graph_New(world_, synthDefinitions_[graphIndex]' in processor_source
    assert 'name.size() > kSCNodeDefNameByteLen - 1' in processor_source, \
        'user SynthDef identifiers must allow SuperCollider names up to 255 bytes'
    assert 'std::array<int32, kSCNodeDefNameLen> nameKey{}' in processor_source
    assert 'World_GetGraphDef(world_, nameKey.data())' in processor_source
    assert 'bool startImmediately = false' in processor_source
    assert 'if (!startImmediately)\n            Node_SetRun(&created->mNode, 0)' in processor_source
    assert 'graphError, true);' in processor_source and 'createUserSynthGraph' in processor_source
    assert 'worldAccessMutex_, std::try_to_lock' in processor_source
    init_block = re.search(
        r'bool initializeWorld\([^)]*\)\s*\{(.*?)\n    void discardPartialWorld',
        processor_source, re.S)
    assert init_block, 'could not find the embedded engine initialization routine'
    assert 'Graph_New(world_, synthDefinitions_' not in init_block.group(1), \
        'synth voice graphs must not all be constructed during plugin startup'
    assert 'static_cast<std::ptrdiff_t>(kVoiceSlotsPerPlayer)' in processor_source, \
        'voice allocation must stay within the owning player partition'
    assert 'kSynthGraphCount + kReverbGraphCount' in processor_source
    assert 'kSynthGraphCount + 3> mapping{}' in processor_source
    assert 'index <= 3 ? 0 : index - 3' in processor_source
    assert 'kSynthIndexToGraphIndex.back() == kSynthGraphCount - 1' in processor_source
    assert 'kSynthIndexToGraphIndex.size()' in processor_source
    graph_selector_match = re.search(
        r'Graph\* graphForSynthIndex\(AudioVoice& voice, int synthIndex\)\s*\{(.*?)\n    \}',
        processor_source, re.S)
    assert graph_selector_match, 'could not find the synth-to-graph selector'
    assert 'synthIndex <= 24' not in graph_selector_match.group(1), \
        'the graph selector must not omit the last six embedded synths'
    synth_graph_mapping = [
        0 if index <= 3 else index - 3
        for index in range(len(registered_graphs) + 3)
    ]
    assert len(synth_graph_mapping) == len(synth_names)
    assert synth_graph_mapping[-6:] == list(range(len(registered_graphs) - 6,
                                                   len(registered_graphs)))
    assert ('LFSaw', 2, 2) in graphs['DexterDirt']
    assert ('LFNoise0', 1, 2) in graphs['DexterNoise']
    assert ('Vibrato', 9, 1) in graphs['DexterSoprano']
    assert ('Crackle', 1, 1) in graphs['DexterCrunch']
    assert ('Pulse', 2, 2) in graphs['DexterLazer']
    assert ('Gendy1', 10, 2) in graphs['DexterRave']
    assert ('Klank', 49, 2) in graphs['DexterBell']
    assert ('Klank', 49, 2) in graphs['DexterGong']
    assert ('Klank', 16, 2) in graphs['DexterMarimba']
    assert ('Trig1', 2, 2) in graphs['DexterBell']
    assert ('PinkNoise', 0, 2) in graphs['DexterMarimba']
    assert ('Decay', 2, 2) in graphs['DexterPluck']
    assert ('CombL', 4, 2) in graphs['DexterTwang']
    assert ('Ringz', 3, 2) in graphs['DexterDonk']
    assert ('LFNoise2', 1, 2) in graphs['DexterSpark']
    assert ('LFCub', 2, 2) in graphs['DexterBlip']
    assert ('Blip', 2, 2) in graphs['DexterBlip']
    assert ('Line', 4, 2) in graphs['DexterCharm']
    assert ('LFNoise0', 1, 2) in graphs['DexterScatter']
    assert ('LFPulse', 5, 2) in graphs['DexterOrient']
    assert ('Saw', 1, 2) in graphs['DexterZap']
    assert ('LFSaw', 4, 1) in graphs['DexterFuzz']
    assert ('Pulse', 2, 2) in graphs['DexterBug']
    assert ('SinOsc', 4, 2) in graphs['DexterRipple']
    for ugen_name in (b'\x06SinOsc', b'\x05LFTri', b'\x03Saw', b'\x05Pulse',
                      b'\x06MouseX', b'\x06MouseY', b'\x03LPF', b'\x03Lag',
                      b'DexterBass', b'DexterVarsaw', b'DexterDirt',
                      b'DexterJBass', b'DexterNoise', b'VarSaw',
                      b'DexterDub', b'DexterGrowl', b'DexterSoprano',
                      b'DexterCrunch', b'DexterLazer', b'DexterRave',
                      b'DexterBell', b'DexterGong', b'DexterMarimba',
                      b'DexterPluck', b'DexterTwang', b'DexterDonk',
                      b'DexterSquish', b'Klank', b'Trig1', b'PinkNoise',
                      b'Decay', b'CombL', b'Ringz',
                      b'LFSaw', b'LFNoise0', b'LFNoise2', b'LFCub', b'Blip',
                      b'Line', b'Vibrato', b'Crackle', b'Gendy1',
                      b'DexterCharm', b'DexterSpark', b'DexterBlip',
                      b'LFPulse', b'DexterScatter', b'DexterOrient',
                      b'DexterZap', b'DexterFuzz', b'DexterBug',
                      b'DexterRipple'):
        assert ugen_name in generated_bytes, 'missing SuperCollider oscillator/send UGen ' + repr(ugen_name)
    generator_text = open(generator, encoding='utf-8').read()
    assert 'ugen("VarSaw", 2, [(0, 1), (-1, 0), (-1, 3)], [2])' in generator_text
    assert 'ugen("LFSaw", 2, [(1, 0), (-1, 0)], [2])' in generator_text
    assert 'ugen("BinaryOpUGen", 2, [(11, 0), (10, 0)], [2], 0)' in generator_text
    assert 'ugen("BinaryOpUGen", 2, [(12, 0), (9, 0)], [2], 0)' in generator_text
    assert 'ugen("LFNoise0", 2, [(1, 0)], [2])' in generator_text
    assert 'ugen("Gendy1", 2,' in generator_text
    reverb_generator = os.path.join(repo_root, 'spikes', 'direct-world', 'testdata', 'generate_reverb.py')
    reverb_header = os.path.join(plugin_dir, 'reverb_synthdef.h')
    with tempfile.TemporaryDirectory() as directory:
        generated_reverbs = os.path.join(directory, 'reverb.scsyndef')
        subprocess.run([sys.executable, reverb_generator, generated_reverbs], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        generated_bytes = open(generated_reverbs, 'rb').read()
        header_text = open(reverb_header, encoding='utf-8').read()
        embedded_bytes = bytes(int(value, 16) for value in
                               re.findall(r'0x([0-9a-fA-F]{2})', header_text))
        assert generated_bytes == embedded_bytes, 'embedded reverb SynthDefs differ from their generator'
        for ugen_name in (b'FreeVerb', b'FreeVerb2', b'GVerb'):
            assert ugen_name in generated_bytes, 'missing SuperCollider reverb UGen ' + repr(ugen_name)

test_embedded_synthdef()

ported_synthdef_dir = os.path.join(repo_root, 'UGens', 'PortedPlugins', 'SynthDefs')
ported_wrappers = {
    'PortedAnalogBassDrum': ('AnalogBassDrum', 'accent tone decay attackfm selffm'),
    'PortedAnalogSnareDrum': ('AnalogSnareDrum', 'accent tone decay snappy'),
    'PortedBLOsc': ('BLOsc', 'pulsewidth waveform'),
    'PortedAnalogFoldOsc': ('AnalogFoldOsc', ''),
    'PortedNeoVarSawOsc': ('NeoVarSawOsc', 'pw waveshape'),
    'PortedVosimOsc': ('VosimOsc', 'form1freq form2freq shape'),
    'PortedZOsc': ('ZOsc', 'formantfreq shape mode'),
    'PortedStringVoice': ('StringVoice', 'accent structure brightness damping'),
}
portable_synthdef_dir = os.path.join(sys.argv[2], 'SynthDefs')
for wrapper_name, (ugen_name, sidecar_controls) in ported_wrappers.items():
    wrapper_path = os.path.join(ported_synthdef_dir, wrapper_name + '.scsyndef')
    assert os.path.isfile(wrapper_path), 'Missing playable wrapper ' + wrapper_name
    assert not os.path.exists(os.path.join(
        portable_synthdef_dir, wrapper_name + '.scsyndef')), (
            'Optional PortedPlugins wrapper must ship with its matching module, '
            'not inside the base VST bundle: ' + wrapper_name)
    wrapper_bytes = open(wrapper_path, 'rb').read()
    wrapper_version, wrapper_graphs, wrapper_controls = read_synthdef_manifest(
        wrapper_bytes, validate_builtin_controls=False, return_controls=True)
    assert wrapper_version in (1, 2) and set(wrapper_graphs) == {wrapper_name}, (
        wrapper_name, 'invalid SynthDef graph name or version')
    graph_ugens = {ugen[0] for ugen in wrapper_graphs[wrapper_name]}
    assert ugen_name in graph_ugens and 'Out' in graph_ugens and 'EnvGen' in graph_ugens, (
        wrapper_name, 'must use its module UGen with an envelope and Out')
    required_controls = {'out', 'freq', 'amp', 'gate', 'atk', 'rel'}
    required_controls.update(sidecar_controls.split())
    assert required_controls.issubset(wrapper_controls[wrapper_name]), (
        wrapper_name, 'wrapper is missing expected controls',
        required_controls - set(wrapper_controls[wrapper_name]))
    sidecar_path = os.path.join(ported_synthdef_dir, wrapper_name + '.controls')
    if sidecar_controls:
        assert os.path.isfile(sidecar_path), 'Missing live slider sidecar ' + wrapper_name
        sidecar_text = open(sidecar_path, encoding='utf-8').read()
        for control_name in sidecar_controls.split():
            assert re.search(r'^\s*' + re.escape(control_name) + r'\s+', sidecar_text,
                             re.MULTILINE), (wrapper_name, 'missing slider', control_name)
    else:
        assert not os.path.exists(sidecar_path), (
            wrapper_name, 'unexpected empty control sidecar')
ported_example_path = os.path.join(
    repo_root, 'UGens', 'PortedPlugins', 'Examples',
    '232-portedplugins-players.py')
ported_example_text = open(ported_example_path, encoding='utf-8').read()
ported_example_names = tuple(ported_wrappers)
ported_example_report = run_compatibility_check(
    ported_example_text, ported_example_names)
assert 'No unsupported constructs detected' in ported_example_report, (
    ported_example_report)
ported_player_rows = run_script(
    ported_example_text, user_synth_names=ported_example_names)
assert len(ported_player_rows) == 8, ported_player_rows

legacy = run_script('tempo(120)\nplay([60, 64], step=0.5, duration=0.4)\n')
assert len(legacy) == 1 and len(legacy[0]) == 24
namespace['tempo'](10000)
assert namespace['_dexter_bpm'] == 10000.0
namespace['tempo'](0.001)
assert namespace['_dexter_bpm'] == 0.001
for badTempo in (0, -1, 0.0009, float('inf'), float('-inf'), float('nan')):
    try:
        namespace['tempo'](badTempo)
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('tempo must reject out-of-range or non-finite values')
namespace['tempo'](120)
fox_literal_rows = run_script('p1 >> saw(P[0, 2, None, (3, 5), 7], dur=0.5, oct=5, sus=0.4)\n')
assert len(fox_literal_rows) == 1 and fox_literal_rows[0][5] == '5', fox_literal_rows
assert legacy[0][5] == '2' and legacy[0][6] == '0'
assert legacy[0][7] == '1.0' and legacy[0][8] == '0.0'
assert legacy[0][10] == '0.0' and legacy[0][11] == '0.0'
assert legacy[0][12] == '0' and legacy[0][13] == '0'
assert legacy[0][14] == '0.01' and legacy[0][15] == '0.04'
assert legacy[0][16] == '-' and legacy[0][17] == '-' and legacy[0][18] == '-'
assert legacy[0][19] == '-' and legacy[0][20] == '-'
example7 = run_example('07-independent-players.py')
assert len(example7) == 2 and all(len(row) == 24 for row in example7)
assert example7[0][5] == '6' and example7[1][5] == '2'
assert example7[0][6] == '0' and example7[1][6] == '0'
assert example7[0][10] == '0.0' and example7[0][11] == '0.0'
example9 = run_example('09-foxdot-core.py')
assert len(example9) == 2 and all(len(row) == 24 for row in example9)
foxSaw, foxPulse = example9
assert foxSaw[1] == '0' and foxSaw[3] == '96.0' and foxSaw[6] == '2'
assert abs(float(foxSaw[7]) - 0.7) < 1e-6 and abs(float(foxSaw[8]) + 0.35) < 1e-6
assert foxPulse[1] == '1' and foxPulse[3] == '96.0' and foxPulse[6] == '3'
assert abs(float(foxPulse[7]) - 0.25) < 1e-6 and abs(float(foxPulse[8]) - 0.35) < 1e-6
example10Rows = run_example('10-pattern-transformations.py')
assert len(example10Rows) == 2 and all(len(row) == 24 for row in example10Rows)
assert example10Rows[0][5] == '7' and example10Rows[0][6] == '2'
assert example10Rows[1][5] == '4' and example10Rows[1][6] == '3'
example11Rows = run_example('11-foxdot-pattern-literals.py')
assert len(example11Rows) == 2 and all(len(row) == 24 for row in example11Rows)
assert example11Rows[0][5] == '7' and example11Rows[1][5] == '2'
example12Rows = run_example('12-pattern-arithmetic.py')
assert len(example12Rows) == 2 and all(len(row) == 24 for row in example12Rows)
assert example12Rows[0][5] == '5' and example12Rows[1][5] == '5'
example13Rows = run_example('13-pattern-methods.py')
assert len(example13Rows) == 5 and all(len(row) == 24 for row in example13Rows)
assert [row[5] for row in example13Rows] == ['8', '5', '4', '4', '4']
example14Rows = run_example('14-pattern-composition.py')
assert len(example14Rows) == 2 and all(len(row) == 24 for row in example14Rows)
assert [row[5] for row in example14Rows] == ['5', '7']
example15Rows = run_example('15-pattern-arithmetic-operators.py')
assert len(example15Rows) == 4 and all(len(row) == 24 for row in example15Rows)
assert [row[5] for row in example15Rows] == ['4', '2', '4', '4']
example16Rows = run_example('16-bounded-pattern-slices.py')
assert len(example16Rows) == 3 and all(len(row) == 24 for row in example16Rows)
assert [row[5] for row in example16Rows] == ['8', '4', '2']
example17Rows = run_example('17-live-player-edits.py')
assert len(example17Rows) == 2 and [row[1] for row in example17Rows] == ['0', '1']
example18Rows = run_example('18-ui-mouse-controls.py')
assert len(example18Rows) == 2 and all(len(row) == 24 for row in example18Rows)
assert example18Rows[0][12] == '1' and example18Rows[0][13] == '0'
assert example18Rows[1][12] == '0' and example18Rows[1][13] == '1'
envelopeRows = run_example('19-envelope-controls.py')
assert len(envelopeRows) == 2 and all(len(row) == 24 for row in envelopeRows)
assert float(envelopeRows[0][14]) == 0.005 and float(envelopeRows[0][15]) == 0.2
assert float(envelopeRows[1][14]) == 0.04 and float(envelopeRows[1][15]) == 0.1
timevarRows = run_example('20-timevar-controls.py')
assert len(timevarRows) == 2 and all(len(row) == 24 for row in timevarRows)
assert timevarRows[0][16] == 'v:0.15,0.45:4.0,4.0'
assert timevarRows[1][17] == 'l:-0.8,0.8:8.0,8.0'
assert timevarRows[1][18] == 'l:400.0,5000.0:8.0,8.0'
limutRiserRows = run_script(
    'p1 >> saw([0, 2, 4], amp=riser(8, 0.05, 0.3), '
    'cutoff=riser(8, 300, 9000))\n')
assert len(limutRiserRows) == 1
assert limutRiserRows[0][16] == 'r:0.05,0.3:8.0'
assert limutRiserRows[0][18] == 'r:300.0,9000.0:8.0'
limutRiserFxRows, limutRiserFxTokens = run_script(
    'p1 >> saw([0, 2], effect="LimutPingPong", '
    'fx_mix=riser(8, 0.1, 0.8))\n',
    include_effect_control_vars=True, user_synth_names=('LimutPingPong',))
assert len(limutRiserFxRows) == 1 and any(
    'r:0.1,0.8:8.0' in row for row in limutRiserFxTokens)
limutRiserExample = run_example('209-limut-riser-automation.py')
assert len(limutRiserExample) == 2
assert limutRiserExample[0][16] == 'r:0.04,0.28:16.0'
assert limutRiserExample[0][18] == 'r:400.0,9000.0:16.0'
limutDropRows = run_script(
    'p1 >> saw([0, 2, 4], amp=drop(30, 2, high=0.3, low=0.02), '
    'cutoff=drop(30, 2, high=8000, low=500))\n')
assert len(limutDropRows) == 1
assert limutDropRows[0][16] == 'v:0.3,0.02:30.0,2.0'
assert limutDropRows[0][18] == 'v:8000.0,500.0:30.0,2.0'
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw([0, 2], amp=drop(30, 2), fx_mix=drop(16, 16))\n')
limutDropFxRows, limutDropFxTokens = run_script(
    'p1 >> saw([0, 2], effect="LimutPingPong", fx_mix=drop(16, 16))\n',
    include_effect_control_vars=True,
    user_synth_names=('LimutPingPong',))
assert len(limutDropFxRows) == 1 and any(
    'v:1.0,0.0:16.0,16.0' in row for row in limutDropFxTokens)
limutDropExample = run_example('210-limut-drop-patterns.py')
assert len(limutDropExample) == 2
assert limutDropExample[0][16] == 'v:0.3,0.02:30.0,2.0'
assert limutDropExample[0][18] == 'v:8000.0,500.0:30.0,2.0'
wowRows = run_script(
    'p1 >> saw([0, 2, 4], amp=wow(amount=0.18, center=0.32, period=4, seed=7), '
    'pan=wow(amount=0.65, seed=9))\n')
assert len(wowRows) == 1 and wowRows[0][16].startswith('w:')
assert wowRows[0][17].startswith('w:')
tgRows = run_script(
    'p1 >> play("x-o-", dur=0.25, amp=tg(0.125, high=0.7, low=0.08))\n')
assert len(tgRows) == 1 and tgRows[0][16] == 'v:0.7,0.08:0.125,0.125'
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw([0, 2], amp=wow(seed=1), pan=tg())\n')
wowEffectRows, wowEffectTokens = run_script(
    'p1 >> saw([0, 2], effect="LimutPingPong", '
    'fx_mix=wow(amount=0.2, center=0.5, seed=3))\n',
    include_effect_control_vars=True, user_synth_names=('LimutPingPong',))
assert len(wowEffectRows) == 1 and any(
    any(token.startswith('w:') for token in row) for row in wowEffectTokens)
limutWowTgExample = run_example('211-limut-wow-tg-patterns.py')
assert len(limutWowTgExample) == 2
assert limutWowTgExample[0][16].startswith('w:')
assert limutWowTgExample[1][16] == 'v:0.7,0.08:0.125,0.125'
unit_time_ns = namespace
unit_time_ns['Clock'].bpm = 120
assert unit_time_ns['beats'](0.25) == 0.25
assert unit_time_ns['seconds'](0.25) == 0.5
assert unit_time_ns['bars'](0.25) == 1.0
assert unit_time_ns['bars'](0.5, beats_per_bar=3) == 1.5
secondsPattern = unit_time_ns['seconds'](
    unit_time_ns['P'][0.25, 0.5], bpm=120)
assert list(secondsPattern) == [0.5, 1.0]
secondsRandom = unit_time_ns['seconds'](
    unit_time_ns['PWhite'](0.25, 0.5, length=16, seed=33), bpm=120)
assert len(secondsRandom) == 16 and all(0.5 <= value <= 1.0
                                       for value in secondsRandom)
secondsLinvar = unit_time_ns['seconds'](
    unit_time_ns['linvar']([0.25, 0.75], [4, 4]), bpm=120)
assert secondsLinvar.linear and secondsLinvar.values == [0.5, 1.5]
assert secondsLinvar.durations == [4.0, 4.0]
secondsWow = unit_time_ns['seconds'](
    unit_time_ns['wow'](amount=0.1, center=0.3), bpm=120)
assert all(abs(actual - expected) < 1e-9
           for actual, expected in zip(secondsWow.values, [0.4, 0.8]))
try:
    unit_time_ns['seconds'](0.25, bpm=unit_time_ns['var']([90, 120], [4, 4]))
except TypeError:
    pass
else:
    raise AssertionError('seconds must require a fixed conversion BPM')
unit_time_ns['Clock'].bpm = unit_time_ns['var']([90, 120], [4, 4])
try:
    unit_time_ns['seconds'](0.25)
except ValueError as error:
    assert 'bpm=' in str(error)
else:
    raise AssertionError('seconds without bpm must reject time-varying tempo')
assert unit_time_ns['seconds'](0.25, bpm=120) == 0.5
unit_time_ns['Clock'].bpm = 120
for bad_time in (True, float('inf'), 1e9):
    try:
        unit_time_ns['beats'](bad_time)
    except (TypeError, ValueError):
        pass
    else:
        raise AssertionError('unit helpers must reject invalid/unbounded values')
try:
    unit_time_ns['bars'](1, beats_per_bar=0)
except ValueError:
    pass
else:
    raise AssertionError('bars must reject a nonpositive meter length')
try:
    unit_time_ns['beats']([[0.25] * 128, [0.5] * 129])
except ValueError:
    pass
else:
    raise AssertionError('unit helpers must enforce the 256-item total cap')
unitTimeRows, unitTimeEvents = run_script(
    'Clock.bpm = 120\n'
    'p1 >> pluck([0, 2], dur=seconds(P[0.25, 0.5]), '
    'sus=seconds(linvar([0.2, 0.3], [4, 4])))\n',
    include_events=True)
assert len(unitTimeRows) == 1 and len(unitTimeEvents) == 2
assert abs(float(unitTimeEvents[0][2]) - 0.4) < 1e-9
assert abs(float(unitTimeEvents[1][1]) - 0.5) < 1e-9
assert abs(float(unitTimeEvents[1][2]) - 0.425) < 1e-9
unitTimeExample = run_example('212-unit-aware-time-patterns.py')
assert len(unitTimeExample) == 2
triangleClickExample = run_example('213-triangle-click-check.py')
assert len(triangleClickExample) == 2
assert 'No unsupported constructs detected' in run_compatibility_check(
    'Clock.bpm = 120\n'
    'p1 >> saw(PRand([0, 2], length=4, seed=1), '
    'dur=seconds(PWhite(0.25, 0.5, length=4, seed=2)), '
    'sus=seconds(linvar([0.2, 0.3], [4, 4])))\n'
    'p2 >> play("x-o-", dur=bars(P[0.25, 0.5]), '
    'pan=beats(var([-0.2, 0.2], [4, 4])))\n')
sardineSpanRows, sardineSpanEvents = run_script(
    'p1 >> saw([0, 2], dur=[0.5, 1.0], sus=[0.25, 0.75], '
    'span=2, until=3)\n',
    include_events=True)
assert len(sardineSpanRows) == 1 and len(sardineSpanEvents) == 2
assert abs(float(sardineSpanRows[0][4]) - 3.0) < 1e-9
assert int(sardineSpanRows[0][-1]) == 3
assert abs(float(sardineSpanEvents[0][1])) < 1e-9
assert abs(float(sardineSpanEvents[0][2]) - 0.5) < 1e-9
assert abs(float(sardineSpanEvents[1][1]) - 1.0) < 1e-9
assert abs(float(sardineSpanEvents[1][2]) - 1.5) < 1e-9
sardineSwingRows = run_script(
    'p1 >> saw([0, 2], span=1.5, until=4)\nClock.swing(0.1)\n')
assert int(sardineSwingRows[0][-1]) == 4
assert abs(float(sardineSwingRows[0][22]) - 0.1) < 1e-9
sardineSampleRows, _, sardineSampleEvents = run_script(
    'p1 >> play("x o", dur=0.25, span=0.5, until=8)\n',
    include_samples=True, include_events=True)
assert len(sardineSampleRows) == 1 and len(sardineSampleEvents) == 2, (
    repr(sardineSampleRows) + ' / ' + repr(sardineSampleEvents))
assert abs(float(sardineSampleRows[0][4]) - 0.5) < 1e-9
assert int(sardineSampleRows[0][-1]) == 8
assert abs(float(sardineSampleEvents[1][1]) - 0.25) < 1e-9
assert abs(float(sardineSampleEvents[1][2]) - 0.125) < 1e-9
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> pluck([0, 2, 4], dur=0.25, span=1.5, until=4)\n'
    'p2 >> play("x o", dur=0.25, span=0.5, until=8)\n')
sardineGateRows, sardineGateControls = run_script(
    'Seed.default = 77\n'
    'p1 >> saw([0, 2], chance="sometimes", loaf=4, on=(1, 3))\n',
    include_gates=True)
assert len(sardineGateRows) == 1 and len(sardineGateControls) == 1
assert float(sardineGateControls[0][2]) == 0.5
assert int(sardineGateControls[0][3]) == 77
assert int(sardineGateControls[0][4]) == 4
assert int(sardineGateControls[0][5]) == 5
alwaysGateRows, alwaysGateControls = run_script(
    'p1 >> play("x-o", chance="always", on=4)\n',
    include_gates=True)
assert float(alwaysGateControls[0][2]) == 1.0
assert int(alwaysGateControls[0][4]) == 4
assert int(alwaysGateControls[0][5]) == 1
neverGateRows, neverGateControls = run_script(
    'p1 >> saw([0], chance="never", loaf=2)\n',
    include_gates=True)
assert float(neverGateControls[0][2]) == 0.0
assert int(neverGateControls[0][4]) == 2
assert int(neverGateControls[0][5]) == 3
for invalid_gate, expected_gate_error in (
        ('p1 >> saw([0], chance=1.1)\n', 'chance probability must be from 0 to 1'),
        ('p1 >> saw([0], chance="sometimesish")\n', 'chance must be a Sardine preset'),
        ('p1 >> saw([0], loaf=33)\n', 'loaf must be from 1 to 32 bars'),
        ('p1 >> saw([0], loaf=4, on=5)\n', 'on bar positions must be within the loaf'),
        ('p1 >> saw([0], on=0)\n', 'on without loaf must be from 1 to 32 bars')):
    invalid_gate_output = run_error_script(invalid_gate)
    if 'ERROR' not in invalid_gate_output or expected_gate_error not in invalid_gate_output:
        raise AssertionError('Sardine chance/loaf/on must reject invalid inputs: ' +
                             invalid_gate.strip() + ' -> ' + invalid_gate_output)
assert 'No unsupported constructs detected' in run_compatibility_check(
    'Seed.default = 21\n'
    'p1 >> play("x-o-", chance="often", loaf=4, on=(1, 3))\n'
    'p2 >> pluck([0, 4, 7], chance=0.25, loaf=2, on=1)\n')
for sardine_gate_fragment in (
        'double chanceProbability = 1.0;',
        'std::uint32_t chanceSeed = 0;',
        "chanceRecord != 'C'",
        'sardineEventPasses(double probability',
        'pattern.chanceProbability',
        'pattern.barOnMask',
        "chance=None, loaf=None, on=None",
        "'chance', 'loaf', 'on'"):
    if sardine_gate_fragment not in (
            source + runtime_header_source + processor_source +
            sardine_gate_header_source):
        raise AssertionError('Sardine chance/loaf/on scheduling is missing: ' +
                             sardine_gate_fragment)
for jazz_lifecycle_fragment in (
        'hostManagedFreeControl',
        'Graph_SetControl(created, userDefinition.hostManagedFreeControl, 0.0f)',
        'synthControlIndex(loaded, "dexterAutoFree")'):
    if jazz_lifecycle_fragment not in processor_source:
        raise AssertionError('Jazz Kit graph lifecycle fix is missing: ' +
                             jazz_lifecycle_fragment)
if 'dexterAutoFree=2' not in open(
        os.path.join(os.path.abspath(sys.argv[2]),
                     'SynthDefs', 'JazzKit.sc'),
        encoding='utf-8').read():
    raise AssertionError('Jazz Kit must retain its standalone self-free default.')
sardineGateExample = run_example('216-sardine-chance-loaf-on.py')
assert len(sardineGateExample) == 2
sardineSwimGatesSource = open(os.path.join(
    os.path.abspath(sys.argv[2]),
    '221-sardine-swim-chance-bar-gates.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    sardineSwimGatesSource), run_compatibility_check(sardineSwimGatesSource)
sardineSwimGateRows, sardineSwimGateControls = run_script(
    sardineSwimGatesSource, include_gates=True)
assert [int(row[5]) for row in sardineSwimGateRows] == [16, 64]
assert [int(row[1]) for row in sardineSwimGateControls] == [0, 1]
assert [float(row[2]) for row in sardineSwimGateControls] == [0.5, 0.75]
assert [int(row[4]) for row in sardineSwimGateControls] == [4, 2]
assert [int(row[5]) for row in sardineSwimGateControls] == [5, 1]
assert all(0 <= int(row[3]) <= 0xffffffff
           for row in sardineSwimGateControls)
everySwimSource = open(os.path.join(
    os.path.abspath(sys.argv[2]),
    '222-sardine-swim-every-cycle.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    everySwimSource), run_compatibility_check(everySwimSource)
everySwimRows, everySwimEvents = run_script(
    everySwimSource, include_events=True)
assert len(everySwimRows) == 1 and int(everySwimRows[0][5]) == 64
assert len(everySwimEvents) == 64
everySwimPitches = [int(event[3]) for event in everySwimEvents]
assert everySwimPitches[0:4] == everySwimPitches[4:8]
assert everySwimPitches[12:16] != everySwimPitches[0:4]
variableSwimTiming = open(os.path.join(
    os.path.abspath(sys.argv[2]),
    '223-sardine-swim-variable-cycle-timing.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    variableSwimTiming), run_compatibility_check(variableSwimTiming)
variableTimingRows, variableTimingEvents = run_script(
    variableSwimTiming, include_events=True)
assert len(variableTimingRows) == 1 and len(variableTimingEvents) == 16
assert [float(event[1]) for event in variableTimingEvents[::2]] == [
    0.0, 0.5, 1.5, 1.75, 2.5, 3.0, 4.0, 4.25]
dieSwimExample = open(os.path.join(
    os.path.abspath(sys.argv[2]),
    '224-sardine-swim-die-stop.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    dieSwimExample), run_compatibility_check(dieSwimExample)
dieSwimRows, dieSwimEvents = run_script(
    dieSwimExample, include_events=True)
assert len(dieSwimRows) == 1 and len(dieSwimEvents) == 32
assert [float(event[1]) for event in dieSwimEvents[::2]] == [
    0.0, 0.5, 1.5, 1.75, 2.5, 3.0, 4.0, 4.25,
    5.0, 5.5, 6.5, 6.75, 7.5, 8.0, 9.0, 9.25]
sleepExample = open(os.path.join(
    os.path.abspath(sys.argv[2]),
    '225-sardine-swim-sleep-offsets.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    sleepExample), run_compatibility_check(sleepExample)
sleepRows, sleepEvents = run_script(sleepExample, include_events=True)
assert len(sleepRows) == 2 and len(sleepEvents) == 32
assert [float(event[1]) for event in sleepEvents[:16]] == [
    float(index) for index in range(16)]
assert [float(event[1]) for event in sleepEvents[16:]] == [
    index + 0.5 for index in range(16)]
patternSleepSource = (
    '@swim(cycles=4, period=1)\n'
    'def patterned_sleep(p=1, i=0):\n'
    '    p1 >> saw([0], dur=0.25)\n'
    '    sleep(P[1.5, 0.25])\n'
    '    p2 >> triangle([i], dur=0.25)\n'
    '    again(patterned_sleep, p=p, i=i+1)\n')
patternSleepReport = run_compatibility_check(patternSleepSource)
assert 'No unsupported constructs detected' in patternSleepReport, patternSleepReport
patternSleepRows, patternSleepEvents = run_script(
    patternSleepSource, include_events=True)
assert len(patternSleepRows) == 2 and len(patternSleepEvents) == 8
assert [float(event[1]) for event in patternSleepEvents[:4]] == [
    0.0, 1.0, 2.0, 3.0]
assert [float(event[1]) for event in patternSleepEvents[4:]] == [
    1.25, 1.5, 3.25, 3.5], (
        'Patterned oversleep must cross cycle boundaries and keep each '
        'player’s events in chronological order: ' + repr(patternSleepEvents))
sampleSleepSource = (
    '@swim(cycles=2, period=1)\n'
    'def sample_sleep(p=1, i=0):\n'
    '    sleep(P[1.5, 0.25])\n'
    '    p1 >> play("xo", sample=P[0, 1], dur=0.25)\n'
    '    again(sample_sleep, p=p, i=i+1)\n')
sampleSleepRows, sampleSleepRequests, sampleSleepEvents = run_script(
    sampleSleepSource, include_samples=True, include_events=True)
assert len(sampleSleepRows) == 1 and len(sampleSleepEvents) == 4
assert [float(event[1]) for event in sampleSleepEvents] == [
    1.25, 1.5, 1.5, 1.75]
assert [int(request[2]) for request in sampleSleepRequests] == [0, 0, 1, 1], (
    'Sorting overslept sample hits must keep each sample request paired with '
    'its event: ' + repr(sampleSleepRequests))
for invalid_sleep_source in (
        '@swim(cycles=1, period=1)\n'
        'def bad(p=1, i=0):\n'
        '    sleep(P[0.5, -0.25])\n'
        '    p1 >> saw([0])\n',
        '@swim(cycles=1, period=1)\n'
        'def bad(p=1, i=0):\n'
        '    sleep(P[4096.1])\n'
        '    p1 >> saw([0])\n',
        '@swim(cycles=1, period=1)\n'
        'def bad(p=1, i=0):\n'
        '    sleep(P[' + ', '.join(['0.25'] * 257) + '])\n'
        '    p1 >> saw([0])\n'):
    sleepReport = run_compatibility_check(invalid_sleep_source)
    assert '@swim sleep() requires a beat value or finite P/list/tuple' in \
        sleepReport, ('Invalid patterned sleep should be rejected before '
                      'execution: ' + sleepReport)
for invalid_sleep_runtime, expected_sleep_runtime_error in (
        ('@swim(cycles=2, period=1)\n'
         'def bad(p=1, i=0):\n'
         '    sleep(P[0.5, -0.25])\n'
         '    p1 >> saw([0])\n'
         '    again(bad, p=p, i=i+1)\n',
         'sleep duration must be from 0 to 4096 beats'),
        ('@swim(cycles=1, period=1)\n'
         'def bad(p=1, i=0):\n'
         '    sleep(4096)\n'
         '    sleep(1)\n'
         '    p1 >> saw([0])\n',
         'combined sleep offset may not exceed 4096 beats per cycle'),
        ('@swim(cycles=1, period=1)\n'
         'def bad(p=1, i=0):\n'
         '    sleep(4096)\n'
         '    p1 >> saw([0], dur=0.25)\n',
         'expanded timeline exceeds 4096 beats')):
    sleepError = run_error_script(invalid_sleep_runtime)
    assert expected_sleep_runtime_error in sleepError, (
        'Oversleep bounds must reject unsafe timing: ' + sleepError)
patternSleepExample = open(os.path.join(
    os.path.abspath(sys.argv[2]),
    '231-sardine-swim-patterned-oversleep.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternSleepExample), run_compatibility_check(patternSleepExample)
patternSleepExampleRows, patternSleepExampleEvents = run_script(
    patternSleepExample, include_events=True)
assert len(patternSleepExampleRows) == 2 and len(patternSleepExampleEvents) == 16
assert [float(event[1]) for event in patternSleepExampleEvents[8:16]] == [
    0.5, 2.25, 2.5, 4.25, 4.5, 6.25, 6.5, 8.25]
rampExample = open(os.path.join(
    os.path.abspath(sys.argv[2]),
    '226-sardine-finite-ramp-patterns.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    rampExample), run_compatibility_check(rampExample)
rampRows, rampEvents = run_script(rampExample, include_events=True)
assert len(rampRows) == 2 and len(rampEvents) == 22
rampPitches = [float(event[3]) for event in rampEvents[:16]]
assert all(right > left for left, right in zip(rampPitches, rampPitches[1:]))
assert [float(event[1]) for event in rampEvents[:16]] == [
    index * 0.25 for index in range(16)]
assert [round(float(event[1]), 8) for event in rampEvents[16:22]] == [
    0.0, 0.25, 0.55, 0.9, 1.3, 1.75]
repeatableSwimGate = (
    'Seed.default = 221\n'
    '@swim(cycles=4, period=1)\n'
    'def repeatable(p=1, i=0):\n'
    '    p1 >> bass([0], dur=1, oct=5, chance=0.5, loaf=4, on=(1, 3))\n'
    '    again(repeatable, p=p, i=i+1)\n')
assert run_script(repeatableSwimGate, include_gates=True) == \
       run_script(repeatableSwimGate, include_gates=True)
sardineSwimPath = os.path.join(
    os.path.abspath(sys.argv[2]), '219-sardine-swim-finite-cycles.py')
sardineSwimSource = open(sardineSwimPath, encoding='utf-8').read()
sardineSwimRows, sardineSwimSamples, sardineSwimEvents = run_script(
    sardineSwimSource, include_samples=True, include_events=True)
assert [int(row[5]) for row in sardineSwimRows] == [32, 32], (
    'Each @swim player should contain all eight expanded cycles: ' +
    repr(sardineSwimRows))
assert [int(row[-1]) for row in sardineSwimRows] == [1, 1], (
    '@swim output must play once, not loop the expanded arrangement.')
assert [float(row[4]) for row in sardineSwimRows] == [16.0, 16.0]
assert len(sardineSwimSamples) == 32 and len(sardineSwimEvents) == 64
assert [int(row[2]) for row in sardineSwimSamples[:16]] == [
    0, 0, 0, 0, 1, 1, 1, 1,
    0, 0, 0, 0, 1, 1, 1, 1]
assert 'No unsupported constructs detected' in run_compatibility_check(
    sardineSwimSource)
bassSwimEvents = sardineSwimEvents[32:]
assert len(bassSwimEvents) == 32
assert float(bassSwimEvents[0][1]) == 0.0
assert float(bassSwimEvents[4][1]) == 2.0
assert bassSwimEvents[0][3] != bassSwimEvents[4][3], (
    'The next @swim cycle should use its updated iteration value.')
variableSwimRows, variableSwimEvents = run_script(
    '@swim(cycles=3, period=1)\n'
    'def variable(p=1, i=0):\n'
    '    p1 >> saw([0], dur=1)\n'
    '    again(variable, p=p+0.5, i=i+1)\n',
    include_events=True)
assert len(variableSwimRows) == 1
assert [float(event[1]) for event in variableSwimEvents] == [0.0, 1.0, 2.5]
assert float(variableSwimRows[0][4]) == 4.5
assert 'No unsupported constructs detected' in run_compatibility_check(
    '@swim(cycles=4, period=2)\n'
    'def groove(p=2, i=0):\n'
    '    p1 >> saw(P[0, 2].rotate(i), dur=1)\n'
    '    again(groove, p=p, i=i+1)\n')
for invalid_swim, expected_swim_error in (
        ('@swim(cycles=33)\ndef groove(p=1, i=0):\n'
         '    p1 >> saw([0])\n', 'cycles must be a literal integer'),
        ('@swim\n'
         'def groove(p=1, i=0):\n'
         '    for n in range(2):\n'
         '        p1 >> saw([0])\n', 'Loops are not supported'),
        ('def groove(p=1, i=0):\n'
         '    again(groove)\n', 'again() is supported only')):
    swim_report = run_compatibility_check(invalid_swim)
    if expected_swim_error not in swim_report:
        raise AssertionError('The @swim compatibility checker must reject: ' +
                             invalid_swim + '\n' + swim_report)
for invalid_swim_runtime, expected_swim_runtime_error in (
        ('@swim(cycles=33)\n'
         'def groove(p=1, i=0):\n'
         '    p1 >> saw([0])\n', 'cycles must be from 1 to 32'),
        ('@swim(cycles=2, period=4096)\n'
         'def groove(p=4096, i=0):\n'
         '    p1 >> saw([0])\n'
         '    again(groove, p=4096, i=i+1)\n',
         'timeline is limited to 4096 beats'),
        ('@swim(cycles=2)\n'
         'def groove(p=1, i=0):\n'
         '    p1 >> saw([0])\n'
         '    p1 >> saw([2])\n'
         '    again(groove, p=p, i=i+1)\n',
          'only once per cycle'),
         ('@swim(cycles=2)\n'
          'def groove(p=1, i=0):\n'
          '    p1 >> saw([0], chance=1.1)\n'
          '    again(groove, p=p, i=i+1)\n',
          'chance probability must be from 0 to 1'),
         ('@swim(cycles=2)\n'
          'def groove(p=1, i=0):\n'
          '    p1 >> saw([0], loaf=33)\n'
          '    again(groove, p=p, i=i+1)\n',
          'loaf must be from 1 to 32 bars'),
         ('@swim(cycles=2)\n'
          'def groove(p=1, i=0):\n'
          '    p1 >> saw([0], loaf=4, on=(0, 3))\n'
          '    again(groove, p=p, i=i+1)\n',
          'on bar positions must be within the loaf')):
    swim_error = run_error_script(invalid_swim_runtime)
    if 'ERROR' not in swim_error or expected_swim_runtime_error not in swim_error:
        raise AssertionError('@swim runtime bounds must reject invalid input: ' +
                             invalid_swim_runtime + ' -> ' + swim_error)
maxSwimRows, maxSwimEvents = run_script(
    '@swim(cycles=32, period=1)\n'
    'def capped(p=1, i=0):\n'
    '    p1 >> saw(P[0, 1, 2, 3, 4, 5, 6, 7], dur=0.125)\n'
    '    again(capped, p=p, i=i+1)\n',
    include_events=True)
assert len(maxSwimRows) == 1 and len(maxSwimEvents) == 256
assert int(maxSwimRows[0][-1]) == 1
overLimitSwim = run_error_script(
    '@swim(cycles=32, period=1)\n'
    'def crowded(p=1, i=0):\n'
    '    p1 >> saw(P[0, 1, 2, 3, 4, 5, 6, 7, 0], dur=0.1)\n'
    '    again(crowded, p=p, i=i+1)\n')
assert '256 expanded events' in overLimitSwim
conditionalSwimRows, conditionalSwimEvents = run_script(
    '@swim(cycles=8, period=1)\n'
    'def fills(p=1, i=0):\n'
    '    if i % 4 == 3:\n'
    '        p1 >> saw([7], dur=1)\n'
    '    else:\n'
    '        p1 >> saw([0, 2], dur=0.5)\n'
    '    again(fills, p=p, i=i+1)\n',
    include_events=True)
assert len(conditionalSwimRows) == 1
assert int(conditionalSwimRows[0][5]) == 14
assert [float(event[3]) for event in conditionalSwimEvents] == [
    60, 64, 60, 64, 60, 64, 72, 60, 64, 60, 64, 60, 64, 72]
assert 'No unsupported constructs detected' in run_compatibility_check(
    '@swim(cycles=4, period=1)\n'
    'def fills(p=1, i=0):\n'
    '    if i % 4 == 3:\n'
    '        p1 >> saw([7], dur=1)\n'
    '    else:\n'
    '        p1 >> saw([0], dur=1)\n'
    '    again(fills, p=p, i=i+1)\n')
earlyStopRows, earlyStopEvents = run_script(
    '@swim(cycles=8, period=1)\n'
    'def ending(p=1, i=0):\n'
    '    if i < 2:\n'
    '        p1 >> saw([i], dur=1)\n'
    '        again(ending, p=p, i=i+1)\n'
    '    else:\n'
    '        p1 >> saw([7], dur=1)\n',
    include_events=True)
assert len(earlyStopRows) == 1 and int(earlyStopRows[0][5]) == 3
assert [float(event[3]) for event in earlyStopEvents] == [60, 62, 72]
for invalid_swim_condition in (
        'p > 2',
        'i ** 2 > 4',
        'i % 0 == 0',
        'abs(i) > 2'):
    invalid_conditional = (
        '@swim(cycles=2, period=1)\n'
        'def unsafe(p=1, i=0):\n'
        '    if ' + invalid_swim_condition + ':\n'
        '        p1 >> saw([0], dur=1)\n'
        '    else:\n'
        '        p1 >> saw([1], dur=1)\n')
    conditional_error = run_compatibility_check(invalid_conditional)
    if '@swim conditions may use only bounded integer expressions based on i' not in conditional_error:
        raise AssertionError('Unsafe @swim conditional should be rejected: ' +
                             invalid_swim_condition + '\n' + conditional_error)
if '@swim conditionals are limited to four nested levels' not in run_compatibility_check(
        '@swim(cycles=1, period=1)\n'
        'def deep(p=1, i=0):\n'
        '    if i > 0:\n'
        '        if i > 1:\n'
        '            if i > 2:\n'
        '                if i > 3:\n'
        '                    if i > 4:\n'
        '                        p1 >> saw([0], dur=1)\n'
        '    else:\n'
        '        p1 >> saw([1], dur=1)\n'):
    raise AssertionError('@swim must enforce its conditional nesting cap.')
assert len(run_example('214-jazz-brush-kit.py',
                       user_synth_names=jazz_synth_names)) == 8
for invalid_timing, expected_error in (
        ('p1 >> saw([0], span=0)\n', 'span must be from 0.125 to 8'),
        ('p1 >> saw([0], span=9)\n', 'span must be from 0.125 to 8'),
        ('p1 >> saw([0], span=True)\n', 'span must be a finite number'),
        ('p1 >> saw([0], until=0)\n', 'until must be from 1 to 256'),
        ('p1 >> saw([0], until=257)\n', 'until must be from 1 to 256'),
        ('p1 >> saw([0], until=1.5)\n', 'until must be a whole number'),
        ('p1 >> saw([0, 2, 4], span=8, until=256)\n',
         'span times until is limited to 4096 total beats')):
    invalid_output = run_error_script(invalid_timing)
    if 'ERROR' not in invalid_output or expected_error not in invalid_output:
        raise AssertionError('Sardine span/until must reject invalid values: ' +
                             invalid_timing.strip() + ' -> ' + invalid_output)
assert int(run_script('p1 >> saw([0])\n')[0][-1]) == 0
assert 'riser' in run_error_script('Clock.bpm = riser(8)\n')
assert 'riser' in run_error_script(
    'p1 >> saw([0], slide=riser(8))\n')
assert 'riser' in run_error_script('play("x", sample=riser(8))\n')
timevarNoteRows, timevarNoteEvents = run_script('Scale.default.set("minor")\np1 >> triangle(P[0, 1, 2, 3, 4, 5, 6, 0, 0, 1, 2, 3, 4, 5, 6, 0], dur=var([0.25, 0.5], [2, 2]), oct=var([4, 5], [2, 2]), sus=var([0.2, 0.4], [2, 2]), velocity=var([60, 110], [2, 2]))\n', include_events=True)
assert len(timevarNoteRows) == 1 and len(timevarNoteEvents) == 16
assert float(timevarNoteEvents[0][1]) == 0.0 and abs(float(timevarNoteEvents[0][2]) - 0.2) < 1e-9
assert int(timevarNoteEvents[0][3]) == 48 and int(timevarNoteEvents[0][4]) == 60
assert abs(float(timevarNoteEvents[0][5]) - 0.01) < 1e-9 and abs(float(timevarNoteEvents[0][6]) - 0.04) < 1e-9
assert abs(float(timevarNoteEvents[8][1]) - 2.0) < 1e-9
assert abs(float(timevarNoteEvents[8][2]) - 0.4) < 1e-9
assert int(timevarNoteEvents[8][3]) == 60 and int(timevarNoteEvents[8][4]) == 110
linNoteRows, linNoteEvents = run_script('Scale.default.set("minor")\np1 >> saw([0, 2, 4, 6, 0, 2, 4, 6], dur=linvar([0.25, 0.5], [1, 1]), oct=linvar([4, 5], [1, 1]), sus=linvar([0.1, 0.3], [1, 1]), velocity=linvar([55, 115], [1, 1]))\n', include_events=True)
assert len(linNoteRows) == 1 and len(linNoteEvents) == 8
assert abs(float(linNoteEvents[0][2]) - 0.1) < 1e-9 and int(linNoteEvents[0][3]) == 48
assert abs(float(linNoteEvents[1][1]) - 0.25) < 1e-9
assert abs(float(linNoteEvents[2][1]) - 0.5625) < 1e-9
assert abs(float(linNoteEvents[2][2]) - 0.2125) < 1e-9
assert int(linNoteEvents[2][3]) == 67 and int(linNoteEvents[2][4]) == 89
effectRows, effectEvents = run_script('p1 >> saw([0, 2, 4, 6], dur=0.5, echo=var([0.1, 0.5], [1, 1]), delay=linvar([0.0, 0.5], [1, 1]), delaytime=linvar([0.1, 0.5], [1, 1]), echomix=var([0.1, 0.5], [1, 1]), feedback=var([0.25, 0.45], [1, 1]), atk=var([0.01, 0.03], [1, 1]), rel=linvar([0.02, 0.1], [1, 1]))\n', include_events=True)
patternDelayRows, patternDelayEvents = run_script(
    'p1 >> saw([0, 2, 4, 5, 7], dur=0.5, delay=PDelay(3, 8))\n',
    include_events=True)
assert len(patternDelayRows) == 1 and len(patternDelayEvents) == 15
assert [round(float(event[7]), 6) for event in patternDelayEvents] == [
    0.0, 0.75, 1.5] * 5
chordDelayRows, chordDelayEvents = run_script(
    'p1 >> saw(P[(0, 2), (2, 4)], dur=1, sus=0.4, strum=0.05, delay=PDelay(2, 4))\n',
    include_events=True)
assert len(chordDelayRows) == 1 and [
    round(float(event[7]), 6) for event in chordDelayEvents
] == [0.0, 0.5, 0.05, 0.55, 0.0, 0.5, 0.05, 0.55]
assert [round(float(event[1]), 6) for event in chordDelayEvents] == [
    0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0]
tooManyDelayedEvents = run_error_script(
    'p1 >> saw([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11] * 7 + [0, 1], '
    'dur=1, sus=0.1, delay=PDelay(3, 8))\n')
assert 'PDelay is limited to 256 expanded note events per player' in tooManyDelayedEvents
foxdotEchoRows, foxdotDecayRows = run_script('p1 >> saw([0, 2, 4, 6], dur=0.5, echo=0.5, decay=0.75, echomix=0.4, feedback=0.5)\n', include_decay=True)
assert foxdotDecayRows[0][0] == 'E' and float(foxdotDecayRows[0][2]) == 0.75 and foxdotDecayRows[0][3] == '-'
assert abs(float(foxdotEchoRows[0][10]) - 0.5) < 1e-9
assert abs(float(foxdotDecayRows[0][4]) - 0.4) < 1e-9 and float(foxdotDecayRows[0][6]) == 0.5 and foxdotDecayRows[0][8] == '-'
_, varDecayRows = run_script('p1 >> saw([0, 2], echo=0.5, decay=var([0.5, 1.5], [1, 1]), delay=linvar([0.1, 0.4], [1, 1]))\n', include_decay=True)
assert varDecayRows[0][3] == 'v:0.5,1.5:1.0,1.0' and len(varDecayRows[0]) == 9
assert varDecayRows[0][8] == 'l:0.1,0.4:1.0,1.0'
assert len(effectRows) == 1 and len(effectRows[0]) == 24 and len(effectEvents) == 4
assert effectRows[0][19] == 'v:0.1,0.5:1.0,1.0'
assert effectRows[0][20] == 'l:0.1,0.5:1.0,1.0'
assert abs(float(effectEvents[0][7]) - 0.0) < 1e-9
assert abs(float(effectEvents[1][7])) < 1e-9
assert abs(float(effectEvents[2][7])) < 1e-9
assert abs(float(effectEvents[0][5]) - 0.01) < 1e-9 and abs(float(effectEvents[0][6]) - 0.02) < 1e-9
assert abs(float(effectEvents[1][5]) - 0.01) < 1e-9 and abs(float(effectEvents[1][6]) - 0.06) < 1e-9
assert abs(float(effectEvents[2][5]) - 0.03) < 1e-9 and abs(float(effectEvents[2][6]) - 0.1) < 1e-9
effectExample = run_example('22-timevar-effects-envelopes.py')
assert len(effectExample) == 2 and all(len(row) == 24 for row in effectExample)
reverbRows, reverbEffects = run_script(open(os.path.join(sys.argv[2], '23-reverb-algorithms.py'), encoding='utf-8').read(), include_effects=True)
assert len(reverbRows) == 3 and len(reverbEffects) == 3
assert [int(row[2]) for row in reverbEffects] == [1, 2, 3]
assert [float(row[4]) for row in reverbEffects] == [0.65, 0.75, 0.8]
assert [float(row[7]) for row in reverbEffects] == [15.0, 15.0, 25.0]
assert all(abs(float(row[3]) - expected) < 1e-6 for row, expected in zip(reverbEffects, [0.2, 0.3, 0.25]))
filterRows, filterControls = run_script('p1 >> saw([0, 2, 4, 6], lpf=linvar([300, 3000], [2, 2]), lpr=var([0.35, 0.7], [2, 2]), hpf=var([0, 150], [2, 2]), hpr=0.8, bpf=linvar([500, 1200], [2, 2]), bpr=0.5)\n', include_filters=True)
assert len(filterRows) == 1 and len(filterRows[0]) == 24
assert len(filterControls) == 1 and len(filterControls[0]) == 14
assert filterControls[0][0:2] == ['F', '0']
assert filterControls[0][8] == 'l:300.0,3000.0:2.0,2.0'
assert filterControls[0][9] == 'v:0.35,0.7:2.0,2.0'
assert filterControls[0][12] == 'l:500.0,1200.0:2.0,2.0'
filterExampleRows, filterExampleControls = run_script(open(os.path.join(sys.argv[2], '24-foxdot-filters.py'), encoding='utf-8').read(), include_filters=True)
legacyEchoExample = run_example('25-echo-decay-compat.py')
assert len(legacyEchoExample) == 1 and legacyEchoExample[0][1] == '0'
scaleChordRows, scaleChordEvents = run_script(open(os.path.join(sys.argv[2], '26-scales-and-chords.py'), encoding='utf-8').read(), include_events=True)
assert len(scaleChordRows) == 3 and [row[1] for row in scaleChordRows] == ['0', '1', '2']
assert [int(event[3]) for event in scaleChordEvents[:8]] == [60, 63, 67, 70, 67, 63, 62, 60]
assert [int(event[3]) for event in scaleChordEvents[8:14]] == [48, 51, 55, 50, 53, 57]
assert [int(event[3]) for event in scaleChordEvents[14:21]] == [60, 62, 63, 65, 67, 68, 70]
assert 'No unsupported constructs detected' in run_compatibility_check('Scale.default.set(Scale.melodicMinor)\np1 >> saw(P[(0, 2, 4), None], scale=[0, 2, 3, 5, 7, 8, 10])\n')
assert 'not available in the embedded scale set' in run_compatibility_check('Scale.notARealScale\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'Clock.bpm = 120\nClock.swing()\np1 >> triangle([0, 2], dur=0.5)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'Clock.clear()\nClock.bpm = 120\np1 >> pluck([0, 4], dur=0.5)\n')
assert 'Clock.clear accepts no arguments' in run_compatibility_check(
    'Clock.clear(1)\n')
assert 'Clock.clear is a method; call it, not assign to it' in run_compatibility_check(
    'Clock.clear = 1\n')
seed_source = (
    'Seed.default = 5\n'
    'p1 >> pluck(PRand([0, 2, 4, 7], length=16), dur=0.25, oct=4)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    seed_source)
seed_rows_a, seed_events_a = run_script(seed_source, include_events=True)
seed_rows_b, seed_events_b = run_script(seed_source, include_events=True)
assert len(seed_rows_a) == len(seed_rows_b) == 1
assert [event[3] for event in seed_events_a] == [
    event[3] for event in seed_events_b]
assert seed_events_a
assert 'Seed.default must be a whole number or None' in run_error_script(
    'Seed.default = True\np1 >> sine([0])\n')
assert 'Seed.default must be from 0 to 4294967295 or None' in run_error_script(
    'Seed.default = 4294967296\np1 >> sine([0])\n')
assert 'Seed only supports Seed.default' in run_compatibility_check(
    'Seed.unsupported\n')
assert 'Clock.swing accepts one optional amount' in run_compatibility_check(
    'Clock.swing(0.1, 0.2)\n')
assert 'Clock.swing accepts only the amount keyword' in run_compatibility_check(
    'Clock.swing(foo=0.1)\n')
assert 'Clock.swing is a method; call it, not assign to it' in run_compatibility_check(
    'Clock.swing = 0.1\n')
for tempo_function, token in (('var', 'v'), ('linvar', 'l')):
    tempo_source = ('Clock.bpm = ' + tempo_function + '([80, 120], [8, 4])\n'
                    'p1 >> sine([0, 2], dur=0.5)\n'
                    'p2 >> play("x-o-", dur=0.5)\n')
    assert 'No unsupported constructs detected' in run_compatibility_check(tempo_source)
    tempo_rows, tempo_controls = run_script(tempo_source, include_tempo=True)
    assert len(tempo_rows) == 2
    assert all(float(row[3]) == 80.0 for row in tempo_rows)
    assert [row[2] for row in tempo_controls] == [
        token + ':80.0,120.0:8.0,4.0'] * 2
# The final global setting applies even when it appears below the players.
tempo_rows, tempo_controls = run_script(
    'p1 >> sine([0])\nClock.bpm = var([90, 135], 8)\n', include_tempo=True)
assert float(tempo_rows[0][3]) == 90.0
assert tempo_controls[0][2] == 'v:90.0,135.0:8.0,8.0'
tempo_rows, tempo_controls = run_script(
    'Clock.bpm = var([90, 135], 8)\nClock.bpm = 110\np1 >> sine([0])\n',
    include_tempo=True)
assert float(tempo_rows[0][3]) == 110.0 and tempo_controls[0][2] == '-'
for invalid_tempo in ('var([0, 120], 4)', 'linvar([-1, 120], 4)',
                      'var([120, float("inf")], 4)', '0', '-10'):
    assert 'tempo must be finite and at least 0.001 BPM' in run_error_script(
        'Clock.bpm = ' + invalid_tempo + '\np1 >> sine([0])\n')
assert '0.49 beats or more' in run_error_script(
    'Clock.bpm = var([80, 400], 8)\nClock.swing(0.2)\np1 >> sine([0])\n')
assert '0.49 beats or more' in run_error_script(
    'Clock.swing(0.1)\nClock.bpm = linvar([80, 400], 8)\np1 >> sine([0])\n')
swingRows = run_script('Clock.bpm = 120\nClock.swing()\np1 >> triangle([0, 2], dur=0.5)\n')
assert len(swingRows[0]) == 24 and abs(float(swingRows[0][22]) - 0.1) < 1e-9
swingAfterPlayerRows = run_script(
    'p1 >> triangle([0, 2], dur=0.5)\nClock.swing(0.05)\n')
assert abs(float(swingAfterPlayerRows[0][22]) - 0.05) < 1e-9
assert abs(float(run_script(
    'Clock.swing(0.1)\nClock.swing(0)\np1 >> triangle([0], dur=1)\n')[0][22])) < 1e-9
assert '0.49 beats or more' in run_error_script(
    'Clock.bpm = 200\nClock.swing(0.1)\np1 >> triangle([0], dur=1)\n')
assert '0.49 beats or more' in run_error_script(
    'Clock.swing(0.1)\nClock.bpm = 400\np1 >> triangle([0], dur=1)\n')
assert 'must be finite' in run_error_script(
    "Clock.swing(float('nan'))\np1 >> triangle([0], dur=1)\n")
renardoClearRows = run_script(
    'p1 >> sine([0, 2], dur=0.5)\np2 >> play("x-o-", dur=0.5)\n'
    'Clock.clear()\n')
assert len(renardoClearRows) == 8 and all(
    row[2] == '1' for row in renardoClearRows), renardoClearRows
rootRows, rootEvents = run_script(open(os.path.join(sys.argv[2], '27-root-and-scale-controls.py'), encoding='utf-8').read(), include_events=True)
assert len(rootRows) == 3 and [row[1] for row in rootRows] == ['0', '1', '2']
assert [int(event[3]) for event in rootEvents] == [62, 65, 69, 72, 55, 58, 62, 57, 60, 63, 63, 67, 70, 74]
assert 'No unsupported constructs detected' in run_compatibility_check('Scale.default = "minor"\nRoot.default.set("Eb")\np1 >> saw([0, 2, 4], root=7)\n')
assert 'Root.default only supports .set(root)' in run_compatibility_check('Root.default.notes\n')
rootTimevarText = (
    'Scale.default = Scale.major\n'
    'Root.default.set(var([0, 7], [2, 2]))\n'
    'Scale.default.set(Scale.major)\n'
    'p1 >> triangle(P[0, 2, 4, 6], dur=1, oct=5)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(rootTimevarText)
rootTimevarRows, rootTimevarEvents = run_script(
    rootTimevarText, include_events=True)
assert len(rootTimevarRows) == 1
assert [int(event[3]) for event in rootTimevarEvents] == [
    60, 64, 74, 78]
rootLinvarRows, rootLinvarEvents = run_script(
    'Root.default.set(linvar([0, 4], [4, 4]))\n'
    'p1 >> triangle(P[0, 0, 0, 0], dur=1, oct=5)\n',
    include_events=True)
assert len(rootLinvarRows) == 1
assert [int(event[3]) for event in rootLinvarEvents] == [60, 61, 62, 63]
rootOverrideRows, rootOverrideEvents = run_script(
    'Root.default.set(var([0, 7], [2, 2]))\n'
    'p1 >> triangle(P[0, 0], dur=1, oct=5, root=2)\n'
    'p2 >> triangle(P[0, 0, 0, 0], dur=1, oct=5)\n',
    include_events=True)
assert len(rootOverrideRows) == 2
assert [int(event[3]) for event in rootOverrideEvents[:2]] == [62, 62]
assert [int(event[3]) for event in rootOverrideEvents[2:]] == [60, 60, 67, 67]
assert 'pitch classes 0..11 or MIDI notes 12..127' in run_error_script(
    'Root.default.set(var([0, 60], [2, 2]))\n'
    'p1 >> triangle([0], dur=1)\n')
assert 'Root.default.set requires' in run_error_script(
    'Root.default.set(None)\np1 >> triangle([0], dur=1)\n')
chordRows, chordEvents = run_script(open(os.path.join(sys.argv[2], '28-roman-numeral-chords.py'), encoding='utf-8').read(), include_events=True)
assert len(chordRows) == 2 and [row[1] for row in chordRows] == ['0', '1']
assert [int(event[3]) for event in chordEvents[:14]] == [48, 52, 55, 53, 57, 48, 55, 47, 50, 65, 48, 52, 55, 59]
assert [int(event[3]) for event in chordEvents[14:]] == [69, 72, 76, 81]
assert 'No unsupported constructs detected' in run_compatibility_check('p1 >> saw(P[I, IV, V7, I7])\np2 >> saw(PGroup(0, 2, 4))\n')
strumRows, strumEvents = run_script(open(os.path.join(sys.argv[2], '29-strummed-chords.py'), encoding='utf-8').read(), include_events=True)
assert len(strumRows) == 2 and [row[1] for row in strumRows] == ['0', '1']
assert [round(float(event[7]), 6) for event in strumEvents[:14]] == [0.0, 0.04, 0.08, 0.0, 0.04, 0.08, 0.12, 0.0, 0.04, 0.08, 0.0, 0.04, 0.08, 0.12]
assert [round(float(event[7]), 6) for event in strumEvents[14:]] == [0.0, 0.02, 0.04, 0.06, 0.0, 0.05, 0.1, 0.15, 0.0, 0.02, 0.04, 0.06, 0.0, 0.05, 0.1, 0.15]
assert all(abs(float(event[2]) - 1.75) < 1e-9 for event in strumEvents)
assert_player_polyphony_ok(strumRows[0], strumEvents[:14])
assert_player_polyphony_ok(strumRows[1], strumEvents[14:])
overlapStrumRows, overlapStrumEvents = run_script(
    'Clock.bpm = 70\nScale.default = Scale.major\nRoot.default.set("C")\n'
    'p1 >> triangle(P[I, V7, IV, I7], dur=2, sus=1.8, oct=2, strum=0.5, amp=0.32)\n'
    'p2 >> saw(P[V7, I7, IV7, V7], dur=2, sus=1.8, oct=3, '
    'strum=var([0.02, 0.15], 2), amp=0.18, pan=0.25)\n',
    include_events=True)
assert len(overlapStrumRows) == 2 and len(overlapStrumEvents) == 30
assert_player_polyphony_ok(overlapStrumRows[0], overlapStrumEvents[:14])
assert_player_polyphony_ok(overlapStrumRows[1], overlapStrumEvents[14:])
parse_start = source.find('bool parseResult(')
parse_end = source.find('\n}', parse_start)
parse_source = source[parse_start:parse_end]
assert "header.back() == '\\r'" in parse_source, 'Windows CRLF error headers must reach the Python traceback branch'
assert 'update.synthRate' in parse_source and 'kEmbeddedSynthCount + userSynthCount' in parse_source
assert 'update.bpnoise' in source and 'update.swell' in source
processor_source = read_cpp_source(
    os.path.join(os.path.dirname(sys.argv[1]), 'processor.cpp'))
native_effect_source = read_cpp_source(
    os.path.join(os.path.dirname(sys.argv[1]), 'native_player_effects.h'))
assert 'advanceNoise1' in processor_source and 'swellEnvelopeAt' in processor_source
assert 'processFoxDotShape' in processor_source and 'processFoxDotDrive' in processor_source
assert 'shapeEffectSmoothed_' in processor_source and 'driveEffectSmoothed_' in processor_source
assert 'processFoxDotShape' in native_effect_source and 'processFoxDotDrive' in native_effect_source
assert 'kReverbSendReleaseSeconds' in processor_source and 'beginReverbTail' in processor_source
assert '(voice.midiNote < 0 && !voice.releasing)' in processor_source
assert 'struct ActiveMidiGate' in processor_source and 'activeMidiGates_' in processor_source
assert 'event.noteOn.noteId = noteId' in processor_source
assert 'event.noteOff.noteId = noteId' in processor_source
assert 'foxDotEnvelopeDurationSeconds' in processor_source and 'safeSynthFrequency' in processor_source
assert 'synthIndex == 5 || synthIndex == 22' in processor_source
assert 'No unsupported constructs detected' in run_compatibility_check('p1 >> saw(P[I, V7], strum=0.06)\np2 >> saw(P[I7, V7], strum=var([0.02, 0.05], 2))\np1.mute()\np1.unmute()\n')
muteRows, muteCommands = run_script('p1.mute()\np2.unmute()\n', include_mutes=True)
assert muteRows == [] and muteCommands == [['M', '0', '1'], ['M', '1', '0']]
namespace['_dexter_events'] = []
namespace['_dexter_length'] = 1.0
namespace['_dexter_slots'] = 0
namespace['play']([(60, 64), (64, 67)], step=1, duration=1, strum=0.2)
assert len(namespace['_dexter_events']) == 4, 'same-pitch sustained overlaps are valid voices'
namespace['_dexter_events'] = []
namespace['_dexter_length'] = 0.0
namespace['_dexter_slots'] = 0
try:
    namespace['play']([(60, 61, 62, 63, 64), (65, 66, 67, 68)],
                      step=1, duration=1.5)
except ValueError as error:
    assert 'pattern exceeds 8-note polyphony' in str(error)
else:
    raise AssertionError('the eight-voice safety limit must remain enforced')
assert len(filterExampleRows) == 3 and [row[1] for row in filterExampleRows] == ['0', '1', '2']
assert len(filterExampleControls) == 3 and all(len(row) == 14 for row in filterExampleControls)
assert [row[1] for row in filterExampleControls] == ['0', '1', '2']
assert [row[8] for row in filterExampleControls] == ['l:300.0,1800.0:4.0,4.0', '-', '-']
assert [row[10] for row in filterExampleControls] == ['-', 'v:150.0,900.0:4.0,4.0', '-']
assert [row[12] for row in filterExampleControls] == ['-', '-', 'l:350.0,1200.0:4.0,4.0']
example30Rows = run_example('30-player-mute-controls.py')
assert len(example30Rows) == 2 and [row[1] for row in example30Rows] == ['0', '1']
example30Muted = open(os.path.join(sys.argv[2], '30-player-mute-controls.py'), encoding='utf-8').read().replace('#p2.mute()', 'p2.mute()')
assert 'No unsupported constructs detected' in run_compatibility_check(example30Muted)
mutedExampleRows, mutedExampleCommands = run_script(example30Muted, include_mutes=True)
assert len(mutedExampleRows) == 2 and mutedExampleCommands == [['M', '1', '1']]
example8 = run_example('08-synths-and-controls.py')
assert len(example8) == 2 and all(len(row) == 24 for row in example8)
p1, p2 = example8
assert p1[1] == '0' and p1[6] == '1' and abs(float(p1[7]) - 0.7) < 1e-6
assert abs(float(p1[8]) + 0.4) < 1e-6 and p1[9] == '5000.0'
assert p1[10] == '0.0' and p1[11] == '0.0'
assert p2[1] == '1' and p2[6] == '2' and abs(float(p2[7]) - 0.5) < 1e-6
assert abs(float(p2[8]) - 0.4) < 1e-6 and p2[9] == '1800.0'
assert p2[10] == '0.25' and p2[11] == '0.24'
example31 = run_example('31-bass-varsaw-synths.py')
assert len(example31) == 2
assert [int(row[6]) for row in example31] == [4, 5]
assert [float(row[21]) for row in example31] == [5.0, 0.32]
assert [float(row[14]) for row in example31] == [0.05, 0.04]
assert [float(row[15]) for row in example31] == [0.22, 0.22]
example32 = run_example('32-dirt-jbass-noise.py')
assert len(example32) == 3
assert [int(row[6]) for row in example32] == [7, 6, 8]
example33 = run_example('33-dub-growl-soprano.py')
assert len(example33) == 3
assert [int(row[6]) for row in example33] == [9, 10, 11]
assert [float(row[21]) for row in example33] == [0.0, 0.0, 2.0]
example34 = run_example('34-crunch-lazer-rave.py')
assert len(example34) == 3
assert [int(row[6]) for row in example34] == [12, 13, 14]
assert [float(row[21]) for row in example34] == [0.0, 2.5, 2.0]
example35 = run_example('35-resonant-bells.py')
assert len(example35) == 3 and [int(row[6]) for row in example35] == [15, 17, 16]
example36 = run_example('36-plucked-resonators.py')
assert len(example36) == 4 and [int(row[6]) for row in example36] == [18, 19, 20, 21]
example37Rows, example37Modulations = run_script(
    open(os.path.join(sys.argv[2], '37-bandpass-motion-and-swell.py'), encoding='utf-8').read(),
    include_modulations=True)
assert len(example37Rows) == 3 and len(example37Modulations) == 3
assert [row[0] for row in example37Modulations] == ['G', 'G', 'G']
assert [round(float(row[2]), 2) for row in example37Modulations] == [0.18, 0.0, 0.22]
assert [round(float(row[3]), 2) for row in example37Modulations] == [0.0, 1.2, 0.2]
assert example37Modulations[2][6].startswith('v:') and example37Modulations[2][7].startswith('l:')
example38Rows = run_example('38-bass-varsaw-release-check.py')
assert len(example38Rows) == 2 and [int(row[6]) for row in example38Rows] == [4, 5]
assert [float(row[14]) for row in example38Rows] == [0.04, 0.04]
assert [float(row[15]) for row in example38Rows] == [0.28, 0.28]
example39Rows, example39Events = run_script(
    open(os.path.join(sys.argv[2], '39-foxdot-native-bass-varsaw.py'), encoding='utf-8').read(),
    include_events=True)
assert [int(row[6]) for row in example39Rows] == [4, 5]
assert all(float(event[5]) == 0.01 and float(event[6]) == 0.04
           for event in example39Events)
for bassExampleName in os.listdir(os.path.abspath(sys.argv[2])):
    if not bassExampleName.endswith('.py'):
        continue
    bassExamplePath = os.path.join(os.path.abspath(sys.argv[2]),
                                   bassExampleName)
    bassExampleTree = ast.parse(
        open(bassExamplePath, encoding='utf-8').read(),
        filename=bassExampleName)
    for bassCall in ast.walk(bassExampleTree):
        if (not isinstance(bassCall, ast.Call) or
                not isinstance(bassCall.func, ast.Name) or
                bassCall.func.id not in ('bass', 'jbass')):
            continue
        octaveKeyword = next((keyword.value for keyword in bassCall.keywords
                              if keyword.arg == 'oct'), None)
        if isinstance(octaveKeyword, ast.Constant) and isinstance(
                octaveKeyword.value, (int, float)):
            assert octaveKeyword.value >= 5, (
                bassExampleName + ' places the /4 bass oscillator too low')
example40Rows = run_example('40-charm-and-spark.py')
assert [int(row[6]) for row in example40Rows] == [22, 23]
example41Rows = run_example('41-blip-oscillator.py')
assert [int(row[6]) for row in example41Rows] == [24, 22]
example42Rows = run_example('42-self-contained-ugens.py')
assert [int(row[6]) for row in example42Rows] == [25, 26, 27, 28, 29, 30]
assert namespace['bug']([0]).synthRate == 1.0
core_synth_names = namespace['_NEW_SYNTHS']
assert len(core_synth_names) == 26
for number, name in enumerate(core_synth_names, 43):
    filename = f'{number:02d}-foxdot-{name}.py'
    rows = run_example(filename)
    active_rows = [row for row in rows if row[2] == '0']
    stopped_players = {int(row[1]) for row in rows if row[2] == '1'}
    assert len(active_rows) == 1 and int(active_rows[0][6]) == number - 12, (filename, rows)
    assert stopped_players == set(range(1, 8)), (filename, stopped_players)
    # Every name must actually evaluate and reach the wire protocol, not merely
    # be whitelisted by the static checker.
    assert namespace[name](0).synth == name
assert namespace['viola'](0).synthControls[0] == 6.0
assert namespace['prophet'](0).synthControls[4] == 0.4
assert namespace['scratch'](0).synthRate == 0.04
assert namespace['swell'](0).synthRate == 1.0
assert namespace['razz'](0).synthRate == 0.3
assert namespace['pads'](0).attackSeconds == 0.1
assert namespace['viola'](0, vib=8, fmod=0.5, blur=2).synthControls == (8, 0.5, 1, 0.5, 0.5, 0.5, 2)
assert namespace['saw'](0, vib=6, vibdepth=0.02).synthControls[0:2] == (6.0, 0.02)
assert namespace['saw'](0, vib=6, depth=0.3).synthControls[0:2] == (6.0, 0.3)
try:
    namespace['saw'](0, depth=0.2, vibdepth=0.3)
except ValueError:
    pass
else:
    raise AssertionError('depth and vibdepth must not silently conflict')
for keyword, value in [('vib', float('inf')), ('rq', 0), ('blur', 0), ('lfowidth', 2)]:
    try:
        namespace['viola'](0, **{keyword: value})
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid synth control accepted: ' + keyword)
pitchEffectRows, pitchEffectEvents = run_script(
    'p1 >> saw([0, 2, 4], dur=0.5, vib=6, '
    'vibdepth=var([0.02, 0.04], [0.5, 0.5]), slide=P[0, 0.5, -0.25], '
    'slidefrom=0.2, slidedelay=0.25, bend=P[0, 0.3, 0], benddelay=0.5)\n',
    include_events=True)
assert len(pitchEffectRows) == 1 and len(pitchEffectEvents) == 3
assert [float(event[8]) for event in pitchEffectEvents] == [0.02, 0.04, 0.02]
assert [float(event[9]) for event in pitchEffectEvents] == [0.0, 0.5, -0.25]
assert all(float(event[10]) == 0.2 and float(event[11]) == 0.25
           for event in pitchEffectEvents)
assert [float(event[12]) for event in pitchEffectEvents] == [0.0, 0.3, 0.0]
assert all(float(event[13]) == 0.5 for event in pitchEffectEvents)
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw([0, 2], vib=6, vibdepth=0.02, slide=0.5, '
    'slidefrom=-0.2, slidedelay=0.3, bend=0.4, benddelay=0.1)\n')
samplePitchRows, samplePitchBuffers, samplePitchEvents = run_script(
    'p1 >> play("x-o", dur=0.5, slide=var([0, 0.5], [1, 1]), '
    'slidedelay=0.25, bend=0.2, benddelay=0.4)\n',
    include_samples=True, include_events=True)
assert len(samplePitchRows) == 1 and len(samplePitchBuffers) == 3, (
    len(samplePitchRows), len(samplePitchBuffers), len(samplePitchEvents))
assert len(samplePitchEvents) == 3
assert all(float(event[12]) == 0.2 and float(event[13]) == 0.4
           for event in samplePitchEvents)
for keyword, value in [('vibdepth', float('inf')), ('slide', -1.01),
                       ('slidefrom', 4.01), ('slidedelay', 1.01),
                       ('bend', -1.01), ('benddelay', 1.01)]:
    try:
        namespace['saw'](0, **{keyword: value})
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid note control accepted: ' + keyword)
supported_report = run_compatibility_check('Clock.bpm = 120\nScale.default.set("minor")\np1 >> saw([0, 2, 4], dur=0.5, oct=5, sus=0.4, amp=0.7, pan=-0.2, echo=0.5, decay=0.75)\np2 >> pulse([0, None, 3], dur=1, cutoff=3000)\np3 >> saw(P[0, None, 2], dur=0.5)\np4 >> triangle([0, 2], pan=MouseX(), cutoff=MouseY(), atk=0.02, rel=0.15)\np5 >> saw([0, 2], amp=var([0.2, 0.6], [4, 4]), pan=linvar([-0.8, 0.8], 8), cutoff=linvar([400, 5000], 8))\np1 >> saw(P[0:8:2], dur=0.5)\np6 >> saw([0, 2], lpf=var([400, 5000], 4), lpr=0.45, hpf=300, hpr=0.8, bpf=1200, bpr=0.35, bpnoise=0.3, swell=1.0)\np1.stop()\n')
assert 'No unsupported constructs detected' in supported_report, supported_report
assert 'script was not executed' in supported_report, supported_report
shapeDriveScript = (
    'p1 >> saw([0, 2, 4, 7], dur=0.5, shape=var([0.25, 0.75], [2, 2]), '
    'drive=linvar([0.5, 1.0], [4, 4]))\n'
    'p2 >> play("x-o-", dur=0.5, shape=0.2, drive=0.3)\n')
shapeDriveRows, shapeDriveControls = run_script(
    shapeDriveScript, include_modulations=True)
assert len(shapeDriveRows) == len(shapeDriveControls) == 2
assert shapeDriveControls[0][0:6] == ['G', '0', '0.0', '0.0', '0.25', '0.5']
assert shapeDriveControls[0][8] == 'v:0.25,0.75:2.0,2.0'
assert shapeDriveControls[0][9] == 'l:0.5,1.0:4.0,4.0'
assert shapeDriveControls[1][4:10] == ['0.2', '0.3', '-', '-', '-', '-']
groupShapeRows, groupShapeControls = run_script(
    'p1 >> saw([0, 2], dur=0.5)\n'
    'p2 >> bass([0, 3], dur=0.5)\n'
    'Group(p1, p2).shape = var([0.3, 0.7], [2, 2])\n',
    include_modulations=True)
assert len(groupShapeRows) == len(groupShapeControls) == 2
assert all(row[8] == 'v:0.3,0.7:2.0,2.0' for row in groupShapeControls)
shapeDriveReport = run_compatibility_check(shapeDriveScript)
assert 'No unsupported constructs detected' in shapeDriveReport, shapeDriveReport
for invalidFxValue in (-0.01, 4.01, float('nan'), True):
    for fxName in ('shape', 'drive'):
        try:
            namespace['saw']([0], **{fxName: invalidFxValue})
        except (ValueError, TypeError):
            pass
        else:
            raise AssertionError(f'{fxName} accepted out-of-range input')
for fxName in ('shape', 'drive'):
    try:
        setattr(namespace['Group'](), fxName, float('nan'))
    except (ValueError, TypeError):
        pass
    else:
        raise AssertionError(f'empty Group accepted invalid {fxName}')
shapeDriveExampleText = open(os.path.join(
    sys.argv[2], '163-foxdot-shape-drive-effects.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    shapeDriveExampleText)
shapeDriveExampleRows, shapeDriveExampleControls = run_script(
    shapeDriveExampleText, include_modulations=True)
assert len(shapeDriveExampleRows) == len(shapeDriveExampleControls) == 3
assert [float(row[4]) for row in shapeDriveExampleControls] == [0.0, 0.0, 0.25]
assert [float(row[5]) for row in shapeDriveExampleControls] == [0.0, 0.1, 0.0]
assert shapeDriveExampleControls[0][8].startswith('v:')
assert shapeDriveExampleControls[1][9].startswith('l:')
chopSpinExampleText = open(os.path.join(
    sys.argv[2], '165-foxdot-chop-spin.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    chopSpinExampleText)
chopSpinRows, chopSpinEvents = run_script(
    chopSpinExampleText, include_events=True)
assert len(chopSpinRows) == 3
assert any(float(event[14]) == 4.0 for event in chopSpinEvents)
assert any(float(event[15]) == 2.0 for event in chopSpinEvents)
for fxName, invalidFxValues in (
        ('chop', (-0.01, 32.01, float('nan'), True)),
        ('spin', (-0.01, 40.01, float('nan'), True))):
    for invalidFxValue in invalidFxValues:
        try:
            namespace['saw']([0], **{fxName: invalidFxValue})
        except (ValueError, TypeError):
            pass
        else:
            raise AssertionError(f'{fxName} accepted out-of-range input')
        if not isinstance(invalidFxValue, bool):
            try:
                namespace['saw'](
                    [0], **{fxName: namespace['var']([0, invalidFxValue], 4)})
            except (ValueError, TypeError):
                pass
            else:
                raise AssertionError(f'{fxName} var accepted out-of-range input')
compat_chop_spin = run_compatibility_check(
    'p1 >> saw([0, 2], chop=var([4, 8], 4), spin=linvar([0, 6], 8))\n'
    'p2 >> play("x---", dur=1, chop=8, spin=2)\n'
    'Group(p1, p2).spin = var([2, 4], 4)\n')
assert 'No unsupported constructs detected' in compat_chop_spin, compat_chop_spin
compat_pattern_internal_methods = run_compatibility_check(
    'notes = P[0, 2, 4]\n'
    'notes.eq(2)\n'
    'notes.getslice(0, 2)\n'
    'notes.i_rotate(1)\n'
    'notes.i_reverse()\n'
    'notes.i_sort()\n'
    'notes.i_shuf()\n'
    'notes.setitem(0, 1)\n'
    'p1 >> saw(notes, dur=0.5)\n')
assert 'No unsupported constructs detected' in compat_pattern_internal_methods, (
    compat_pattern_internal_methods)
generatorCompatibilityReport = run_compatibility_check(
    'p1 >> saw(PSq(0, 2, 5).norm(), dur=0.25)\n'
    'p1 >> saw(P10(16, seed=7), dur=0.25)\n'
    'p2 >> saw(PBern(size=16, ratio=0.4, seed=8), dur=0.25)\n'
    'p3 >> saw(PSine(16), dur=0.25)\n'
    'p4 >> saw(PTri(0, 8, 2), dur=0.25)\n'
    'p5 >> saw(PSum(8, 4, lim=0.125), dur=0.25)\n'
    'p2 >> play(PEuclid2(3, 8, "-", "x"), dur=0.25, sample=0)\n'
    'p3 >> saw(PRange(0, 8, 2).undup(), dur=0.5)\n'
    'notes = P[0, 2, 4, 5].compress(P[1, 0])\n'
    'p4 >> saw(notes.select(P[1, 0]), dur=0.5)\n'
    'p5 >> saw(notes.sample(2, seed=17), dur=0.5)\n'
    'p6 >> saw(notes.deep_shuffle(2, seed=17), dur=0.25)\n'
    'p7 >> saw(PIndex(16), dur=0.25)\n'
    'p8 >> saw(PxRand(P[0, 2, 4, 7], length=32, seed=9), dur=0.25)\n'
    'p1 >> saw(PwRand(P[0, 2, 4, 7], P[1, 2, 3, 1], length=32, seed=8), dur=0.25)\n'
    'p2 >> play(PxRand("x-o", length=32, seed=7), dur=0.25, sample=0)\n')
assert 'No unsupported constructs detected' in generatorCompatibilityReport, (
    generatorCompatibilityReport)
mixedRandomGeneratorReport = run_compatibility_check(
    'PZip(PxRand("xo", length=8, seed=1), P[0, 1])\n')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in (
    mixedRandomGeneratorReport), mixedRandomGeneratorReport
compat_pvar_expand = run_compatibility_check(
    'phrases = Pvar([P[0, 2], P[4, 5]], [1, 1])\n'
    'phrases.expand(0.5)\n')
assert 'No unsupported constructs detected' in compat_pvar_expand, (
    compat_pvar_expand)
compat_pattern_expand = run_compatibility_check(
    'P[0, 2, 4].expand(0.5)\n')
assert 'P pattern method' in compat_pattern_expand and 'expand' in (
    compat_pattern_expand), compat_pattern_expand
for unsupported_pattern_method in ('transpose',):
    for pattern_expression in (
            'P[0, 2, 4]',
            'notes = P[0, 2, 4]\nnotes'):
        patternMethodReport = run_compatibility_check(
            pattern_expression + '.' + unsupported_pattern_method +
            '()\np1 >> saw([0])\n')
        assert 'P pattern method' in patternMethodReport, patternMethodReport
        assert unsupported_pattern_method in patternMethodReport, patternMethodReport
for malformed_generator_call, expected_diagnostic in (
        ('PRange()', 'requires start'),
        ('PStep(4)', 'requires value'),
        ('PEuclid(3)', 'requires k'),
        ('PEuclid2(3, 8, "-", "x", 1)', 'at most 4 positional'),
        ('P10()', 'requires n'),
        ('PBern(16, 0.5, 1, 2)', 'at most 3 positional'),
        ('PSine(16, 2)', 'at most 1 positional'),
        ('PTri()', 'requires start'),
        ('PSum(8)', 'requires total'),
        ('PIndex(8, 2)', 'at most 1 positional'),
        ('PxRand()', 'requires a start'),
        ('PwRand(P[0, 2])', 'requires weights'),
        ('PwRand(P[0, 2], P[1, 2], 8, 1, 9)', 'at most 4 positional')):
    malformedGeneratorReport = run_compatibility_check(
        'p1 >> saw(' + malformed_generator_call + ')\n')
    assert expected_diagnostic in malformedGeneratorReport, (
        malformed_generator_call, expected_diagnostic,
        malformedGeneratorReport)
loadedUserSynthReport = run_compatibility_check(
    'p1 >> userSynth("CustomPluck", P[0, 2, 4], dur=0.5, amp=0.3)\n',
    ('CustomPluck',))
assert 'No unsupported constructs detected' in loadedUserSynthReport, loadedUserSynthReport
mappedUserSynthControlReport = run_compatibility_check(
    'p1 >> userSynth("CustomPluck", [0, 2], vib=0.5)\n',
    ('CustomPluck',))
assert 'No unsupported constructs detected' in mappedUserSynthControlReport, mappedUserSynthControlReport
missingUserSynthReport = run_compatibility_check(
    'p1 >> userSynth("NotInstalled", [0, 2], dur=0.5)\n')
assert 'not present in Portable/SynthDefs' in missingUserSynthReport
unsupported_report = run_compatibility_check('from FoxDot import *\nClock.swing = 0.2\np9 >> bass([0, 2], dur=0.5)\nplay("x-o-")\nP["x-o"]\nP[0:]\nP[0:257]\nP[0:8:0]\nP[start:8]\nP[0] | ["x-o"]\n')
for phrase in ('Line 1:', 'Line 2:', 'Line 5:', 'P sample-symbol literals', 'finite stop', '256 generated items', 'step cannot be zero', 'integer-literal bounds', 'Pattern | and & do not accept sample strings'):
    assert phrase.lower() in unsupported_report.lower(), (phrase, unsupported_report)
sampleZipxReport = run_compatibility_check(
    'p1 >> play(P["x", "o"].zipx(P["-", "x"]), sample=0)\n')
assert 'No unsupported constructs detected' in sampleZipxReport, sampleZipxReport
syntax_report = run_compatibility_check('p1 >> saw([0])\nif :\n')
assert 'Python syntax error at column' in syntax_report and 'Line 2:' in syntax_report, syntax_report
for filename in os.listdir(sys.argv[2]):
    if filename.endswith('.py'):
        example_names = (
            ('CustomTone',)
            if filename == '202-custom-synth-live-controls.py' else
            ('CustomPluck',) if filename == '94-user-synthdef-loader.py' else
            ('AirwindowsTapeDelay2',)
            if filename == '103-sample-player-airwindows-delay.py' else
            ('AirwindowsEchojam',)
            if filename == '104-sample-player-airwindows-echojam.py' else
            ('AirwindowsTapeDelay2', 'AirwindowsEchojam')
            if filename == '105-airwindows-timevar-controls.py' else
            ('AirwindowsEchojam',)
            if filename == '186-airwindows-echojam-on-chords.py' else
            ('Chorus', 'Phaser', 'Tremolo')
            if filename == '187-effect-chain-control-sliders.py' else
            ('Chorus',) if filename == '168-chorus-stereo-width.py' else
            ('Flanger',) if filename == '169-flanger-sweep.py' else
            ('Chorus', 'Flanger')
            if filename == '170-chorus-flanger-together.py' else
            ('Phaser',) if filename == '171-phaser-sweep.py' else
            ('Tremolo',) if filename == '172-tremolo-motion.py' else
            ('Phaser', 'Tremolo') if filename == '173-ordered-player-effect-chain.py' else
            ('Flanger', 'Chorus') if filename == '174-drum-effect-chain.py' else
            ('LimutPingPong', 'LimutDuck')
            if filename == '195-limut-pingpong-duck.py' else
            ('LimutDisperser', 'LimutFuzz')
            if filename == '196-limut-disperser-fuzz.py' else
            ('LimutShifter', 'LimutShimmer', 'LimutGatedVerb')
            if filename in ('197-limut-shifter-shimmer.py',
                            '198-limut-gated-reverb.py') else
            ('Chorus', 'Flanger', 'Phaser')
            if filename == '227-pbindfx-style-event-effects.py' else
            ('JPverb', 'Greyhole', 'LimutPingPong',
             'LimutShimmer', 'LimutGatedVerb')
            if filename == '228-pbindfx-style-delay-reverb-tails.py' else
            ('AirwindowsTapeDelay2', 'AirwindowsEchojam')
             if filename == '229-seeded-random-per-note-delays.py' else
             ('Chorus', 'Phaser', 'Flanger', 'Tremolo')
             if filename == '230-pbindfx-style-ordered-note-effect-chains.py'
              else ())
        if filename == '201-sclork-insert-effects.py':
            example_names = (
                'SCLOrkPhazerFX', 'SCLOrkDecimatorFX',
                'SCLOrkCrossoverDistortionFX')
        if filename == '199-limut-compressor.py':
            example_names = ('LimutCompressor',)
        if filename in ('214-jazz-brush-kit.py',
                        '217-jazz-kit-core-groove.py',
                        '218-jazz-kit-kick-compare.py'):
            example_names = jazz_synth_names
        example_report = run_compatibility_check(
            open(os.path.join(sys.argv[2], filename), encoding='utf-8').read(),
            example_names)
        assert 'No unsupported constructs detected' in example_report, (filename, example_report)
for scale_name in namespace['_SCALES']:
    namespace['Scale'].default.set(scale_name)
    assert namespace['Scale'].__dict__[scale_name] == scale_name
example69 = run_example('69-composed-star-and-karp.py')
assert [int(row[6]) for row in example69 if row[2] == '0'] == [55, 56, 0]
example70 = run_example('70-portable-sample-beat.py')
assert int(next(row for row in example70 if row[2] == '0')[6]) == 57
example141Text = open(os.path.join(
    sys.argv[2], '141-finite-pattern-generators.py'), encoding='utf-8').read()
example141, example141Samples, example141Events = run_script(
    example141Text, include_samples=True, include_events=True)
assert {int(row[1]) for row in example141 if row[2] == '0'} == {0, 1, 2, 3}
assert len(example141Samples) == 3
assert len(example141Events) == 19
example142Text = open(os.path.join(
    sys.argv[2], '142-pattern-select-and-compress.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example142Text)
example142, example142Samples, example142Events = run_script(
    example142Text, include_samples=True, include_events=True)
assert {int(row[1]) for row in example142 if row[2] == '0'} == {0, 1, 2}
assert len(example142Samples) == 2
assert len(example142Events) == 11
example143Text = open(os.path.join(
    sys.argv[2], '143-sample-and-deep-shuffle.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example143Text)
example143, example143Samples, example143Events = run_script(
    example143Text, include_samples=True, include_events=True)
assert {int(row[1]) for row in example143 if row[2] == '0'} == {0, 1}
assert not example143Samples
assert len(example143Events) == 20
example144Text = open(os.path.join(
    sys.argv[2], '144-callable-map-and-submap.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example144Text)
example144, example144Samples, example144Events = run_script(
    example144Text, include_samples=True, include_events=True)
assert {int(row[1]) for row in example144 if row[2] == '0'} == {0, 1}
assert len(example144Samples) == 4
assert len(example144Events) == 9
example145Text = open(os.path.join(
    sys.argv[2], '145-duplicate-and-iter.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example145Text)
example145, example145Events = run_script(
    example145Text, include_events=True)
assert {int(row[1]) for row in example145 if row[2] == '0'} == {0, 1}
assert len(example145Events) == 24
example146Text = open(os.path.join(
    sys.argv[2], '146-pattern-zipx-nested-pairs.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example146Text)
example146, example146Events = run_script(
    example146Text, include_events=True)
assert {int(row[1]) for row in example146 if row[2] == '0'} == {0, 1}
assert len(example146Events) == 14
example147Text = open(os.path.join(
    sys.argv[2], '147-concat-add-abs-and-checks.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example147Text)
example147, example147Events = run_script(
    example147Text, include_events=True)
assert {int(row[1]) for row in example147 if row[2] == '0'} == {0}
assert len(example147Events) == 6
example148Text = open(os.path.join(
    sys.argv[2], '148-bounded-limit-patterns.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example148Text)
example148, example148Events = run_script(
    example148Text, include_events=True)
assert {int(row[1]) for row in example148 if row[2] == '0'} == {0, 1, 2}
assert len(example148Events) == 16
example149Text = open(os.path.join(
    sys.argv[2], '149-pattern-every-transforms.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example149Text)
example149, example149Events = run_script(
    example149Text, include_events=True)
assert {int(row[1]) for row in example149 if row[2] == '0'} == {0, 1, 2}
assert len(example149Events) == 27
example150Text = open(os.path.join(
    sys.argv[2], '150-pattern-copy.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example150Text)
example150, example150Events = run_script(
    example150Text, include_events=True)
assert {int(row[1]) for row in example150 if row[2] == '0'} == {0, 1}
assert len(example150Events) == 8
example151Text = open(os.path.join(
    sys.argv[2], '151-pattern-as-group-chords.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example151Text)
example151, example151Events = run_script(
    example151Text, include_events=True)
assert {int(row[1]) for row in example151 if row[2] == '0'} == {0, 1}
assert len(example151Events) == 7
assert [round(float(event[1]), 6) for event in example151Events[:4]] == [
    0.0, 0.0, 0.0, 0.0]
assert [round(float(event[1]), 6) for event in example151Events[4:]] == [
    0.0, 0.0, 0.0]
example152Text = open(os.path.join(
    sys.argv[2], '152-pattern-numeric-casts.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example152Text)
example152, example152Events = run_script(
    example152Text, include_events=True)
assert {int(row[1]) for row in example152 if row[2] == '0'} == {0, 1, 2}
assert len(example152Events) == 11
example153Text = open(os.path.join(
    sys.argv[2], '153-pattern-string-casts.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example153Text)
example153, example153Events = run_script(
    example153Text, include_events=True)
assert {int(row[1]) for row in example153 if row[2] == '0'} == {0, 1}
assert len(example153Events) == 8
example154Text = open(os.path.join(
    sys.argv[2], '154-pattern-append-extend.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example154Text)
example154, example154Events = run_script(
    example154Text, include_events=True)
assert {int(row[1]) for row in example154 if row[2] == '0'} == {0, 1, 2}, example154
assert len(example154Events) == 20, len(example154Events)
example155Text = open(os.path.join(
    sys.argv[2], '155-live-pattern-mutation.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example155Text)
example155, example155Events = run_script(
    example155Text, include_events=True)
assert {int(row[1]) for row in example155 if row[2] == '0'} == {0, 1, 2}
assert len(example155Events) == 16
example156Text = open(os.path.join(
    sys.argv[2], '156-pattern-insert-pop-remove.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example156Text)
example156, example156Events = run_script(
    example156Text, include_events=True)
assert {int(row[1]) for row in example156 if row[2] == '0'} == {0, 1, 2, 3}
assert len(example156Events) > 0
example157Text = open(os.path.join(
    sys.argv[2], '157-pattern-set-and-alt.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example157Text)
example157, example157Events = run_script(
    example157Text, include_events=True)
assert {int(row[1]) for row in example157 if row[2] == '0'} == {0, 1, 2}
assert len(example157Events) > 0
example158Text = open(os.path.join(
    sys.argv[2], '158-pattern-string-to-samples.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example158Text)
example158, example158Events = run_script(
    example158Text, include_events=True)
assert {int(row[1]) for row in example158 if row[2] == '0'} == {0, 1}
assert len(example158Events) > 0
example159Text = open(os.path.join(
    sys.argv[2], '159-pattern-items.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example159Text)
example159, example159Events = run_script(
    example159Text, include_events=True)
assert {int(row[1]) for row in example159 if row[2] == '0'} == {0, 1}
assert len(example159Events) > 0
example160Text = open(os.path.join(
    sys.argv[2], '160-cyclic-pattern-getitem.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example160Text)
example160, example160Events = run_script(
    example160Text, include_events=True)
assert {int(row[1]) for row in example160 if row[2] == '0'} == {0, 1}
assert len(example160Events) > 0
example161Text = open(os.path.join(
    sys.argv[2], '161-pattern-comparison-and-inplace-helpers.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example161Text)
example161, example161Events = run_script(
    example161Text, include_events=True)
assert {int(row[1]) for row in example161 if row[2] == '0'} == {0, 1, 2, 3}
assert len(example161Events) > 0
silentMaskRows, silentMaskEvents = run_script(
    'p1 >> sine(P[0, 2, 4].select(P[0, 0]), dur=0.5)\n',
    include_events=True)
assert len(silentMaskRows) == 1 and not silentMaskEvents
example71 = run_example('71-sample-selection-and-reverse.py')
assert [int(row[6]) for row in example71 if row[2] == '0'] == [57, 57]
example72 = run_example('72-var-sample-and-rate.py')
assert [int(row[6]) for row in example72 if row[2] == '0'] == [57, 57]
example73 = run_example('73-linvar-sample-and-rate.py')
assert [int(row[6]) for row in example73 if row[2] == '0'] == [57]
example74 = run_example('74-nested-play-strings.py')
assert [int(row[6]) for row in example74 if row[2] == '0'] == [57, 57]
example75 = run_example('75-random-and-layered-strings.py')
assert [int(row[6]) for row in example75 if row[2] == '0'] == [57, 57, 57]
example76 = run_example('76-sample-index-selection.py')
assert [int(row[6]) for row in example76 if row[2] == '0'] == [57]
example77 = run_example('77-pattern-sample-sequences.py')
assert len(example77) == 2
example100 = run_example('100-sample-loop-and-slicing.py')
assert [int(row[6]) for row in example100 if row[2] == '0'] == [57, 57]
example101 = run_example('101-patterned-sample-slices.py')
assert [int(row[6]) for row in example101 if row[2] == '0'] == [57, 57]
example102 = run_example('102-random-pattern-generators.py')
assert [int(row[5]) for row in example102 if row[0] == 'P'] == [16, 16]
renardoSeedExample = run_example('189-renardo-seed-default.py')
assert len(renardoSeedExample) == 8
assert all(row[0] == 'P' for row in renardoSeedExample)
assert sum(row[2] == '0' for row in renardoSeedExample) == 3
renardoGeneratorExampleText = open(os.path.join(
    sys.argv[2], '190-renardo-probability-fibonacci-patterns.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    renardoGeneratorExampleText)
renardoGeneratorExample, renardoGeneratorEvents = run_script(
    renardoGeneratorExampleText, include_events=True)
assert len(renardoGeneratorExample) == 8
assert {int(row[1]) for row in renardoGeneratorExample if row[2] == '0'} == {
    0, 1, 2}
assert len(renardoGeneratorEvents) > 0
comparisonExampleText = open(os.path.join(
    sys.argv[2], '191-pattern-comparison-operators.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    comparisonExampleText)
comparisonExample, comparisonEvents = run_script(
    comparisonExampleText, include_events=True)
assert len(comparisonEvents) > 0
assert len([row for row in comparisonExample if row[0] == 'P' and
            row[2] == '0']) == 3
transformExampleText = open(os.path.join(
    sys.argv[2], '192-pattern-transform-recursive.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    transformExampleText)
transformExample, transformEvents = run_script(
    transformExampleText, include_events=True)
assert len(transformEvents) > 0
assert len([row for row in transformExample if row[0] == 'P' and
            row[2] == '0']) == 2
copyConvertExampleText = open(os.path.join(
    sys.argv[2], '193-true-copy-convert-data.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    copyConvertExampleText)
copyConvertExample, copyConvertEvents = run_script(
    copyConvertExampleText, include_events=True)
assert len(copyConvertEvents) > 0
assert len([row for row in copyConvertExample if row[0] == 'P' and
            row[2] == '0']) == 2
metadataExampleText = open(os.path.join(
    sys.argv[2], '194-pattern-metadata-helpers.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    metadataExampleText)
metadataExample, metadataEvents = run_script(
    metadataExampleText, include_events=True)
assert len(metadataEvents) > 0
assert len([row for row in metadataExample if row[0] == 'P' and
            row[2] == '0']) == 2
example103, example103EffectRows = run_example(
    '103-sample-player-airwindows-delay.py', ('AirwindowsTapeDelay2',),
    include_player_effects=True)
assert [int(row[5]) for row in example103 if row[0] == 'P'] == [28, 6]
assert example103EffectRows == [['A', '0', '0'], ['A', '1', '-1']]
example103, example103ControlRows = run_example(
    '103-sample-player-airwindows-delay.py', ('AirwindowsTapeDelay2',),
    include_player_effect_values=True)
example103TapeControls = next(row for row in example103ControlRows
                              if row[1] == '0')
assert [float(value) for value in example103TapeControls[2:]] == [
    0.72, 0.64, 0.35, 0.42, 0.18, 0.72, 0.76]
example104, example104EffectRows, example104ControlRows = run_example(
    '104-sample-player-airwindows-echojam.py', ('AirwindowsEchojam',),
    include_player_effects=True, include_player_effect_values=True)
assert [int(row[5]) for row in example104 if row[0] == 'P'] == [28, 6]
assert example104EffectRows == [['A', '0', '0'], ['A', '1', '-1']]
example104EchojamControls = next(row for row in example104ControlRows
                                 if row[1] == '0')
assert [float(value) for value in example104EchojamControls[2:]] == [
    0.62, 0.72, 0.38, 0.5, 0.5, 0.68, 0.82]
modulatedEffect = run_script(
    'p1 >> play("x", effect="AirwindowsTapeDelay2", '
    'fx_mix=linvar([0.25, 0.75], [2, 2]), '
    'fx_vib=var([0.2, 0.8], [2, 3]), '
    'fx_depth=linvar([0.1, 0.9], 8), fx_blur=0.6)\n',
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True, user_synth_names=('AirwindowsTapeDelay2',))
modulatedControls = next(row for row in modulatedEffect[3] if row[1] == '0')
assert modulatedControls[2:] == [
    'v:0.2,0.8:2.0,3.0', 'l:0.1,0.9:8.0,8.0',
    '-', '-', '-', 'l:0.25,0.75:2.0,2.0', '-']
modulatedEchojam = run_script(
    'p1 >> play("x", effect="AirwindowsEchojam", '
    'fx_mix=var([0.2, 0.8], 4), '
    'fx_lforate=var([0.15, 0.7], 4), '
    'fx_blur=linvar([0.2, 0.8], [2, 6]))\n',
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True, user_synth_names=('AirwindowsEchojam',))
echojamControlVars = next(row for row in modulatedEchojam[3] if row[1] == '0')
assert echojamControlVars[2:] == [
    '-', '-', 'v:0.15,0.7:4.0,4.0', '-', '-',
    'v:0.2,0.8:4.0,4.0',
    'l:0.2,0.8:2.0,6.0']
badTapeControl = run_error_script(
    'p1 >> play("x", effect="AirwindowsTapeDelay2", fx_depth=1.1)\n',
    ('AirwindowsTapeDelay2',))
assert 'fx_depth must be between 0.0 and 1.0' in badTapeControl
unsupportedEchojamControl = run_error_script(
    'p1 >> play("x", effect="AirwindowsEchojam", fx_rq=0.4)\n',
    ('AirwindowsEchojam',))
assert 'fx_rq is not a control on AirwindowsEchojam' in unsupportedEchojamControl
timeVaryingEffectControl = run_error_script(
    'p1 >> play("x", effect="AirwindowsEchojam", fx_depth=var([0.2, 0.8]))\n',
    ('AirwindowsEchojam',))
assert not timeVaryingEffectControl.startswith('ERROR'), timeVaryingEffectControl
invalidTimeVaryingEffectControl = run_error_script(
    'p1 >> play("x", effect="AirwindowsEchojam", '
    'fx_depth=linvar([0.2, 1.2]))\n', ('AirwindowsEchojam',))
assert 'fx_depth must be between 0.0 and 1.0' in invalidTimeVaryingEffectControl
example105, example105Effects, example105Controls, example105ControlVars = run_example(
    '105-airwindows-timevar-controls.py',
    ('AirwindowsTapeDelay2', 'AirwindowsEchojam'),
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True)
assert example105Effects == [['A', '0', '0'], ['A', '1', '1'], ['A', '2', '-1']]
assert len([row for row in example105ControlVars if row[1] == '0']) == 1
assert len([row for row in example105ControlVars if row[1] == '1']) == 1
assert all(any(token[0] in 'vl' for token in row[2:])
           for row in example105ControlVars if row[1] in ('0', '1'))
orphanEffectControl = run_error_script(
    'p1 >> play("x", fx_blur=0.4)\n')
assert 'fx_* controls require effect=' in orphanEffectControl
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("x-o-", effect="AirwindowsTapeDelay2")\n',
    ('AirwindowsTapeDelay2',))
assert 'requires the matching player-effect SynthDef' in run_compatibility_check(
    'p1 >> play("x-o-", effect="AirwindowsTapeDelay2")\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("x-o-", effect="AirwindowsEchojam", fx_mix=0.25)\n',
    ('AirwindowsEchojam',))
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("x-o-", effects=[("AirwindowsEchojam", {"fx_mix": 0.25})])\n',
    ('AirwindowsEchojam',))
assert 'fx_mix must be between 0.0 and 1.0' in run_error_script(
    'p1 >> play("x", effect="AirwindowsEchojam", fx_mix=1.1)\n',
    ('AirwindowsEchojam',))
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("x-o-", effects=[("AirwindowsEchojam", {"fx_depth": 0.25}), '
    '("Chorus", {"fx_mix": 0.3, "fx_rate": 0.6})])\n',
    ('AirwindowsEchojam', 'Chorus'))
limutPingPongCompatibility = run_compatibility_check(
    'p1 >> play("x-o-", effect="LimutPingPong", '
    'fx_time=var([0.25, 0.5], 4), fx_feedback=0.55)\n',
    ('LimutPingPong',))
assert 'No unsupported constructs detected' in limutPingPongCompatibility
limutDuckCompatibility = run_compatibility_check(
    'p2 >> play("x-o-", effects=[("LimutPingPong", {"fx_time": 0.25}), '
    '("LimutDuck", {"fx_period": 0.5, "fx_depth": 0.7, '
    '"fx_length": 0.12, "fx_attack": 0.18})])\n',
    ('LimutPingPong', 'LimutDuck'))
assert 'No unsupported constructs detected' in limutDuckCompatibility
assert 'requires the matching player-effect SynthDef' in run_compatibility_check(
    'p1 >> play("x", effect="LimutDuck")\n')
assert 'fx_hold is not a control on LimutDuck' in run_compatibility_check(
    'p1 >> play("x", effect="LimutDuck", fx_hold=0.1)\n',
    ('LimutDuck',))
limutDisperserCompatibility = run_compatibility_check(
    'p1 >> play("x-o-", effect="LimutDisperser", '
    'fx_frequency=var([440, 880], 4), fx_q=3, fx_stages=12)\n',
    ('LimutDisperser',))
assert 'No unsupported constructs detected' in limutDisperserCompatibility
limutFuzzCompatibility = run_compatibility_check(
    'p1 >> play("x-o-", effects=[("LimutFuzz", '
    '{"fx_mix": 0.4, "fx_drive": linvar([6, 24], 8), "fx_tone": 9000})])\n',
    ('LimutFuzz',))
assert 'No unsupported constructs detected' in limutFuzzCompatibility
assert 'fx_stages is not a control on LimutFuzz' in run_compatibility_check(
    'p1 >> play("x", effect="LimutFuzz", fx_stages=8)\n',
    ('LimutFuzz',))
limutShifterCompatibility = run_compatibility_check(
    'p1 >> pluck([0, 7], effect="LimutShifter", fx_ratio=1.5, '
    'fx_window=0.12, fx_mix=0.8)\n', ('LimutShifter',))
assert 'No unsupported constructs detected' in limutShifterCompatibility
limutShimmerCompatibility = run_compatibility_check(
    'p1 >> pluck([0, 7], effects=[("LimutShifter", {"fx_ratio": 1.5}), '
    '("LimutShimmer", {"fx_ratio": 2, "fx_room": 0.8, "fx_damp": 0.3})])\n',
    ('LimutShifter', 'LimutShimmer'))
assert 'No unsupported constructs detected' in limutShimmerCompatibility
limutGatedCompatibility = run_compatibility_check(
    'p1 >> pluck([0, 7], effect="LimutGatedVerb", '
    'fx_threshold=0.02, fx_hold=var([0.1, 0.2], 4), '
    'fx_attack=0.01, fx_release=0.15)\n', ('LimutGatedVerb',))
assert 'No unsupported constructs detected' in limutGatedCompatibility
limutCompressorCompatibility = run_compatibility_check(
    'p1 >> pluck([0, 3, 7], effect="LimutCompressor", '
    'fx_threshold=-24, fx_ratio=var([3, 9], 4), fx_attack=0.01, '
    'fx_release=0.12, fx_knee=4)\n', ('LimutCompressor',))
assert 'No unsupported constructs detected' in limutCompressorCompatibility
assert 'fx_knee is not a control on LimutDuck' in run_compatibility_check(
    'p1 >> pluck([0], effect="LimutDuck", fx_knee=4)\n', ('LimutDuck',))
vocoderCompatibility = run_compatibility_check(
    'p1 >> pluck([0], effect="LimutVocoder")\n', ('LimutVocoder',))
assert 'requires a separate modulator bus' in vocoderCompatibility
assert 'fx_time is not a control on LimutShifter' in run_compatibility_check(
    'p1 >> pluck([0], effect="LimutShifter", fx_time=0.2)\n',
    ('LimutShifter',))
assert 'supports the bundled Airwindows, reverb, modulation, and LimutFX player-effect SynthDefs.' in run_compatibility_check(
    'p1 >> play("x-o-", effect="NotAnEffect")\n',
    ('NotAnEffect',))
for name in ('Chorus', 'Flanger', 'Phaser', 'Tremolo'):
    report = run_compatibility_check(
        'p1 >> play("x-o-", effect="' + name + '", fx_rate=0.5)\n',
        (name,))
    assert 'No unsupported constructs detected' in report, report
    missing = run_compatibility_check(
        'p1 >> play("x-o-", effect="' + name + '")\n')
    assert 'requires the matching player-effect SynthDef' in missing, missing
for filename, names, effect_indices, player_count in (
        ('168-chorus-stereo-width.py', ('Chorus',), ['0'], 1),
        ('169-flanger-sweep.py', ('Flanger',), ['0'], 1),
        ('170-chorus-flanger-together.py', ('Chorus', 'Flanger'),
         ['0', '1'], 2),
        ('171-phaser-sweep.py', ('Phaser',), ['0', '0'], 2),
        ('172-tremolo-motion.py', ('Tremolo',), ['0', '0'], 2)):
    rows, effects, controls, variables = run_example(
        filename, names, include_player_effects=True,
        include_synth_controls=True, include_effect_control_vars=True)
    assert len(rows) == player_count, (filename, rows, effects, controls, variables)
    assert [row[2] for row in effects] == effect_indices
    assert len(controls) == player_count
    assert len(variables) == player_count
chorusControls = run_script(
    'p1 >> play("x", effect="Chorus", fx_rate=linvar([0.1, 2.5], [2, 3]), '
    'fx_tone=var([1000, 12000], 4))\n',
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True, user_synth_names=('Chorus',))
chorusTimeVars = next(row for row in chorusControls[3]
                      if row[1] == '0')
assert chorusTimeVars[3].startswith('l:0.010101010101'), chorusTimeVars
assert chorusTimeVars[8].startswith('v:0.050632911392'), chorusTimeVars
flangerControls = run_script(
    'p1 >> play("x", effect="Flanger", fx_feedback=var([0, 0.95], 2))\n',
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True, user_synth_names=('Flanger',))
assert next(row for row in flangerControls[3] if row[1] == '0')[2:8] == [
    '-', '-', '-', '-', '-', 'v:0.0,1.0:2.0,2.0']
phaserControls = run_script(
    'p1 >> saw([0], effect="Phaser", fx_center=var([250, 800], 4), '
    'fx_feedback=0.5)\n',
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True, user_synth_names=('Phaser',))
assert next(row for row in phaserControls[3] if row[1] == '0')[5].startswith(
    'v:0.06060606060606061,0.3939393939393939'), phaserControls[3]
tremoloControls = run_script(
    'p1 >> sine([0], effect="Tremolo", fx_shape=0.75, '
    'fx_phase=linvar([0, 0.5], [2, 2]))\n',
    include_player_effects=True, include_synth_controls=True,
    include_effect_control_vars=True, user_synth_names=('Tremolo',))
assert next(row for row in tremoloControls[3] if row[1] == '0')[7].startswith(
    'l:0.0,0.5:2.0,2.0'), tremoloControls[3]
eventEffectCompatibility = run_compatibility_check(
    'p1 >> pluck(P[0, 4, 7], fx_event=P[None, "Chorus", "Flanger"], '
    'fx_event_mix=P[0, 0.35, 0.4])\n', ('Chorus', 'Flanger'))
assert 'No unsupported constructs detected' in eventEffectCompatibility
assert 'requires the matching player-effect SynthDef' in run_compatibility_check(
    'p1 >> pluck(P[0, 4], fx_event=P["Chorus", None])\n')
assert 'ordered pair such as' in \
    run_compatibility_check(
        'p1 >> pluck(P[0], fx_event="NotAnEffect")\n',
        ('AirwindowsEchojam',))
assert 'fx_event_mix values must be from 0 to 1' in run_error_script(
    'p1 >> pluck(P[0], fx_event="Chorus", fx_event_mix=1.1)\n',
    ('Chorus',))
assert 'fx_event effect Chorus is not loaded' in run_error_script(
    'p1 >> pluck(P[0], fx_event="Chorus")\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> pluck(P[0, 4], effect="Chorus", '
    'fx_event=P["Flanger", None], fx_event_mix=P[0.3, 0.0])\n',
    ('Chorus', 'Flanger'))
assert 'at most 256 effect names' in run_compatibility_check(
    'p1 >> pluck(P[0], fx_event=P[' + ','.join(['None'] * 257) + '])\n')
eventEffectRows = run_script(
    'p1 >> pluck(P[0, 4, 7, 2], fx_event=P[None, "Chorus", "Flanger"], '
    'fx_event_mix=P[0.0, 0.35, 0.4])\n',
    include_events=True, user_synth_names=('Chorus', 'Flanger'))
eventEffectNotes = eventEffectRows[1]
assert [row[20] for row in eventEffectNotes] == ['-1', '0', '1', '-1']
assert [float(row[21]) for row in eventEffectNotes] == [0.0, 0.35, 0.4, 0.0]
assert all(len(row) == 37 for row in eventEffectNotes)
assert [row[28] for row in eventEffectNotes] == ['0', '1', '1', '0']
assert [row[29] for row in eventEffectNotes] == ['-1'] * 4
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> pluck(P[0, 2, 4], fx_event=P['
    '("Chorus", "Phaser"), None, ("Flanger", "Tremolo")], '
    'fx_event_mix=0.25)\n',
    ('Chorus', 'Phaser', 'Flanger', 'Tremolo'))
chainEffectScript = (
    'p1 >> pluck(P[0, 2, 4], fx_event=P['
    '("Chorus", "Phaser"), None, ("Flanger", "Tremolo")], '
    'fx_event_mix=P[0.25, 0.0, 0.4])\n')
chainEffectNames = (
    'Chorus', 'Phaser', 'Flanger', 'Tremolo')
chainEffectRows, chainEffectEvents = run_script(
    chainEffectScript, include_events=True,
    user_synth_names=chainEffectNames)
assert len(chainEffectRows) == 1 and len(chainEffectEvents) == 3
assert [row[28] for row in chainEffectEvents] == ['2', '0', '2']
assert [row[20] for row in chainEffectEvents] == ['0', '-1', '2']
assert [row[29] for row in chainEffectEvents] == ['1', '-1', '3']
assert [float(row[21]) for row in chainEffectEvents] == [0.25, 0.0, 0.4]
assert [float(row[30]) for row in chainEffectEvents] == [0.25, 0.0, 0.4]
assert 'limited to two effects' in run_compatibility_check(
    'p1 >> pluck(P[0], fx_event=P['
    '("Chorus", "Phaser", "Flanger"), None])\n', chainEffectNames)
randomEffectChains = (
    'p1 >> pluck(P[0, 2, 4, 5], fx_event=PRand(['
    '("Chorus", "Phaser"), ("Flanger", "Tremolo"), None], '
    'length=16, seed=230))\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    randomEffectChains, chainEffectNames)
_, chainEventsFirst = run_script(
    randomEffectChains, include_events=True,
    user_synth_names=chainEffectNames)
_, chainEventsAgain = run_script(
    randomEffectChains, include_events=True,
    user_synth_names=chainEffectNames)
assert [row[20:37] for row in chainEventsFirst] == [
    row[20:37] for row in chainEventsAgain]
chainEffectExample = open(os.path.join(
    sys.argv[2], '230-pbindfx-style-ordered-note-effect-chains.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    chainEffectExample, chainEffectNames)
chainExampleRows, chainExampleEvents = run_script(
    chainEffectExample, include_events=True,
    user_synth_names=chainEffectNames)
assert len(chainExampleRows) == 2 and len(chainExampleEvents) == 12
assert all(int(row[28]) <= 2 for row in chainExampleEvents)
assert 'No unsupported constructs detected' in run_compatibility_check(
    open(os.path.join(
        sys.argv[2], '227-pbindfx-style-event-effects.py'),
         encoding='utf-8').read(),
    ('Chorus', 'Flanger', 'Phaser'))
eventEffectExampleRows, eventEffectExampleEvents = run_script(
    open(os.path.join(
        sys.argv[2], '227-pbindfx-style-event-effects.py'),
         encoding='utf-8').read(),
    include_events=True, user_synth_names=('Chorus', 'Flanger', 'Phaser'))
assert len(eventEffectExampleRows) == 2
assert len(eventEffectExampleEvents) == 12
eventTailNames = (
    'JPverb', 'Greyhole', 'LimutPingPong', 'LimutShimmer',
    'LimutGatedVerb')
eventTailExample = open(os.path.join(
    sys.argv[2], '228-pbindfx-style-delay-reverb-tails.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    eventTailExample, eventTailNames)
eventTailRows, eventTailEvents = run_script(
    eventTailExample, include_events=True, user_synth_names=eventTailNames)
assert len(eventTailRows) == 2
assert len(eventTailEvents) == 12
assert [row[20] for row in eventTailEvents[:5]] == ['0', '1', '2', '3', '4']
assert [float(row[21]) for row in eventTailEvents[:4]] == [
    0.18, 0.25, 0.2, 0.28]
randomAirwindowsScript = (
    'p1 >> pluck(P[0, 4, 7, 2], '
    'fx_event=PRand(["AirwindowsTapeDelay2", "AirwindowsEchojam"], '
    'length=16, seed=21), '
    'fx_event_mix=PWhite(0.15, 0.65, length=16, seed=7))\n')
randomAirwindowsNames = ('AirwindowsTapeDelay2', 'AirwindowsEchojam')
assert 'No unsupported constructs detected' in run_compatibility_check(
    randomAirwindowsScript, randomAirwindowsNames)
randomAirwindowsRows, randomAirwindowsEvents = run_script(
    randomAirwindowsScript, include_events=True,
    user_synth_names=randomAirwindowsNames)
_, randomAirwindowsEventsAgain = run_script(
    randomAirwindowsScript, include_events=True,
    user_synth_names=randomAirwindowsNames)
assert [row[20:28] for row in randomAirwindowsEvents] == [
    row[20:28] for row in randomAirwindowsEventsAgain]
assert {row[20] for row in randomAirwindowsEvents} == {'0', '1'}
assert all(0.15 <= float(row[26]) <= 0.65 for row in randomAirwindowsEvents)
randomAirwindowsExample = open(os.path.join(
    sys.argv[2], '229-seeded-random-per-note-delays.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    randomAirwindowsExample, randomAirwindowsNames)
randomAirwindowsExampleRows, randomAirwindowsExampleEvents = run_script(
    randomAirwindowsExample, include_events=True,
    user_synth_names=randomAirwindowsNames)
assert len(randomAirwindowsExampleRows) == 2
assert len(randomAirwindowsExampleEvents) == 12
assert {row[20] for row in randomAirwindowsExampleEvents[:8]} == {'0', '1'}
assert 'seeded PRand' in run_compatibility_check(
    'p1 >> pluck(P[0], fx_event=PRand(["AirwindowsTapeDelay2", '
    '"AirwindowsEchojam"], length=257, seed=21))\n',
    randomAirwindowsNames)
assert 'seeded PRand' in run_compatibility_check(
    'p1 >> pluck(P[0], fx_event=PRand(["AirwindowsTapeDelay2", '
    '"AirwindowsEchojam"], 0, length=8, seed=21))\n',
    randomAirwindowsNames)
assert 'PRand length must be from 1 to 256' in run_error_script(
    'p1 >> pluck(P[0], fx_event=PRand(["AirwindowsTapeDelay2", '
    '"AirwindowsEchojam"], length=257, seed=1))\n',
    randomAirwindowsNames)
assert 'fx_event effect AirwindowsEchojam is not loaded' in run_error_script(
    'p1 >> pluck(P[0], fx_event=PRand(["AirwindowsTapeDelay2", '
    '"AirwindowsEchojam"], length=8, seed=21))\n',
    ('AirwindowsTapeDelay2',))
subprocess.run([sys.executable, os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    'UGens', 'SpaceReverbs', 'test-examples.py')], check=True)
_, duplicateLayerRequests, duplicateLayerEvents = run_script(
    'p1 >> play("<x><x>", dur=1)', include_samples=True, include_events=True)
assert [int(row[1]) for row in duplicateLayerRequests] == [120, 120]
assert len({int(row[3]) for row in duplicateLayerEvents}) == 2
players, requests = run_script('p1 >> play("x-o-", dur=0.5, sample=P[0, 1], rate=P[1, -1])', include_samples=True)
assert int(players[0][6]) == 57
assert [int(row[1]) for row in requests] == [120, 45, 111, 45]
assert [int(row[2]) for row in requests] == [0, 1, 0, 1]
assert [float(row[3]) for row in requests] == [1, -1, 1, -1]
assert all(row[4] == '0' for row in requests)
pvarNoteRows, pvarNoteEvents = run_script(
    'p1 >> pluck(Pvar([P[0, 2], P[4, 6]], [1, 1]), '
    'dur=0.5, sus=0.4, oct=4)\n', include_events=True)
assert int(pvarNoteRows[0][5]) == 4 and float(pvarNoteRows[0][4]) == 2.0
assert [int(row[3]) for row in pvarNoteEvents] == [48, 52, 55, 59]
pvarSampleRows, pvarSampleRequests, pvarSampleEvents = run_script(
    'p1 >> play(Pvar(["x-o-", "o--x"], [2, 2]), '
    'dur=0.5, sample=0)\n', include_samples=True, include_events=True)
assert int(pvarSampleRows[0][5]) == 8
assert [int(row[1]) for row in pvarSampleRequests] == [
    120, 45, 111, 45, 111, 45, 45, 120]
assert [float(row[1]) for row in pvarSampleEvents] == [
    0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5]
pvarVariableNoteRows, pvarVariableNoteEvents = run_script(
    'p1 >> pluck(Pvar([P[0, 2, 4, 6], P[7, 5, 3, 1]], [3, 3]), '
    'dur=P[0.5, 1.0], sus=0.4, oct=4)\n', include_events=True)
assert [float(row[1]) for row in pvarVariableNoteEvents] == [
    0.0, 0.5, 1.5, 2.0, 3.0, 3.5, 4.5, 5.0]
pvarVariableSampleRows, pvarVariableSampleRequests = run_script(
    'p1 >> play(Pvar(["x---", "--xo"], [3, 3]), '
    'dur=P[0.5, 1.0], sample=0)\n', include_samples=True)
assert int(pvarVariableSampleRows[0][5]) == 8
pvarVariableExample = run_example('137-pvar-phrase-switching.py')
assert len(pvarVariableExample) == 2
assert all(float(row[4]) == 12.0 for row in pvarVariableExample)
assert int(pvarVariableExample[0][5]) == 16
assert int(pvarVariableExample[1][5]) > 0
assert 'No unsupported constructs detected' in run_compatibility_check(
    open(os.path.join(
        sys.argv[2], '137-pvar-phrase-switching.py'), encoding='utf-8').read())
pvarTimevarNoteRows, pvarTimevarNoteEvents = run_script(
    'p1 >> pluck(Pvar([P[0, 2, 4], P[7, 5, 3]], [2, 2]), '
    'dur=var([0.5, 1.0], [1, 1]), sus=0.4, oct=4)\n',
    include_events=True)
assert [float(row[1]) for row in pvarTimevarNoteEvents] == [
    0.0, 0.5, 1.0, 2.0, 2.5, 3.0]
pvarTimevarSampleRows, pvarTimevarSampleRequests, pvarTimevarSampleEvents = (
    run_script(
        'p1 >> play(Pvar(["x-", "-o"], [1, 1]), '
        'dur=linvar([0.25, 1.25], [0.5, 0.5]), sample=0)\n',
        include_samples=True, include_events=True))
assert [float(row[1]) for row in pvarTimevarSampleEvents] == [
    0.0, 0.25, 1.0, 1.25]
assert [int(request[1]) for request in pvarTimevarSampleRequests] == [
    ord('x'), ord('-'), ord('-'), ord('o')]
pvarTimevarExample = run_example('138-pvar-timevar-durations.py')
assert len(pvarTimevarExample) == 2
assert float(pvarTimevarExample[0][4]) == 4.0
assert float(pvarTimevarExample[1][4]) == 2.0
assert int(pvarTimevarExample[0][5]) == 6
assert int(pvarTimevarExample[1][5]) == 4
assert 'No unsupported constructs detected' in run_compatibility_check(
    open(os.path.join(
        sys.argv[2], '138-pvar-timevar-durations.py'),
        encoding='utf-8').read())
pvarGroupedRows, pvarGroupedEvents = run_script(
    'p1 >> triangle(Pvar([P[0, 2], P[7, 5]], [3, 3]), '
    'dur=PRhythm([1, (3, 8)]), sus=0.2, oct=4)\n',
    include_events=True)
assert len(pvarGroupedRows) == 1 and len(pvarGroupedEvents) == 8
assert [round(float(row[1]) + float(row[7]), 6)
        for row in pvarGroupedEvents] == [
    0.0, 1.0, 1.75, 2.5, 3.0, 4.0, 4.75, 5.5]
pvarGroupedExample = run_example('139-pvar-grouped-rhythm.py')
assert len(pvarGroupedExample) == 2
assert int(pvarGroupedExample[0][5]) == 8
assert int(pvarGroupedExample[1][5]) == 8
assert 'No unsupported constructs detected' in run_compatibility_check(
    open(os.path.join(
        sys.argv[2], '139-pvar-grouped-rhythm.py'),
        encoding='utf-8').read())
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> pluck(Pvar([P[0, 2], P[4, 6]], [1, 1]), dur=0.5)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(Pvar(["x-o-", "o--x"], [2, 2]), dur=0.5)\n')
assert 'Pvar accepts only patterns and dur' in run_compatibility_check(
    'p1 >> pluck(Pvar(P[0, 2], invalid=True))\n')
assert 'Pvar cannot mix numeric note patterns with sample-symbol patterns' in (
    run_compatibility_check(
        'p1 >> pluck(Pvar([P[0, 2], P["x", "o"]], [1, 1]), dur=0.5)\n'))
_, requests = run_script('p1 >> play("x ", sample=P[0, 1, 2])', include_samples=True)
assert [int(row[2]) for row in requests] == [0, 2, 1], requests
_, requests = run_script('p1 >> play("a", dur=0.5, sus=0.1, sample=P[-1, 0], rate=P[1, -1, 2, -2])', include_samples=True)
assert [float(row[3]) for row in requests] == [1, -1, 2, -2]
assert all(row[4] == '1' for row in requests)
_, sample_notes = run_script('p1 >> play("x", dur=P[0.5, 1])', include_events=True)
assert [float(row[1]) for row in sample_notes] == [0, 0.5]
varPlayers, varRequests, varControls = run_script(
    'p1 >> play("x-o-", sample=var([0, 1], [2, 2]), rate=var([1, -1], 4))',
    include_sample_controls=True)
assert len(varRequests) == 4
assert varControls == [['Q', '0', 'v:0.0,1.0:2.0,2.0', 'v:1.0,-1.0:4.0,4.0']], varControls
linPlayers, linRequests, linControls = run_script(
    'p1 >> play("x", sample=linvar([0, 1], 4), rate=linvar([0.75, 1.5], 4))',
    include_sample_controls=True)
assert linControls == [['Q', '0', 'l:0.0,1.0:4.0,4.0', 'l:0.75,1.5:4.0,4.0']], linControls
assert float(linRequests[0][3]) == 0.75
for bad in ('rate=0', 'rate=9', 'sample=0.5', 'sample=[]', 'rate=[]',
            'sample=var([0, 0.5], 4)', 'sample=linvar([0, 1.5], 4)',
            'rate=var([0.01, 1], 4)', 'rate=linvar([-1, 1], 4)'):
    failure = run_error_script('p1 >> play("x", ' + bad + ')')
    assert failure.startswith('ERROR\n') and 'ValueError' in failure, (bad, failure)
_, slicedRequests = run_script(
    'p1 >> play("x-o", start=0.2, end=0.65, loop=False)', include_samples=True)
assert all(float(row[5]) == 0.2 and float(row[6]) == 0.65 and row[7] == '0'
           for row in slicedRequests)
_, patternSliceRequests = run_script(
    'p1 >> play("x-o-", start=P[0.0, 0.2], end=P[0.5, 0.9])',
    include_samples=True)
assert [float(row[5]) for row in patternSliceRequests] == [0.0, 0.2, 0.0, 0.2]
assert [float(row[6]) for row in patternSliceRequests] == [0.5, 0.9, 0.5, 0.9]
slicePattern256 = 'P[' + ','.join(['0.0'] * 256) + ']'
_, maxSliceRequests = run_script(
    'p1 >> play("x", start=' + slicePattern256 + ', end=1.0)',
    include_samples=True)
assert len(maxSliceRequests) == 256
_, expandedSliceRequests = run_script(
    'p1 >> play("x", start=P[0.0, 0.1, 0.2], end=0.8)',
    include_samples=True)
assert [float(row[5]) for row in expandedSliceRequests] == [0.0, 0.1, 0.2]
_, timevarSliceRequests = run_script(
    'p1 >> play("x-o-", start=var([0.0, 0.2], 2), '
    'end=var([0.6, 1.0], 2))', include_samples=True)
assert [float(row[5]) for row in timevarSliceRequests] == [0.0, 0.0, 0.2, 0.2]
assert [float(row[6]) for row in timevarSliceRequests] == [0.6, 0.6, 1.0, 1.0]
_, linearSliceRequests = run_script(
    'p1 >> play("x-o-", start=linvar([0.1, 0.3], 4), '
    'end=linvar([0.7, 0.9], 4))', include_samples=True)
assert [round(float(row[5]), 8) for row in linearSliceRequests] == [
    0.1, 0.15, 0.2, 0.25]
assert [round(float(row[6]), 8) for row in linearSliceRequests] == [
    0.7, 0.75, 0.8, 0.85]
_, loopRequests = run_script(
    'p1 >> play("x", dur=2, loop=True)', include_samples=True)
assert loopRequests[0][4:] == ['1', '0.0', '1.0', '1']
for bad in ('start=-0.1', 'end=1.1', 'start=0.5, end=0.5',
            'start=float("nan")', 'start=[]', 'start=P[0.0, 1.1]',
            'start=[0.0]*257',
            'start=P[0.2, 0.8], end=P[0.7, 0.6]',
            'start=var([0.8, 0.2], 2), end=var([0.6, 1.0], 2)',
            'loop=1'):
    failure = run_error_script('p1 >> play("x", ' + bad + ')')
    assert failure.startswith('ERROR\n') and 'ValueError' in failure, (bad, failure)
_, nestedRequests, nestedEvents = run_script(
    'p1 >> play("x[--]o", dur=1)', include_samples=True, include_events=True)
assert [int(row[1]) for row in nestedRequests] == [120, 45, 45, 111]
assert [float(row[1]) for row in nestedEvents] == [0.0, 1.0, 1.5, 2.0]
_, lacedRequests, lacedEvents = run_script(
    'p1 >> play("(xo)---", dur=1)', include_samples=True, include_events=True)
assert [int(row[1]) for row in lacedRequests] == [120, 45, 45, 45, 111, 45, 45, 45]
assert [float(row[1]) for row in lacedEvents] == [float(i) for i in range(8)]
_, layeredRequests, layeredEvents = run_script(
    'p1 >> play("<x  ><-  >", dur=1)', include_samples=True, include_events=True)
assert [int(row[1]) for row in layeredRequests] == [120, 45]
assert [float(row[1]) for row in layeredEvents] == [0.0, 0.0]
_, randomRequests, randomEvents = run_script(
    'p1 >> play("{xo}", dur=1)', include_samples=True, include_events=True)
assert len(randomRequests) == 8 and len(randomEvents) == 8
assert len({int(row[1]) for row in randomRequests}) == 2
_, sampleIndexRequests = run_script(
    'p1 >> play("|x1|x|x0|x", sample=P[0, 1])', include_samples=True)
assert [int(row[1]) for row in sampleIndexRequests] == [120, 120, 120, 120]
assert [int(row[2]) for row in sampleIndexRequests] == [1, 1, 0, 1]
_, zippedSampleRequests, zippedSampleEvents = run_script(
    'p1 >> play(P["x", " "] & P["o", "-", " "], dur=0.5, sample=0)',
    include_samples=True, include_events=True)
assert [int(row[1]) for row in zippedSampleRequests] == [
    120, 111, 45, 120, 111, 120, 45], zippedSampleRequests
assert [float(row[1]) for row in zippedSampleEvents] == [
    0.0, 0.0, 0.5, 1.0, 1.5, 2.0, 2.0], zippedSampleEvents
_, nestedSampleIndexRequests, nestedSampleIndexEvents = run_script(
    'p1 >> play("[|x1|x]o", dur=1, sample=0)',
    include_samples=True, include_events=True)
assert [int(row[2]) for row in nestedSampleIndexRequests] == [1, 0, 0]
assert [float(row[1]) for row in nestedSampleIndexEvents] == [0.0, 0.5, 1.0]
_, reverseSampleRequests = run_script(
    'p1 >> play(P["x", "o", " "].reverse(), sample=0)', include_samples=True)
assert [int(row[1]) for row in reverseSampleRequests] == [111, 120]
_, rotateSampleRequests = run_script(
    'p1 >> play(P["x", "o", " "].rotate(1), sample=0)', include_samples=True)
assert [int(row[1]) for row in rotateSampleRequests] == [111, 120]
_, repeatSampleRequests = run_script(
    'p1 >> play(P["x", "o"].repeat(2), sample=0)', include_samples=True)
assert [int(row[1]) for row in repeatSampleRequests] == [120, 111, 120, 111]
failure = run_error_script('p1 >> play(P["x", "o"] + P["a"])')
assert failure.startswith('ERROR\n') and 'TypeError' in failure
for badPattern in ('P["xx", "o"]', 'P[1, "x"]'):
    failure = run_error_script('p1 >> play(' + badPattern + ')')
    assert failure.startswith('ERROR\n') and 'TypeError' in failure, (badPattern, failure)
for badSelector in ('|x|', '|x10|', '|.1|', '|x1'):
    failure = run_error_script('p1 >> play("' + badSelector + '")')
    assert failure.startswith('ERROR\n') and 'ValueError' in failure, (badSelector, failure)
malformed_string = run_compatibility_check('p1 >> play("x[--")')
assert 'missing closing delimiter' in malformed_string
assert 'empty group' in run_compatibility_check('p1 >> play("x[]")')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("|x1|x|a1|a", sample=0)')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(P["x", "o", " "].reverse(), sample=P[0, 1])')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(P["x", " "] | P["o", "-"], sample=P[0, 1])')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(P["x"] & P["o"])')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play((P["x", " "] & P["o", "-"]).rotate(1))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(P["x"] & P[0])')
assert 'requires two finite P sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(P["x"] & "o")')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(P["x"] | P["o"])')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PAlt(P["x", " "], P["o", "-"]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PAlt(P[0, 1], P[4, 5]))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(PAlt(P[0, 1], P["x"]))')
assert 'at least two finite patterns' in run_compatibility_check(
    'p1 >> play(PAlt(P["x"]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PStutter(PStretch(P["x", "o", " "], 6), P[1, 2]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PShuf(P["x", "o", " "]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PShuf(P[0, 1]))')
assert 'PShuf requires exactly one finite pattern' in run_compatibility_check(
    'p1 >> play(PShuf(P["x"], P["o"]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PShuf(PJoin([P["x", " "], P["o", "-"]])))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PShuf(PJoin([P[0, 2], P[4, 7]])))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(PJoin([P[0, 1], P["x"]]))')
assert 'PJoin requires one list or pattern' in run_compatibility_check(
    'p1 >> play(PJoin())')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("x", dur=PBeat("x xx", start=1, dur=0.25))\n'
    'p2 >> play("o", dur=PDur(3, 8))')
assert 'PBeat expects a pulse string' in run_compatibility_check(
    'p1 >> play("x", dur=PBeat())')
assert 'PDur expects pulse and step counts' in run_compatibility_check(
    'p1 >> play("x", dur=PDur(3))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw(P[0, 2, 4], dur=PStrum(2), delay=PDelay(3, 8))')
assert 'PDelay expects pulse and step counts' in run_compatibility_check(
    'p1 >> saw([0], delay=PDelay(3))')
assert 'PStrum accepts one optional subdivision count' in run_compatibility_check(
    'p1 >> saw([0], dur=PStrum(2, 3))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw([0, 2], dur=PRhythm([1, (3, 8)]), sus=0.1)\n'
    'p2 >> sine([0, 2], delay=PQuicken(0.35, stepsize=2, steps=4))')
assert 'PQuicken accepts dur, stepsize, and steps' in run_compatibility_check(
    'p1 >> saw([0], delay=PQuicken(0.5, 2, 4, 5))')
assert 'PQuicken supports only dur, stepsize, and steps keywords' in run_compatibility_check(
    'p1 >> saw([0], delay=PQuicken(speed=2))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> saw(PZip(P[0, 2], P[4, 5]), dur=1, sus=0.5)\n'
    'p2 >> sine(PZip2(P[0, 1], P[2, 3], rule=lambda a, b: a != b))')
numericChordZipText = (
    'p1 >> pluck(PZip(P[PGroup(0, 4, 7), None], '
    'P[PGroup(12, 16), PGroup(14, 19)]), dur=1, sus=0.7, oct=4)\n'
    'p2 >> triangle(PZip2(P[PGroup(0, 3), PGroup(2, 5)], '
    'P[PGroup(7, 11), PGroup(9, 12)], rule=lambda a, b: a != b), '
    'dur=1.5, sus=1.2, oct=3)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    numericChordZipText)
numericChordZipRows, numericChordZipEvents = run_script(
    numericChordZipText, include_events=True)
assert len(numericChordZipRows) == 2
assert len(numericChordZipEvents) == 15
assert all(int(event[3]) >= 0 for event in numericChordZipEvents)
assert sorted(float(event[1]) for event in numericChordZipEvents) == (
    [0.0] * 9 + [1.0] * 2 + [1.5] * 4)
assert list(namespace['PZip'](
    namespace['P'][namespace['PGroup'](0, 4, 7), None],
    namespace['P'][namespace['PGroup'](12, 16), namespace['PGroup'](14, 19)]
)) == [
    (namespace['PGroup'](0, 4, 7), namespace['PGroup'](12, 16)),
    (None, namespace['PGroup'](14, 19))]
assert list(namespace['PZip2'](
    namespace['P'][namespace['PGroup'](0, 4), namespace['PGroup'](2, 5)],
    namespace['P'][namespace['PGroup'](7, 11), namespace['PGroup'](9, 12)],
    rule=lambda a, b: a != b
)) == [
    (namespace['PGroup'](0, 4), namespace['PGroup'](7, 11)),
    (namespace['PGroup'](2, 5), namespace['PGroup'](9, 12))]
for chord_zip in (
        lambda: namespace['PZip'](
            namespace['P'][namespace['PGroup'](0, 1, 2, 3, 4),],
            namespace['P'][namespace['PGroup'](5, 6, 7, 8, 9),]),
        lambda: namespace['PZip2'](
            namespace['P'][namespace['PGroup'](0, 1, 2, 3, 4),],
            namespace['P'][namespace['PGroup'](5, 6, 7, 8, 9),])):
    try:
        chord_zip()
    except ValueError as error:
        assert 'limited to 8 notes' in str(error)
    else:
        raise AssertionError('PZip accepted a chord over the eight-note bound')
numericChordZipExample = run_example('140-numeric-pzip-chord-groups.py')
assert len(numericChordZipExample) == 2
assert 'No unsupported constructs detected' in run_compatibility_check(
    open(os.path.join(
        sys.argv[2], '140-numeric-pzip-chord-groups.py'),
        encoding='utf-8').read())
assert 'PZip requires 2 to 8 finite numeric or sample-symbol patterns' in run_compatibility_check(
    'p1 >> saw(PZip(P[0]))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play(PZip(P["x", " "], P["o", "-"]))\n'
    'p2 >> play(PZip2(P["x", "-"], P["o", " "], rule=lambda a, b: a != b))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(PZip(P["x", "o"], P[0, 1]))')
assert 'cannot mix numeric note patterns with sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(PZip2(P["x"], P[0]))')
assert 'PRhythm expects one finite list or P pattern' in run_compatibility_check(
    'p1 >> saw([0], dur=PRhythm())')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PStretch(P[0, 1], 6))')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine(PStutter(P[0, 1], P[1, 2]))')
assert 'PStretch requires a finite pattern and a target length' in run_compatibility_check(
    'p1 >> play(PStretch(P["x"]))')
assert 'PStutter requires a finite pattern and optional repeat counts' in run_compatibility_check(
    'p1 >> play(PStutter())')
assert 'P arithmetic over sample-symbol patterns' in run_compatibility_check(
    'p1 >> play(P["x", "o"] + P["a"])')
assert 'P sample-symbol literals' in run_compatibility_check('p1 >> play(P["xx", "o"])')
for badSelector in ('|x|', '|x10|', '|.1|', '|x1'):
    assert 'sample-index selector' in run_compatibility_check(
        'p1 >> play("' + badSelector + '")')
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> play("x[--]o(=[-o])")')
_, composedSampleRequests = run_script(
    'p1 >> play((P["x", " ", "x", " "] | P["o", "-", " ", "-"]).rotate(2), dur=0.5, sample=P[0, 1])',
    include_samples=True)
assert [int(row[1]) for row in composedSampleRequests] == [120, 111, 45, 45, 120]
assert [int(row[2]) for row in composedSampleRequests] == [0, 0, 1, 1, 0]
sampleZipText = (
    'p1 >> play(PZip(P["x", " "], P["o", "-", " "]), dur=1, sample=0)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(sampleZipText)
sampleZipRows, sampleZipRequests, sampleZipEvents = run_script(
    sampleZipText, include_samples=True, include_events=True)
assert len(sampleZipRows) == 1 and len(sampleZipRequests) == 7
assert sorted(int(row[1]) for row in sampleZipRequests) == [
    45, 45, 111, 111, 120, 120, 120]
assert len(sampleZipEvents) == 7
example92Text = open(
    os.path.join(sys.argv[2], '92-sample-pzip-pzip2.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example92Text)
example92Rows, example92Requests, example92Events = run_script(
    example92Text, include_samples=True, include_events=True)
assert len(example92Rows) == 2 and len(example92Requests) == 35
assert len(example92Events) == 35
example93Text = open(
    os.path.join(sys.argv[2], '93-clock-swing.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example93Text)
example93Rows = run_script(example93Text)
assert len(example93Rows) == 2
assert all(abs(float(row[22]) - 0.04) < 1e-9 for row in example93Rows)
example78Rows, example78Requests = run_script(
    open(os.path.join(sys.argv[2], '78-sample-pattern-concatenation.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example78Rows) == 1 and len(example78Requests) == 5
example79Rows, example79Requests = run_script(
    open(os.path.join(sys.argv[2], '79-sample-pattern-alternation.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example79Rows) == 1 and [int(row[1]) for row in example79Requests] == [
    120, 111, 45, 120, 111, 120, 45]
example80Rows, example80Requests = run_script(
    open(os.path.join(sys.argv[2], '80-sample-pattern-stretch-stutter.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example80Rows) == 1 and [int(row[1]) for row in example80Requests] == [
    120, 111, 111, 120, 120, 111]
example81Rows, example81Requests = run_script(
    open(os.path.join(sys.argv[2], '81-sample-pattern-shuffle.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example81Rows) == 1 and sorted(int(row[1]) for row in example81Requests) == [
    45, 111, 111, 120, 120]
example82Rows, example82Requests = run_script(
    open(os.path.join(sys.argv[2], '82-sample-pattern-join-shuffle.py'), encoding='utf-8').read(),
    include_samples=True)
assert len(example82Rows) == 1 and sorted(int(row[1]) for row in example82Requests) == [
    45, 111, 120, 120]
example83Rows, example83Requests, example83Events = run_script(
    open(os.path.join(sys.argv[2], '83-pbeat-pdur-rhythm-durations.py'), encoding='utf-8').read(),
    include_samples=True, include_events=True)
assert len(example83Rows) == 2 and len(example83Requests) == 7 and len(example83Events) == 7
assert sorted(round(float(row[1]), 6) for row in example83Events) == [
    0.0, 0.0, 0.5, 0.75, 1.25, 1.5, 1.5]
example84Rows, example84Events = run_script(
    open(os.path.join(sys.argv[2], '84-pdelay-pstrum-combination.py'), encoding='utf-8').read(),
    include_events=True)
assert len(example84Rows) == 1 and len(example84Events) == 36
assert [round(float(event[7]), 6) for event in example84Events] == [
    0.0, 0.75, 1.5] * 12
assert [round(float(event[1]), 6) for event in example84Events] == [
    beat for beat in (0.0, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 5.0, 5.5, 6.0, 6.5, 7.0)
    for _ in range(3)]
rhythmGroupRows, rhythmGroupEvents = run_script(
    'p1 >> triangle(P[0, 2], dur=PRhythm([1, (3, 8)]), sus=0.1)\n',
    include_events=True)
assert len(rhythmGroupRows) == 1 and len(rhythmGroupEvents) == 4
assert [round(float(event[1]), 6) for event in rhythmGroupEvents] == [
    0.0, 1.0, 1.0, 1.0]
assert [round(float(event[7]), 6) for event in rhythmGroupEvents] == [
    0.0, 0.0, 0.75, 1.5]
quickenRows, quickenEvents = run_script(
    'p1 >> sine(P[0, 2], dur=1, sus=0.02, delay=PQuicken(0.4, 2, 3))\n',
    include_events=True)
assert len(quickenRows) == 1 and len(quickenEvents) == 6
assert [round(float(event[7]), 6) for event in quickenEvents] == [
    0.0, 0.2, 0.3, 0.0, 0.2, 0.3]
zipRows, zipEvents = run_script(
    'p1 >> sine(PZip(P[None, 2], P[3, 4]), dur=1, sus=0.5)\n',
    include_events=True)
assert len(zipRows) == 1 and len(zipEvents) == 3
example86Rows, example86Events = run_script(
    open(os.path.join(sys.argv[2], '86-pzip-pzip2-chords.py'), encoding='utf-8').read(),
    include_events=True)
assert len(example86Rows) == 2 and len(example86Events) == 28
overlapExampleText = open(
    os.path.join(sys.argv[2], '87-overlapping-sustain-voices.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(overlapExampleText)
overlapRows, overlapEvents = run_script(overlapExampleText, include_events=True)
assert len(overlapRows) == 2 and len(overlapEvents) == 10
assert [float(event[2]) for event in overlapEvents[:4]] == [1.0] * 4
assert [float(event[2]) for event in overlapEvents[4:]] == [1.5] * 6
assert_player_polyphony_ok(overlapRows[0], overlapEvents[:4])
assert_player_polyphony_ok(overlapRows[1], overlapEvents[4:])
example85Rows, example85Requests, example85Events = run_script(
    open(os.path.join(sys.argv[2], '85-pquicken-prhythm.py'), encoding='utf-8').read(),
    include_samples=True, include_events=True)
assert len(example85Rows) == 2 and len(example85Requests) == 16
assert len(example85Events) == 32
assert sorted(round(float(event[7]), 6) for event in example85Events
              if abs(float(event[2]) - 0.02) < 1e-9) == [
    0.0, 0.0, 0.0, 0.0, 0.175, 0.175, 0.175, 0.175,
    0.2625, 0.2625, 0.2625, 0.2625,
    0.30625, 0.30625, 0.30625, 0.30625]
example89Text = open(
    os.path.join(sys.argv[2], '89-nested-prhythm.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(example89Text)
example89Rows, example89Events = run_script(example89Text, include_events=True)
assert len(example89Rows) == 1 and len(example89Events) == 6
assert [round(float(event[1]) + float(event[7]), 6)
        for event in example89Events] == [
    0.0, 0.75, 1.0, 1.25, 2.0, 2.75]
assert [round(float(event[2]), 6) for event in example89Events] == [
    0.35] * 6
assert_player_polyphony_ok(example89Rows[0], example89Events)
groupedSustainText = (
    'p1 >> triangle(P[PGroup(0, 2, 4), PGroup(4, 6)], dur=1, '
    'sus=P[PGroup(0.25, 0.45), PGroup(0.35, 0.65)], oct=4, amp=0.14)\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupedSustainText)
groupedSustainRows, groupedSustainEvents = run_script(
    groupedSustainText, include_events=True)
assert len(groupedSustainRows) == 1 and len(groupedSustainEvents) == 5
assert [round(float(event[1]), 6) for event in groupedSustainEvents] == [
    0.0, 0.0, 0.0, 1.0, 1.0]
assert [round(float(event[2]), 6) for event in groupedSustainEvents] == [
    0.25, 0.45, 0.25, 0.35, 0.65]
assert_player_polyphony_ok(groupedSustainRows[0], groupedSustainEvents)
groupedRhythmText = (
    'p1 >> triangle(P[PGroup(0, 2, 4), PGroup(0, 2, 4)], '
    'dur=PRhythm([1, (3, 8)]), sus=PGroup(0.2, 0.4, 0.6))\n')
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupedRhythmText)
groupedRhythmRows, groupedRhythmEvents = run_script(
    groupedRhythmText, include_events=True)
assert len(groupedRhythmRows) == 1 and len(groupedRhythmEvents) == 6
assert [round(float(event[7]), 6) for event in groupedRhythmEvents] == [
    0.0, 0.0, 0.0, 0.0, 0.75, 1.5]
assert [round(float(event[2]), 6) for event in groupedRhythmEvents] == [
    0.2, 0.4, 0.6, 0.2, 0.4, 0.6]
sampleGroupedSustainRows, sampleGroupedSustainRequests, sampleGroupedSustainEvents = run_script(
    'p1 >> play("x o ", dur=1, sus=PGroup(0.25, 0.5), sample=0)\n',
    include_samples=True, include_events=True)
assert len(sampleGroupedSustainRows) == 1
assert len(sampleGroupedSustainRequests) == len(sampleGroupedSustainEvents) == 4
assert [int(row[1]) for row in sampleGroupedSustainRequests] == [
    120, 120, 111, 111]
assert [round(float(event[2]), 6) for event in sampleGroupedSustainEvents] == [
    0.25, 0.5, 0.25, 0.5]
badGroupedSustain = run_error_script(
    'p1 >> sine([0], dur=1, sus=PGroup(0, 0.5))\n')
assert 'each sus value must be finite and at least 0.02 beats' in badGroupedSustain
tooManyGroupedEvents = run_error_script(
    'p1 >> sine(P[0:33], dur=1, '
    'sus=PGroup(0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09))\n')
assert 'limited to 256 note events' in tooManyGroupedEvents
groupedSustainPolyphony = run_error_script(
    'p1 >> sine(P[0, 2], dur=1, '
    'sus=PGroup(1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5))\n')
assert 'pattern exceeds 8-note polyphony' in groupedSustainPolyphony
example90Text = open(
    os.path.join(sys.argv[2], '90-grouped-sustain.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    example90Text)
example90Rows, example90Requests, example90Events = run_script(
    example90Text, include_samples=True, include_events=True)
assert len(example90Rows) == 2 and len(example90Requests) == 4
assert len(example90Events) == 9
assert [round(float(event[2]), 6) for event in example90Events[:5]] == [
    0.25, 0.45, 0.25, 0.35, 0.65]
assert [round(float(event[1]), 6) for event in example90Events[5:]] == [
    0.0, 0.0, 2.0, 2.0]
assert [round(float(event[2]), 6) for event in example90Events[5:]] == [
    0.25, 0.5, 0.25, 0.5]
assert_player_polyphony_ok(example90Rows[0], example90Events[:5])
numericTransformText = open(
    os.path.join(sys.argv[2], '88-numeric-pattern-transforms.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(numericTransformText)
numericTransformRows, numericTransformEvents = run_script(
    numericTransformText, include_events=True)
assert len(numericTransformRows) == 3 and len(numericTransformEvents) == 30
eventOffset = 0
for row, eventCount in zip(numericTransformRows, (12, 6, 12)):
    playerEvents = numericTransformEvents[eventOffset:eventOffset + eventCount]
    eventOffset += eventCount
    assert_player_polyphony_ok(row, playerEvents)
playerFollowText = open(
    os.path.join(sys.argv[2], '109-player-degree-follow.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    playerFollowText)
followRows, followEvents = run_script(playerFollowText, include_events=True)
assert len(followRows) == 3 and len(followEvents) == 24
def player_event_pitches(rows, events, player_index):
    first = sum(int(row[5]) for row in rows[:player_index])
    count = int(rows[player_index][5])
    return [int(event[3]) for event in events[first:first + count]]
assert player_event_pitches(followRows, followEvents, 0) == player_event_pitches(
    followRows, followEvents, 1)
assert player_event_pitches(followRows, followEvents, 0) == player_event_pitches(
    followRows, followEvents, 2)
assert float(followRows[1][7]) == 0.32 and float(followRows[1][8]) == 0.2
assert float(followRows[2][7]) == 0.12 and float(followRows[2][8]) == 0.35
referenceEditRows, referenceEditEvents = run_script(
    'p2 >> bass([0, 4, 2, 5], dur=1, oct=3, amp=0.32, pan=0.2)\n'
    'p1 >> sine([0, 2, 4, 6], dur=0.5, amp=0.2)\n'
    'p2.follow(p1)\n'
    'p3 >> triangle([6, 2, 0, 4], dur=0.5, oct=5, amp=0.12, pan=0.35)\n'
    'p3.degree = p1.degree\n'
    'p1 >> pluck([1, 3, 5, 7], dur=0.5, oct=5, amp=0.24)\n',
    include_events=True)
assert len(referenceEditRows) == 3 and len(referenceEditEvents) == 12
sourcePitches = player_event_pitches(referenceEditRows, referenceEditEvents, 0)
assert player_event_pitches(referenceEditRows, referenceEditEvents, 1) == sourcePitches
assert player_event_pitches(referenceEditRows, referenceEditEvents, 2) == sourcePitches
assert float(referenceEditRows[1][7]) == 0.32
assert float(referenceEditRows[1][8]) == 0.2
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\np1.every(2, "reverse")\n')
reverseRows, reverseEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.every(2, "reverse")\n',
    include_events=True)
assert len(reverseRows) == 1 and len(reverseEvents) == 8
assert float(reverseRows[0][4]) == 4.0
assert [int(event[3]) for event in reverseEvents] == [
    60, 62, 64, 65, 65, 64, 62, 60]
rotateRows, rotateEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.every(2, "rotate", 1)\n',
    include_events=True)
assert len(rotateRows) == 1 and len(rotateEvents) == 16
assert [int(event[3]) for event in rotateEvents[:12]] == [
    60, 62, 64, 65, 65, 60, 62, 64, 64, 65, 60, 62]
cycleReverseRows, cycleReverseEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(1, "reverse", cycle=4, ident="phrase")\n',
    include_events=True)
assert len(cycleReverseRows) == 1 and len(cycleReverseEvents) == 16
assert float(cycleReverseRows[0][4]) == 8.0
assert [int(event[3]) for event in cycleReverseEvents[:8]] == [
    60, 62, 62, 60, 65, 64, 62, 60]
identifiedStutterRows, identifiedStutterEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(0.5, "stutter", 2, cycle=2, ident="a")\n'
    'p1.every(1.5, "stutter", 2, cycle=2, ident="b")\n',
    include_events=True)
assert len(identifiedStutterRows) == 1
assert float(identifiedStutterRows[0][4]) == 2.0
assert len(identifiedStutterEvents) == 6
sameIdentRows, sameIdentEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(1, "reverse", cycle=4, ident="same")\n'
    'p1.every(3, "reverse", cycle=4, ident="same")\n',
    include_events=True)
lastIdentRows, lastIdentEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(3, "reverse", cycle=4, ident="same")\n',
    include_events=True)
assert sameIdentRows == lastIdentRows
assert sameIdentEvents == lastIdentEvents
cycleOftenRows, cycleOftenEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.often("shuffle", cycle=8, ident="variation")\n',
    include_events=True)
assert len(cycleOftenRows) == 1 and len(cycleOftenEvents) == 64
cycleSometimesRows, cycleSometimesEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.sometimes("shuffle", cycle=16, ident="variation")\n',
    include_events=True)
assert len(cycleSometimesEvents) == 128
cycleRarelyRows, cycleRarelyEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.rarely("shuffle", cycle=32, ident="variation")\n',
    include_events=True)
assert len(cycleRarelyEvents) == 256
cycleIdentText = open(
    os.path.join(sys.argv[2], '114-cycle-ident-schedules.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    cycleIdentText)
cycleIdentRows, cycleIdentEvents = run_script(
    cycleIdentText, include_events=True)
assert len(cycleIdentRows) == 1 and 4 <= len(cycleIdentEvents) <= 256
assert_player_polyphony_ok(cycleIdentRows[0], cycleIdentEvents)
for scheduleSeed in range(16):
    seededCycleRows, seededCycleEvents = run_script_with_schedule_seed(
        cycleIdentText, scheduleSeed)
    assert len(seededCycleRows) == 1
    assert 4 <= len(seededCycleEvents) <= 256
    assert_player_polyphony_ok(seededCycleRows[0], seededCycleEvents)
sometimesText = open(
    os.path.join(sys.argv[2], '110-every-sometimes-players.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    sometimesText)
sometimesRows, sometimesEvents = run_script(
    sometimesText, include_events=True)
assert len(sometimesRows) == 2
assert 2 < len(sometimesEvents) <= 256
assert int(sometimesRows[1][5]) <= 256
assert 8.0 <= float(sometimesRows[1][4]) <= 128.0
shuffleRows, shuffleEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.every(1, "shuffle")\n',
    include_events=True)
assert len(shuffleRows) == 1 and len(shuffleEvents) == 4
assert set(int(event[3]) for event in shuffleEvents) <= {60, 62, 64, 65}
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.often("shuffle")\n')
oftenRows, oftenEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.often("shuffle")\n',
    include_events=True)
assert len(oftenRows) == 1 and 4 <= len(oftenEvents) <= 256
assert 'No unsupported constructs detected' in run_compatibility_check(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.rarely("reverse")\n')
rarelyRows, rarelyEvents = run_script(
    'p1 >> sine([0, 1], dur=0.25)\np1.rarely("reverse")\n',
    include_events=True)
assert len(rarelyRows) == 1 and 4 <= len(rarelyEvents) <= 256
stutterRows, stutterEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5, sus=0.4)\n'
    'p1.every(1, "stutter", 3)\n',
    include_events=True)
assert len(stutterRows) == 1 and len(stutterEvents) == 8
assert sorted(int(event[3]) for event in stutterEvents) == [
    60, 60, 60, 62, 64, 64, 64, 65]
assert_player_polyphony_ok(stutterRows[0], stutterEvents)
defaultStutterRows, defaultStutterEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5, sus=0.4)\n'
    'p1.every(1, "stutter")\n',
    include_events=True)
assert len(defaultStutterRows) == 1 and len(defaultStutterEvents) == 6
stackedRows, stackedEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5, sus=0.4)\n'
    'p1.every(1, "stutter", 3)\np1.every(2, "reverse")\n',
    include_events=True)
assert len(stackedRows) == 1 and len(stackedEvents) <= 256
assert_player_polyphony_ok(stackedRows[0], stackedEvents)
singleUpdatedRows, singleUpdatedEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\np1.every(1, "reverse")\n',
    include_events=True)
replacedScheduleRows, replacedScheduleEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p1.every(2, "reverse")\np1.every(1, "reverse")\n',
    include_events=True)
assert replacedScheduleRows == singleUpdatedRows
assert replacedScheduleEvents == singleUpdatedEvents
sampleStutterRows, sampleStutterRequests, sampleStutterEvents = run_script(
    'p1 >> play("x-o-", dur=0.5, sample=0, amp=0.3)\n'
    'p1.every(1, "stutter", 3)\n',
    include_samples=True, include_events=True)
assert len(sampleStutterRows) == 1
assert [int(row[1]) for row in sampleStutterRequests] == [
    120, 120, 120, 45, 111, 111, 111, 45], (
        sampleStutterRequests, sampleStutterEvents)
assert [round(float(row[1]), 6) for row in sampleStutterEvents] == [
    0.0, 0.166667, 0.333333, 0.5, 1.0, 1.166667, 1.333333, 1.5]
_, restStutterRequests, restStutterEvents = run_script(
    'p1 >> play("x o ", dur=0.5, sample=0)\n'
    'p1.every(0.5, "stutter", 3)\n',
    include_samples=True, include_events=True)
assert len(restStutterRequests) == 6
assert len(restStutterEvents) == 6
_, identifiedSampleRequests, identifiedSampleEvents = run_script(
    'p1 >> play("x-o-", dur=0.5, sample=0)\n'
    'p1.every(0.5, "stutter", 2, cycle=2, ident="first")\n'
    'p1.every(1.5, "stutter", 2, cycle=2, ident="second")\n',
    include_samples=True, include_events=True)
assert len(identifiedSampleRequests) == len(identifiedSampleEvents) == 6
assert 'Sample players currently support scheduled stutter only' in run_error_script(
    'p1 >> play("x-o-", dur=0.5)\np1.every(1, "shuffle")\n')
assert 'unstrummed hits without per-note delay' in run_error_script(
    'p1 >> play("x-o-", dur=0.5, strum=0.05)\n'
    'p1.every(1, "stutter", 3)\n')
scheduleShortcutText = open(
    os.path.join(sys.argv[2], '111-player-schedule-shortcuts.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    scheduleShortcutText)
scheduleRows, scheduleEvents = run_script(
    scheduleShortcutText, include_events=True)
assert len(scheduleRows) == 3 and len(scheduleEvents) <= 256
assert int(scheduleRows[2][5]) > 4
assert_player_polyphony_ok(scheduleRows[2], scheduleEvents[
    int(scheduleRows[0][5]) + int(scheduleRows[1][5]):])
for scheduleSeed in range(16):
    seededRows, seededEvents = run_script_with_schedule_seed(
        scheduleShortcutText, scheduleSeed)
    assert len(seededRows) == 3 and len(seededEvents) <= 256
    assert all(int(row[5]) <= 256 for row in seededRows)
drumStutterText = open(
    os.path.join(sys.argv[2], '112-drum-sample-stutter.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    drumStutterText)
drumStutterRows, drumStutterRequests, drumStutterEvents = run_script(
    drumStutterText, include_samples=True, include_events=True)
assert len(drumStutterRows) == 8 and len(drumStutterRequests) == 8, (
    drumStutterRows, drumStutterRequests, drumStutterEvents)
assert drumStutterRows[0][2] == '0' and drumStutterRows[0][4] == '2.0'
assert drumStutterRows[0][5] == '8'
assert [int(row[1]) for row in drumStutterRequests].count(120) == 3
assert [int(row[1]) for row in drumStutterRequests].count(111) == 3
assert len(drumStutterEvents) == 8
stackedScheduleText = open(
    os.path.join(sys.argv[2], '113-stacked-player-schedules.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    stackedScheduleText)
stackedScheduleRows, stackedScheduleEvents = run_script(
    stackedScheduleText, include_events=True)
assert len(stackedScheduleRows) == 1
assert 4 <= len(stackedScheduleEvents) <= 256
assert_player_polyphony_ok(stackedScheduleRows[0], stackedScheduleEvents)
for scheduleSeed in range(16):
    seededDrumRows, seededDrumRequests, seededDrumEvents = (
        run_script_with_schedule_seed(
            drumStutterText, scheduleSeed, include_samples=True))
    assert len(seededDrumRequests) == len(seededDrumEvents) <= 256
for scheduleSeed in range(16):
    seededStackRows, seededStackEvents = run_script_with_schedule_seed(
        stackedScheduleText, scheduleSeed)
    assert len(seededStackRows) == 1
    assert 4 <= len(seededStackEvents) <= 256
sampleOftenStutter = (
    'p1 >> play("x-o-", dur=0.5, sample=0)\n'
    'p1.often("stutter", 3)\n')
for scheduleSeed in range(16):
    seededOftenRows, seededOftenRequests, seededOftenEvents = (
        run_script_with_schedule_seed(
            sampleOftenStutter, scheduleSeed, include_samples=True))
    assert len(seededOftenRequests) == len(seededOftenEvents) <= 256
sampleStutterOverflow = run_error_script(
    'p1 >> play("xxxxxxxxxxxxxxxxxxxx", dur=0.05)\n'
    'p1.every(0.05, "stutter", 16)\n')
assert 'scheduled stutter expands beyond the 256-event limit' in (
    sampleStutterOverflow.lower()), sampleStutterOverflow
assert 'Scheduled player operations currently need a dense' in run_error_script(
    'p1 >> sine([0, None, 2, None], dur=0.5)\n'
    'p1.every(2, "reverse")\n')
assert 'Scheduled transforms and degree-follow relationships cannot be combined' in run_error_script(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\n'
    'p2 >> bass([0, 2, 4, 6], dur=0.5)\n'
    'p2.follow(p1)\np2.every(2, "reverse")\n')
sampleUnsupportedStack = run_error_script(
    'p1 >> play("x-o-", dur=0.5)\n'
    'p1.every(1, "stutter", 2)\np1.every(2, "shuffle")\n')
assert 'Sample players currently support scheduled stutter only' in (
    sampleUnsupportedStack), sampleUnsupportedStack
assert 'cannot follow itself' in run_compatibility_check(
    'p1 >> sine([0, 2], dur=0.5)\np1.follow(p1)\n')
assert 'degree assignment supports only' in run_compatibility_check(
    'p1 >> sine([0, 2], dur=0.5)\np1.degree = 3\n')
assert 'Use bounded transforms' in run_compatibility_check(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\np1.every(2, "stutter", 17)\n')
assert 'Use bounded transforms' in run_compatibility_check(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\n'
    'p1.every(2, "reverse", cycle=var([8, 16], 4))\n')
assert 'Use bounded transforms' in run_compatibility_check(
    'p1 >> sine([0, 2, 4, 6], dur=0.5)\np1.every(var([2, 4], 2), "reverse")\n')
dense_degrees = ', '.join(str(index % 4) for index in range(100))
assert 'beyond the 256-event limit' in run_error_script(
    'p1 >> sine([' + dense_degrees + '], dur=0.1)\n'
    'p1.every(6, "reverse")\n')
assert 'Circular player degree reference' in run_error_script(
    'p1 >> sine([0, 2], dur=0.5)\np2 >> bass([2, 4], dur=0.5)\n'
    'p1.follow(p2)\np2.follow(p1)\n')

groupExampleText = open(
    os.path.join(sys.argv[2], '115-player-groups.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupExampleText)
groupRows, groupEvents = run_script(
    groupExampleText, include_events=True)
assert len(groupRows) == 3 and len(groupEvents) <= 256
assert all(abs(float(groupRows[index][7]) - 0.22) < 1e-9
           for index in (0, 1))
assert abs(float(groupRows[0][9]) - 800.0) < 1e-9
assert abs(float(groupRows[1][9]) - 800.0) < 1e-9
assert groupRows[2][7] == '0.4'

groupPitchTimingText = open(
    os.path.join(sys.argv[2], '116-group-pitch-and-timing.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupPitchTimingText)
exampleGroupRows, exampleGroupBuffers, exampleGroupEvents = run_script(
    groupPitchTimingText, include_samples=True, include_events=True)
assert len(exampleGroupRows) == 3 and len(exampleGroupBuffers) == 4
assert len(exampleGroupEvents) == 12
groupDegreeTimevarText = open(
    os.path.join(sys.argv[2], '117-group-degree-timevars.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupDegreeTimevarText)
degreeTimevarExampleRows, degreeTimevarExampleEvents = run_script(
    groupDegreeTimevarText, include_events=True)
assert len(degreeTimevarExampleRows) == 3
assert len(degreeTimevarExampleEvents) == 12
groupRootText = open(
    os.path.join(sys.argv[2], '118-group-root-control.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    groupRootText)
groupRootExampleRows, groupRootExampleEvents = run_script(
    groupRootText, include_events=True)
assert len(groupRootExampleRows) == 3
assert len(groupRootExampleEvents) == 12
globalRootText = open(
    os.path.join(sys.argv[2], '119-global-root-timevars.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    globalRootText)
globalRootRows, globalRootEvents = run_script(
    globalRootText, include_events=True)
assert len(globalRootRows) == 4 and len(globalRootEvents) == 16
patternHelperText = open(
    os.path.join(sys.argv[2], '120-pattern-shaping-helpers.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternHelperText)
patternHelperRows, patternHelperEvents = run_script(
    patternHelperText, include_events=True)
assert len(patternHelperRows) == 3
assert 0 < len(patternHelperEvents) <= 256
patternMethodText = open(
    os.path.join(sys.argv[2], '121-pattern-method-parity.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternMethodText)
patternMethodRows, patternMethodEvents = run_script(
    patternMethodText, include_events=True)
assert len(patternMethodRows) == 3
assert 0 < len(patternMethodEvents) <= 256
pitchTransformText = open(
    os.path.join(sys.argv[2], '122-pitch-inversion.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    pitchTransformText)
pitchTransformRows, pitchTransformEvents = run_script(
    pitchTransformText, include_events=True)
assert len(pitchTransformRows) == 3
assert 0 < len(pitchTransformEvents) <= 256
patternCompositionText = open(
    os.path.join(sys.argv[2], '123-arpeggio-and-splice.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternCompositionText)
patternCompositionRows, patternCompositionEvents = run_script(
    patternCompositionText, include_events=True)
assert len(patternCompositionRows) == 3
assert 0 < len(patternCompositionEvents) <= 256
sortUnduplicateText = open(
    os.path.join(sys.argv[2], '124-sort-and-unduplicate.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    sortUnduplicateText)
sortUnduplicateRows, sortUnduplicateEvents = run_script(
    sortUnduplicateText, include_events=True)
assert len(sortUnduplicateRows) == 3
assert 0 < len(sortUnduplicateEvents) <= 256
patternReplaceText = open(
    os.path.join(sys.argv[2], '125-pattern-replace.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternReplaceText)
patternReplaceRows, patternReplaceEvents = run_script(
    patternReplaceText, include_events=True)
assert len(patternReplaceRows) == 3
assert 0 < len(patternReplaceEvents) <= 256
patternLayerText = open(
    os.path.join(sys.argv[2], '126-pattern-layer.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternLayerText)
patternLayerRows, patternLayerEvents = run_script(
    patternLayerText, include_events=True)
assert len(patternLayerRows) == 3
assert 0 < len(patternLayerEvents) <= 256
patternTrimText = open(
    os.path.join(sys.argv[2], '127-pattern-trim.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternTrimText)
patternTrimRows, patternTrimEvents = run_script(
    patternTrimText, include_events=True)
assert len(patternTrimRows) == 3
assert 0 < len(patternTrimEvents) <= 256
patternRandomText = open(
    os.path.join(sys.argv[2], '128-normalise-and-choose.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternRandomText)
patternRandomRows, patternRandomEvents = run_script(
    patternRandomText, include_events=True)
assert len(patternRandomRows) == 3
assert 0 < len(patternRandomEvents) <= 256
patternZipText = open(
    os.path.join(sys.argv[2], '129-pattern-zip-methods.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternZipText)
patternZipRows, patternZipEvents = run_script(
    patternZipText, include_events=True)
assert len(patternZipRows) == 3
assert 0 < len(patternZipEvents) <= 256
patternMapPivotText = open(
    os.path.join(sys.argv[2], '130-pattern-map-pivot.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternMapPivotText)
patternMapPivotRows, patternMapPivotEvents = run_script(
    patternMapPivotText, include_events=True)
assert len(patternMapPivotRows) == 3
assert 0 < len(patternMapPivotEvents) <= 256
patternShuffleText = open(
    os.path.join(sys.argv[2], '131-true-shuffle-and-shufflets.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternShuffleText)
patternShuffleRows, patternShuffleEvents = run_script(
    patternShuffleText, include_events=True)
assert len(patternShuffleRows) == 3
assert 0 < len(patternShuffleEvents) <= 256
patternWalkText = open(
    os.path.join(sys.argv[2], '132-pwalk-melodic-walk.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternWalkText)
patternWalkRows, patternWalkEvents = run_script(
    patternWalkText, include_events=True)
assert len(patternWalkRows) == 3
assert 0 < len(patternWalkEvents) <= 256
patternChainText = open(
    os.path.join(sys.argv[2], '133-pchain-transition-melody.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternChainText)
patternChainRows, patternChainEvents = run_script(
    patternChainText, include_events=True)
assert len(patternChainRows) == 3
assert 0 < len(patternChainEvents) <= 256
patternDeltaText = open(
    os.path.join(sys.argv[2], '134-pdelta-pitch-contour.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternDeltaText)
patternDeltaRows, patternDeltaEvents = run_script(
    patternDeltaText, include_events=True)
assert len(patternDeltaRows) == 3
assert 0 < len(patternDeltaEvents) <= 256
patternSquareText = open(
    os.path.join(sys.argv[2], '135-psquare-pitch-sequence.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    patternSquareText)
patternSquareRows, patternSquareEvents = run_script(
    patternSquareText, include_events=True)
assert len(patternSquareRows) == 3
assert 0 < len(patternSquareEvents) <= 256
groupPitchRows, groupPitchEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([1, 3], dur=1, oct=4)\n'
    'p2 >> pluck([2, 5], dur=1, oct=5)\n'
    'pair = Group(p1, p2)\n'
    'pair.degree = P[0, 2, 4, 6]\n'
    'pair.scale = Scale.minor\n'
    'pair.oct = var([3, 4], [1, 1])\n'
    'pair.dur = var([0.5, 1], [1, 1])\n'
    'pair.sus = 0.35\n', include_events=True)
assert len(groupPitchRows) == 2 and len(groupPitchEvents) == 8
expectedGroupPitches = [36, 39, 55, 46]
for playerEvents in (groupPitchEvents[:4], groupPitchEvents[4:]):
    assert [int(event[3]) for event in playerEvents] == expectedGroupPitches
    assert [round(float(event[1]), 6) for event in playerEvents] == [
        0.0, 0.5, 1.0, 2.0]
    assert all(abs(float(event[2]) - 0.35) < 1e-9
               for event in playerEvents)

groupBeforeRun = run_script(
    'pair = Group(p1, p2)\n'
    'pair.degree = P[1, 3]\n'
    'pair.scale = Scale.dorian\n'
    'pair.oct = 4\npair.dur = 0.5\npair.sus = 0.25\n'
    'p1 >> triangle([0, 2, 4], oct=5)\n'
    'p2 >> saw([3, 5], oct=5)\n')
assert len(groupBeforeRun) == 2
assert all(int(row[5]) == 2 for row in groupBeforeRun)
assert all(abs(float(row[4]) - 1.0) < 1e-9 for row in groupBeforeRun)

groupPitchReassignmentRows, groupPitchReassignmentEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([5, 6], oct=4, scale=Scale.major)\n'
    'pair = Group(p1)\n'
    'pair.degree = P[0, 1]\n'
    'pair.scale = Scale.majorPentatonic\n'
    'pair.oct = 3\n'
    'pair.scale = Scale.minor\n'
    'pair.oct = 5\n',
    include_events=True)
assert len(groupPitchReassignmentRows) == 1
assert [int(event[3]) for event in groupPitchReassignmentEvents] == [60, 62]

groupRootRows, groupRootEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([0, 2, 4, 6], dur=1, oct=5)\n'
    'p2 >> pluck([0, 2, 4, 6], dur=1, oct=4)\n'
    'pair = Group(p1, p2)\n'
    'pair.root = "D"\n',
    include_events=True)
assert len(groupRootRows) == 2 and len(groupRootEvents) == 8
assert [int(event[3]) for event in groupRootEvents[:4]] == [62, 66, 69, 73]
assert [int(event[3]) for event in groupRootEvents[4:]] == [50, 54, 57, 61]
assert 'No unsupported constructs detected' in run_compatibility_check(
    'pair = Group(p1, p2)\npair.root = "D"\n')

preassignedRootRows, preassignedRootEvents = run_script(
    'Scale.default.set("major")\n'
    'pair = Group(p1)\npair.root = 7\n'
    'p1 >> triangle([0, 2], dur=0.5, oct=5)\n',
    include_events=True)
assert len(preassignedRootRows) == 1
assert [int(event[3]) for event in preassignedRootEvents] == [67, 71]

def group_root_events(root_assignment):
    return run_script(
        'Scale.default.set("major")\nRoot.default.set("C")\n'
        'p1 >> triangle([0, 0, 0, 0], dur=0.5, oct=5)\n'
        'pair = Group(p1)\npair.root = ' + root_assignment + '\n',
        include_events=True)[1]

steppedGroupRootEvents = group_root_events('var([0, 7], [1, 1])')
assert [int(event[3]) for event in steppedGroupRootEvents] == [
    60, 60, 67, 67]
linearGroupRootEvents = group_root_events('linvar([0, 4], [1, 1])')
assert [int(event[3]) for event in linearGroupRootEvents] == [
    60, 62, 64, 62]

playerRootEvents = run_script(
    'Scale.default.set("major")\n'
    'p1 >> triangle([0, 0, 0, 0], dur=0.5, oct=5, '
    'root=var([0, 7], [1, 1]))\n',
    include_events=True)[1]
assert [int(event[3]) for event in playerRootEvents] == [60, 60, 67, 67]
assert 'root var/linvar must use pitch classes 0..11 or MIDI notes 12..127' in (
    run_error_script(
        'p1 >> triangle([0, 0], root=var([0, 12], [1, 1]))\n'))
assert 'root must be a pitch class' in run_error_script(
    'pair = Group(p1)\npair.root = "H"\n')

def group_degree_events(degree_assignment):
    return run_script(
        'Scale.default.set("major")\nRoot.default.set("C")\n'
        'p1 >> triangle([0, 0, 0, 0], dur=0.5, oct=4)\n'
        'p2 >> pluck([0, 0, 0, 0], dur=0.5, oct=5)\n'
        'pair = Group(p1, p2)\n'
        'pair.degree = ' + degree_assignment + '\n',
        include_events=True)[1]

dynamicGroupDegreeEvents = group_degree_events(
    'var([0, 2, 4, 6], [0.5, 0.5, 0.5, 0.5])')
staticGroupDegreeEvents = group_degree_events('P[0, 2, 4, 6]')
assert len(dynamicGroupDegreeEvents) == 8
assert [int(event[3]) for event in dynamicGroupDegreeEvents] == [
    int(event[3]) for event in staticGroupDegreeEvents]
linearGroupDegreeEvents = group_degree_events('linvar([0, 4], [1, 1])')
steppedLinearDegreeEvents = group_degree_events('P[0, 2, 4, 2]')
assert [int(event[3]) for event in linearGroupDegreeEvents] == [
    int(event[3]) for event in steppedLinearDegreeEvents]

preassignedGroupRows, preassignedGroupEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'pair = Group(p1, p2)\n'
    'pair.degree = var([0, 2], [0.5, 0.5])\n'
    'p1 >> triangle([0, 0], dur=0.5, oct=4)\n'
    'p2 >> pluck([0, 0], dur=0.5, oct=5)\n',
    include_events=True)
preassignedStaticRows, preassignedStaticEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([0, 0], dur=0.5, oct=4)\n'
    'p2 >> pluck([0, 0], dur=0.5, oct=5)\n'
    'Group(p1, p2).degree = P[0, 2]\n',
    include_events=True)
assert len(preassignedGroupRows) == 2 and len(preassignedGroupEvents) == 4
assert [int(event[3]) for event in preassignedGroupEvents] == [
    int(event[3]) for event in preassignedStaticEvents]

chordDegreeRows, chordDegreeEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([[0, 2, 4], [0, 2, 4]], dur=1, oct=5)\n'
    'Group(p1).degree = var([0, 2], [1, 1])\n',
    include_events=True)
assert len(chordDegreeRows) == 1 and len(chordDegreeEvents) == 6
assert [int(event[3]) for event in chordDegreeEvents[:3]] == [60, 64, 67]
assert [int(event[3]) for event in chordDegreeEvents[3:]] == [64, 67, 71]

restingGroupDegreeRows, restingGroupDegreeEvents = run_script(
    'Scale.default.set("major")\nRoot.default.set("C")\n'
    'p1 >> triangle([0, None, 0, 0], dur=0.5, oct=5)\n'
    'Group(p1).degree = var([0, 2, 4, 6], [0.5, 0.5, 0.5, 0.5])\n',
    include_events=True)
assert len(restingGroupDegreeRows) == 1
assert [float(event[1]) for event in restingGroupDegreeEvents] == [0.0, 1.0, 1.5]

sampleGroupRows, sampleGroupBuffers, sampleGroupEvents = run_script(
    'p1 >> play("x-o-", dur=0.5, sample=0)\n'
    'drums = Group(p1)\ndrums.dur = 1\ndrums.sus = 0.25\n',
    include_samples=True, include_events=True)
assert len(sampleGroupRows) == 1 and len(sampleGroupBuffers) == 4
assert len(sampleGroupEvents) == 4
assert 'numeric note players, not sample players' in run_error_script(
    'p1 >> play("x-o-", sample=0)\nGroup(p1).degree = P[0, 2]\n')
for invalidGroupAssignment in (
        'Group(p1).dur = 0\n',
        'Group(p1).sus = 0.01\n',
        'Group(p1).oct = 11\n',
        'Group(p1).degree = P["x", "o"]\n',
        'Group(p1).degree = var([0, 128], 1)\n'):
    assert run_error_script(invalidGroupAssignment)

groupSetAfterRun = run_script(
    'p1 >> sine([0, 2, 4, 6], amp=0.8)\n'
    'p2 >> triangle([1, 3, 5, 7], amp=0.7)\n'
    'pair = Group(p1, p2)\n'
    'pair.amp = 0.3\n'
    'pair.pan = -0.25\n'
    'pair.reverb = "none"\n')
assert len(groupSetAfterRun) == 2
assert all(abs(float(row[7]) - 0.3) < 1e-9 for row in groupSetAfterRun)
assert all(abs(float(row[8]) + 0.25) < 1e-9 for row in groupSetAfterRun)
assert all(abs(float(row[9]) - 20000.0) < 1e-9 for row in groupSetAfterRun)

groupSetBeforeRun = run_script(
    'pair = Group(p1, p2)\n'
    'pair.cutoff = 1250\n'
    'p1 >> sine([0, 2], cutoff=700)\n'
    'p2 >> bass([0, 3], cutoff=900)\n')
assert len(groupSetBeforeRun) == 2
assert all(abs(float(row[9]) - 1250.0) < 1e-9
           for row in groupSetBeforeRun)

allGroupControls = (
    'pair = Group(p1, p2)\n'
    'pair.amp = 0.3\npair.pan = -0.2\npair.cutoff = 1500\n'
    'pair.echo = 0.25\npair.delaytime = 0.2\n'
    'pair.atk = 0.02\npair.rel = 0.18\npair.reverb = "gverb"\n'
    'pair.room = 0.6\npair.mix = 0.25\npair.damp = 0.4\n'
    'pair.revtime = 2.5\npair.spread = 30\n'
    'pair.lpf = 1200\npair.lpr = 0.75\npair.hpf = 60\n'
    'pair.hpr = 0.8\npair.bpf = 700\npair.bpr = 0.65\n'
    'pair.decay = 2\npair.echomix = 0.4\npair.feedback = 0.5\n'
    'pair.bpnoise = 0.2\npair.swell = 0.3\n'
    'p1 >> sine([0, 2], dur=1)\n'
    'p2 >> triangle([1, 3], dur=1)\n')
allGroupRows, allGroupEffects, allGroupFilters, allGroupDecay, allGroupMod = (
    run_script(allGroupControls, include_group_controls=True))
assert len(allGroupRows) == 2
assert all(abs(float(row[7]) - 0.3) < 1e-9 for row in allGroupRows)
assert all(abs(float(row[8]) + 0.2) < 1e-9 for row in allGroupRows)
assert all(abs(float(row[9]) - 1500.0) < 1e-9 for row in allGroupRows)
assert all(abs(float(row[10]) - 0.25) < 1e-9 for row in allGroupRows)
assert all(abs(float(row[11]) - 0.2) < 1e-9 for row in allGroupRows)
assert len(allGroupEffects) == 2 and all(row[2] == '3' for row in allGroupEffects)
assert all([float(value) for value in row[3:8]] ==
           [0.25, 0.6, 0.4, 2.5, 30.0] for row in allGroupEffects)
assert len(allGroupFilters) == 2
assert all([float(value) for value in row[2:8]] ==
           [1200.0, 0.75, 60.0, 0.8, 700.0, 0.65]
           for row in allGroupFilters)
assert len(allGroupDecay) == 2
assert all([float(row[index]) for index in (2, 4, 6)] ==
           [2.0, 0.4, 0.5] for row in allGroupDecay)
assert len(allGroupMod) == 2
assert all([float(value) for value in row[2:4]] == [0.2, 0.3]
           for row in allGroupMod)

groupScheduleRows, groupScheduleEvents = run_script(
    'p1 >> sine([0, 1, 2, 3], dur=0.5)\n'
    'p2 >> triangle([3, 2, 1, 0], dur=0.5)\n'
    'pair = Group(p1, p2)\n'
    'pair.every(1, "reverse", cycle=4, ident="shared")\n',
    include_events=True)
assert len(groupScheduleRows) == 2 and len(groupScheduleEvents) == 32
assert [event[1] for event in groupScheduleEvents[:16]] == [
    event[1] for event in groupScheduleEvents[16:]]

groupStopRows = run_script(
    'p1 >> sine([0, 2])\np2 >> triangle([1, 3])\np3 >> bass([0, 4])\n'
    'Group(p1, p2).stop()\n')
assert [row[2] for row in groupStopRows] == ['1', '1', '0']

groupOnlyRows = run_script(
    'p1 >> sine([0, 2])\np2 >> triangle([1, 3])\np3 >> bass([0, 4])\n'
    'Group(p1, p2).only()\n')
assert [row[2] for row in groupOnlyRows] == [
    '0', '0', '1']

groupSoloRows, groupSoloMutes = run_script(
    'p1 >> sine([0])\np2 >> triangle([0])\np3 >> bass([0])\n'
    'Group(p1, p2).solo()\n', include_mutes=True)
assert len(groupSoloRows) == 3 and len(groupSoloMutes) == 8
assert [int(row[2]) for row in groupSoloMutes] == [
    0, 0, 1, 1, 1, 1, 1, 1]

masterRows = run_script(
    'p1 >> sine([0], amp=0.8)\np2 >> triangle([0], amp=0.7)\n'
    'p2.stop()\nMaster().amp = 0.25\n')
assert len(masterRows) == 2
assert abs(float(masterRows[0][7]) - 0.25) < 1e-9
assert masterRows[1][2] == '1'

assert 'Group accepts zero to eight distinct players' in run_compatibility_check(
    'Group(p1, p1)\n')
assert 'Unsupported Group attribute' in run_compatibility_check(
    'pair = Group(p1, p2)\npair.not_a_control = 1\n')
assert 'Group.add accepts one player name' in run_compatibility_check(
    'pair = Group(p1)\npair.add(p9)\n')

subprocess.run([
    sys.executable,
    os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), '..', '..',
                 'tools', 'test-modulation-synthdefs.py'),
    os.path.dirname(os.path.abspath(sys.argv[1]))], check=True)
subprocess.run([
    sys.executable,
    os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), '..', '..',
                 'tools', 'test-sclork-synthdefs.py'),
    os.path.dirname(os.path.abspath(sys.argv[1]))], check=True)

chainScript = (
    'p1 >> sine([0,2,4], effects=[("Phaser", {"fx_mix": 0.4}),'
    '("Tremolo", {"fx_rate": var([3,6], 4), "fx_depth": 0.7})])\n'
    'p2 >> play("x-o-", effects=[("Tremolo", {"fx_depth": 0.2})])\n')
chainNames = ('Phaser', 'Tremolo')
_, chainRecords = run_script(chainScript, user_synth_names=chainNames,
                             include_effect_chains=True)
assert chainRecords[0][:4] == ['J', '0', '1', '1'], chainRecords
assert len(chainRecords[0]) == 18, chainRecords
assert chainRecords[0][12].startswith('v:'), chainRecords
assert chainRecords[1] == ['J', '1', '0'], chainRecords
assert 'No unsupported constructs detected' in run_compatibility_check(
    chainScript, chainNames)
echojamExampleName = '186-airwindows-echojam-on-chords.py'
echojamExampleText = open(
    os.path.join(sys.argv[2], echojamExampleName), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    echojamExampleText, ('AirwindowsEchojam',))
echojamExampleRows = run_example(
    echojamExampleName, ('AirwindowsEchojam',))
assert len(echojamExampleRows) == 2
sclorkEffectNames = (
    'SCLOrkPhazerFX', 'SCLOrkDecimatorFX',
    'SCLOrkCrossoverDistortionFX')
sclorkEffectExample = open(
    os.path.join(sys.argv[2], '201-sclork-insert-effects.py'),
    encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    sclorkEffectExample, sclorkEffectNames)
sclorkRows = run_example('201-sclork-insert-effects.py', sclorkEffectNames)
assert len(sclorkRows) == 3
sclorkChainScript = (
    'p1 >> sine([0, 3, 7], effects=['
    '("SCLOrkPhazerFX", {"fx_center": 700}),'
    '("SCLOrkDecimatorFX", {"fx_bits": 7}),'
    '("SCLOrkCrossoverDistortionFX", {"fx_shape": 0.8})])\n')
_, sclorkChainRows = run_script(
    sclorkChainScript, user_synth_names=sclorkEffectNames,
    include_effect_chains=True)
assert len(sclorkChainRows) == 1 and sclorkChainRows[0][0] == 'J'
assert 'No unsupported constructs detected' in run_compatibility_check(
    sclorkChainScript, sclorkEffectNames)
for invalidSCLOrkControl in (
        ('SCLOrkDecimatorFX', 'fx_bits', 17),
        ('SCLOrkPhazerFX', 'fx_q', 1.1),
        ('SCLOrkCrossoverDistortionFX', 'fx_shape', -0.1)):
    effectName, controlName, controlValue = invalidSCLOrkControl
    badControls = run_error_script(
        'p1 >> sine(0, effect="' + effectName + '", ' +
        controlName + '=' + repr(controlValue) + ')\n',
        (effectName,))
    assert 'between' in badControls, badControls
_, maxChain = run_script(
    'p1 >> sine(0, effects=[("Tremolo", {"fx_rate":1}),'
    '("Tremolo", {"fx_rate":2}),("Tremolo", {"fx_rate":3}),'
    '("Tremolo", {"fx_rate":4})])\n',
    user_synth_names=('Tremolo',), include_effect_chains=True)
assert maxChain[0][:3] == ['J', '0', '3'] and len(maxChain[0]) == 48
run_script('c = {"fx_rate": 2}\n'
           'p1 >> sine(0, effects=[("Tremolo", c)])\n'
           'assert c == {"fx_rate": 2}\n'
           'p1 >> sine(0, effects=[])\n'
           'p1.stop()\n', user_synth_names=('Tremolo',))
for badScript, message in (
    ('p1 >> sine(0, effect="Phaser", effects=[])', 'not both'),
    ('p1 >> sine(0, effects=[], fx_mix=0.3)', 'standalone'),
    ('p1 >> sine(0, effects="Phaser")', 'list'),
    ('p1 >> sine(0, effects=[("Phaser", {}, {})])', 'each effects item'),
    ('p1 >> sine(0, effects=[("Unknown", {})])', 'unsupported'),
    ('p1 >> sine(0, effects=[("Phaser", [])])', 'dictionary'),
    ('p1 >> sine(0, effects=[("Phaser", {1: 2})])', 'strings'),
    ('p1 >> sine(0, effects=[("Phaser", {"bad": 1})])', 'Unsupported'),
    ('p1 >> sine(0, effects=[("Phaser", {"fx_mix": 2})])', 'between'),
    ('p1 >> sine(0, effects=[("Phaser", {"fx_rq": 0.2})])', 'not a control'),
    ('p1 >> sine(0, effects=[("Phaser", {})]*5)', 'four inserts')):
    chainError = run_error_script(badScript, chainNames)
    assert message.lower() in chainError.lower(), (badScript, chainError)
assert 'not loaded' in run_error_script(
    'p1 >> sine(0, effects=[("Tremolo", {})])')
for exampleName, names in (
        ('173-ordered-player-effect-chain.py', ('Phaser', 'Tremolo')),
        ('174-drum-effect-chain.py', ('Flanger', 'Chorus'))):
    text = open(os.path.join(sys.argv[2], exampleName), encoding='utf-8').read()
    assert 'No unsupported constructs detected' in run_compatibility_check(text, names)
    run_script(text, user_synth_names=names)
    assert 'Portable/SynthDefs' in run_compatibility_check(text)
cutExample = open(os.path.join(sys.argv[2], '175-foxdot-cut-controls.py'),
                  encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(cutExample)
run_script(cutExample)
for malformed in (
        'effects=[("Phaser", {})]*5', 'effects="Phaser"',
        'effects=[("Phaser", {}, {})]',
        'effects=[("Phaser", {"bad": 2})]',
        'effects=[("Missing", {})]', 'effects=[], effect="Phaser"'):
    assert 'No unsupported constructs detected' not in run_compatibility_check(
        'p1 >> sine(0, ' + malformed + ')', chainNames)
assert 'effectIndices !=' in editor_source
assert 'L" -> "' in editor_source
print('PASS ordered effects: legacy form, four-insert bound, independent modulation, checker, copies, examples and panel order')

sample_directory = os.path.join(sys.argv[2], 'Samples')
assert len([name for path, dirs, files in os.walk(sample_directory) for name in files if name.endswith('.wav')]) == 6
pitchCrusherExample = open(os.path.join(
    sys.argv[2], '177-pshift-bits-crush.py'), encoding='utf-8').read()
assert 'No unsupported constructs detected' in run_compatibility_check(
    pitchCrusherExample), run_compatibility_check(pitchCrusherExample)
run_script(pitchCrusherExample)
for controls in ('pshift=-24, bits=0, crush=1',
                 'pshift=24, bits=24, crush=128'):
    run_script('p1 >> sine(0, ' + controls + ')\n')
for controls in ('pshift=-24.1', 'pshift=24.1', 'bits=-1', 'bits=25',
                 'crush=0', 'crush=129', 'bits=True', 'crush=False'):
    error = run_error_script('p1 >> sine(0, ' + controls + ')\n')
    assert ('outside the supported range' in error.lower() or
            'not bool' in error.lower()), (controls, error)
print('Smoke tests passed: embedded pattern/player API, Pvar phrase switching, Clock.bpm var/linvar, compatibility checks, diagnostics, WAV samples, user SynthDefs, and Portable examples.')
