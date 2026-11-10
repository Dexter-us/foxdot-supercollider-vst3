len(randomRequests) == 8 and len(randomEvents) == 8
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
