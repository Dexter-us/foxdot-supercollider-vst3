# Remaining core-UGen FoxDot synth bank

The 26 files in `upstream/` are unmodified FoxDot source from commit
`affae587ab7b2a5f93102302254de16dd72d4c50`, at
https://github.com/Qirky/FoxDot/tree/affae587ab7b2a5f93102302254de16dd72d4c50/FoxDot/osc/scsyndef
by Ryan Kirkbride and FoxDot contributors. Adaptations are CC BY-SA 4.0; see
`../Licenses/FoxDot-CC-BY-SA-4.0.txt` and `../Licenses/FoxDot-ATTRIBUTION.txt`.
The checked-in source hashes make the upstream snapshot auditable.

Regenerate the embedded header with `python tools/build-foxdot-synth-bank.py`.
This requires SuperCollider 3.13.x's `sclang` on the development machine only.
Windows CI consumes the compiled header; it does not run sclang or rebuild UGens.

The adapter preserves the original oscillator/filter expressions, amplitude
arithmetic and envelope shapes. Intentional host adaptations:

- Replace FoxDot's control-bus frequency input with the host's frequency control.
- Replace output/pan routing with private mono/Mouse/reverb buses; host panning.
- Trigger EnvGen/Line/XLine on every pooled note, with doneAction=0 so a source
  patch cannot free a node that the host owns.
- Resample Rand on note triggers, not just once when the pooled graph is created.
- `creep` keeps its exponential rise but uses a linear final 1 ms fall to a
  1e-9 floor (the host fades it completely to zero). The nonzero floor also
  makes exponential retriggering safe after the previous note has finished.
  The upstream exponential-to-zero segment produced non-finite samples in the
  render regression test. The host also applies a final 5 ms safety fade.
- The sawbass filter's permanently held ADSR is represented as its attack/decay
  followed by the same held level, so pooled triggers do not release it early.
- Each note in this new bank receives a fresh SC graph from SC's bounded RT
  memory pool, matching FoxDot's fresh-node oscillator/filter history. The
  previous graph in that voice slot is freed before its replacement is created.
  Repeated-note audio tests caught silence in pads when its zero-Hz Pulse state
  was reused; envelope retriggering alone was not sufficient.
- `klank` uses FoxDot's core-only definition, without optional SC3 Decimator.
- `piano`, audio input, samples, and Renardo-only synths are not added here.

The new synthesis controls are scalar, per-player values. Existing amp, pan,
filters, effects, note patterns and time-variable controls remain supported.
