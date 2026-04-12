# QA-Conformance findings (2026-04-11)

Scope: VST3 spec conformance and host compatibility for `mw160`. Discovery
only, no source modifications.

## Summary

- **pluginval strictness-10: FAILED.** The "Plugin state restoration"
  block reports a reproducible failure on the two boolean parameters
  (`overEasy`, `stereoLink`). All 3 runs at random seeds reproduced a
  failure on at least one of the two booleans; the float parameters
  restored cleanly every time. All other pluginval test groups pass.
- **Thread-safety: PASS with one gap in coverage.** The audio thread
  path (`MW160Processor::processBlock` and the entire `mw160::Compressor`
  pipeline) is free of allocations, locks, logging, and other RT-unsafe
  primitives. The existing `MW160NoAllocTests` covers `Compressor::
  processSample` / `processSampleLinked` only; it does not cover the
  full `processBlock` (meter atomic publish, `getMagnitude`, parameter
  smoothing through APVTS), which are also safe by inspection but
  uncovered by automation.
- **State round-trip: PARTIAL.** Float parameters round-trip correctly.
  Both bool parameters fail to round-trip under pluginval, indicating a
  real bug in `setStateInformation`'s interaction with
  `juce::AudioParameterBool` instances.
- **Latency reporting: PASS.** The plugin uses no lookahead;
  `getLatencySamples()` reports 0 in pluginval (`Reported latency: 0`).
  Tail length: 0. Both consistent with the implementation.
- **Bus layouts: PASS.** Only mono and stereo are accepted with matching
  input/output, and pluginval enumerates both as expected.
- **Preset load: PASS for factory, fragile for user.** Factory preset
  apply uses `setValueNotifyingHost` with normalized values; the seven
  factory presets all round-trip cleanly via APVTS. User preset load is
  hardened against parse failure (XML returns `nullptr` if malformed)
  but ignores files that parse but contain a wrong root tag — silently.
  See QA-CONF-003.

Counts by severity: **HIGH: 1, MEDIUM: 4, LOW: 2**

Findings file: `/home/mwoolly/projects/mw160/qa-findings/qa-conformance.md`
Artifacts directory: `/home/mwoolly/projects/mw160/qa-artifacts/`

---

## pluginval strictness-10 output

Three independent runs of
`xvfb-run -a ./build/pluginval --validate-in-process --strictness-level 10
--validate ./build/MW160_artefacts/Release/VST3/MW160.vst3`.

| Run | Seed       | Failures                                                                 |
|-----|------------|---------------------------------------------------------------------------|
| 1   | 0x443e57   | Test 4: `OverEasy not restored` — expected 0, got 0.163635                |
| 2   | (random)   | Test 4: `OverEasy not restored` (got 0.435694) AND Test 5: `Stereo Link not restored` (expected 1, got 0.572227) |
| 3   | 0x3ce904c  | Test 4: `OverEasy not restored` — expected 0, got 0.160072                |

The relevant excerpt from run 1
(`qa-artifacts/pluginval_strict10.log`):

```
Plugin name: MW160
Alternative names: MW160
SupportsDoublePrecision: no
Reported latency: 0
Reported taillength: 0
...
Num programs: 7
All program names checked
...
Starting tests in: pluginval / Plugin state restoration...
!!! Test 4 failed: OverEasy not restored on setStateInformation -- Expected value  within 0.1 of: 0, Actual value: 0.163635
FAILED!!  1 test failed, out of a total of 7
...
Inputs:
        Named layouts: Mono, Stereo
        Discrete layouts: Discrete #1
Outputs:
        Named layouts: Mono, Stereo
        Discrete layouts: Discrete #1
Main bus num input channels: 2
Main bus num output channels: 2
...
*** FAILED
```

All other pluginval test groups (Plugin info, Plugin programs, Editor,
Open editor whilst processing, Audio processing, Non-releasing audio
processing, Plugin state, Automation, Editor Automation, Automatable
Parameters, Parameters, Background thread state, Parameter thread safety,
auval, Basic bus, Listing available buses, Enabling all buses, Disabling
non-main busses, Restoring default layout, Fuzz parameters) **pass** at
strictness 10.

Full logs:

- `/home/mwoolly/projects/mw160/qa-artifacts/pluginval_strict10.log`
- `/home/mwoolly/projects/mw160/qa-artifacts/pluginval_run3.log`

---

## Thread-safety audit

### `src/PluginProcessor.cpp` — `processBlock`

- Reads parameters via `std::atomic<float>::load()` on pointers cached
  in the constructor — no per-block APVTS lookup. Safe.
- Uses stack locals only; no `new`, no `std::vector::push_back`, no
  `juce::String`, no `std::function`, no `juce::Array` mutation.
- Calls `juce::AudioBuffer<float>::getMagnitude` and `getWritePointer`
  — both RT-safe by JUCE contract.
- Publishes meter values via `std::atomic<float>::store(...,
  std::memory_order_relaxed)`. Read on the GUI timer thread via
  `load(... relaxed)`. Acceptable for monitor metering — strict
  acquire/release is not required since the values are advisory.
- No locks, no `juce::CriticalSection::enter`, no logger calls.
- `juce::ScopedNoDenormals noDenormals` is set, which is the standard
  guard.
- Verdict: **safe.**

### `src/dsp/*` — Compressor pipeline

- `RmsDetector::processSample` / `processSampleFromSquared`: scalar
  ops, no allocation, no state outside `runningSquared_`. Safe.
- `Ballistics::processSample`: scalar ops, branch on `diff`, scalar
  exp/pow. No allocation. Safe. Note that `std::exp` and `std::pow`
  are called per-sample on the attack branch, which is a perf concern
  (filed as LOW: QA-CONF-007) but not a thread-safety concern.
- `GainComputer::computeGainReduction`: stateless, scalar ops. Safe.
- `VcaSaturation::processSample`: stateless polynomial. Safe.
- `ParameterSmoother`: scalar ops. `setTarget` may compute `std::pow`
  on the audio thread when called from `Compressor::setOutputGain`
  (Multiplicative type). Per-sample work in `getNextValue` is just
  multiply/add. Safe.
- `Compressor::applyCompression`: per-sample log10 + per-sample pow
  conversion for the dB→linear gain. Safe but again a perf concern.
- Verdict: **safe**, no allocations, no locks, no logging on the audio
  thread.

### `MW160NoAllocTests` — coverage analysis

- Tests `Compressor::processSample` and `Compressor::processSampleLinked`
  for 1 second of audio at 44.1 kHz with allocation tracking via
  thread-local `operator new` override. Solid technique.
- Pre-warms the compressor before tracking starts, so any lazy
  initialisation in JUCE / std lib does not pollute the count.
- Tests parameter changes mid-stream (good — covers `setTarget` path
  including the multiplicative smoother's `std::pow` call, which
  doesn't allocate).
- **Coverage gap (QA-CONF-006):** does NOT exercise
  `MW160Processor::processBlock` end-to-end. The pieces inside
  `processBlock` that aren't covered:
  - `apvts.getRawParameterValue` returns (already cached, but the
    pattern is worth a regression guard).
  - `juce::AudioBuffer<float>::getMagnitude` (RT-safe by contract,
    but should be in the alloc-test fence to catch a future
    refactor that swaps in something that allocates).
  - The meter-atomic store calls (cannot allocate, but again, a
    full processBlock test pins the contract end-to-end).
- Verdict: alloc test is **sound where it covers**; **add a
  processBlock-level alloc test** to close the gap.

### Verdict: PASS for the audio thread; one alloc-test coverage gap.

---

## Findings

### QA-CONF-001: Bool parameter state restoration is broken under pluginval
**Severity:** HIGH
**Owner:** owner:dev-agent
**Context / repro:**
1. `xvfb-run -a build/pluginval --validate-in-process --strictness-level
   10 --validate build/MW160_artefacts/Release/VST3/MW160.vst3`
2. The "Plugin state restoration" group fails on `OverEasy` and/or
   `Stereo Link`. Reproduced across 3 random seeds.

**Expected:** After `setStateInformation` with a previously captured
state, every parameter — including booleans — must read back its
captured value. This is the canonical VST3 host expectation and is
asserted by pluginval's restoration test loop (set → capture → mutate →
restore → verify).

**Actual:** Float parameters (`threshold`, `ratio`, `outputGain`, `mix`)
restore correctly. The two `juce::AudioParameterBool` parameters
(`overEasy`, `stereoLink`) read back the *pre-restore mutated* random
value rather than the captured value, e.g. expected 0, actual 0.163635 /
0.435694 / 0.160072. The non-snapped intermediate values strongly
suggest pluginval is reading the parameter via
`AudioProcessorParameter::getValue()` (normalized [0,1]) and the bool
parameter's underlying atomic was never overwritten by the restore.

**Location:** `src/PluginProcessor.cpp:217-222` —
```cpp
void MW160Processor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

The implementation looks canonical — `apvts.replaceState` is the
JUCE-recommended path and internally calls `valueTreeRedirected` →
`updateParameterConnectionsToChildTrees` → `setNewState` →
`setDenormalisedValue`, which should propagate the restored value to
each parameter via `setValueNotifyingHost`. However, pluginval observes
that bool parameters do not actually update. Likely root causes (in
order of probability):

1. Pluginval is setting the parameter to non-snapped values via the
   raw VST3 parameter API (e.g. `setValue(0.43)`), the
   `flushParameterValuesToValueTree` step writes that raw float (not
   the snapped 0/1) into the state tree, and on subsequent
   `replaceState` the new state's bool value differs from the cached
   atomic (which was already snapped on read), so the restored value
   ends up at an in-between value that the bool parameter itself does
   not snap when read via `getValue()`.
2. There is a race between `setStateInformation` (called from
   pluginval's harness thread) and an asynchronous APVTS timer flush
   that re-publishes the pre-restore value back into the tree.
3. JUCE `AudioParameterBool::setValue` may snap on input but not on
   read; the underlying `std::atomic<float>` retains whatever was
   written by the previous `parameter.setValue(0.43)` call, and the
   restore path's `approximatelyEqual` short-circuit on
   `setDenormalisedValue` skips the actual write because the new value
   (e.g. 0.0) doesn't differ enough.

The dev-agent should reproduce the bug with a small unit test that
mimics pluginval's set → capture → mutate → restore cycle on each bool
parameter and confirm round-trip equality. The fix is most likely to
explicitly snap the bool atomics on restore, or to call
`apvts.getParameter("overEasy")->setValueNotifyingHost(...)` in
`setStateInformation` for each parameter, instead of relying on
`replaceState` alone.

**Evidence:**
- `qa-artifacts/pluginval_strict10.log` (run 1)
- `qa-artifacts/pluginval_run3.log` (run 3)

---

### QA-CONF-002: `apvts.replaceState` does not snap bool params; consider explicit per-parameter restore
**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context / repro:** Same as QA-CONF-001 — the underlying mechanism
behind the round-trip failure.
**Expected:** State restore is lossless for every declared parameter
type the plugin uses.
**Actual:** Float params survive `replaceState`; bool params do not.
**Location:** `src/PluginProcessor.cpp:217-222`
**Recommendation:** In `setStateInformation`, after the
`apvts.replaceState` call, iterate the parameter layout and explicitly
re-apply each parameter's value via
`apvts.getParameter(id)->setValueNotifyingHost(
apvts.getParameter(id)->convertTo0to1(...))`. Or, simpler: parse the
XML directly and write each child node's `value` property into the
matching parameter via `setValueNotifyingHost`. This makes the restore
robust against any internal APVTS quirk and removes the bool-only edge
case. Pair with a DAW-style state-round-trip unit test (see
QA-CONF-006) to lock the contract.

---

### QA-CONF-003: User preset load silently ignores wrong-tag XML
**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context / repro:** Save a malformed preset XML (correct XML, wrong
root tag) into the user preset directory and try to load it via the
preset combo box.
**Expected:** Either fail loudly with a user-visible error, or fall back
to a safe default.
**Actual:** The load is silently a no-op. The plugin's APVTS state is
unchanged, but the UI's preset combo box still selects the bad preset
as if it loaded. The user has no signal that the preset is corrupt.
**Location:** `src/PresetManager.cpp:61-71` —
```cpp
void PresetManager::loadUserPreset(int userIndex)
{
    if (userIndex < 0 || userIndex >= userPresetFiles_.size())
        return;

    const auto& file = userPresetFiles_[userIndex];
    auto xml = juce::parseXML(file);

    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}
```

If `parseXML` returns null (malformed XML) OR the root tag mismatches,
the function silently returns. There is no logging, no
`AlertWindow`, no return value. The combo box selection in
`PluginEditor::comboBoxChanged` does not check for failure either.

**Recommendation:** Return a `bool` from `loadUserPreset` /
`loadPreset`, and on failure show an `AlertWindow` from the editor or
revert the combo selection. Also consider validating the version
attribute of the loaded state (see QA-CONF-005).

---

### QA-CONF-004: Preset version field absent — silent compatibility break risk
**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context / repro:** Save a user preset, change the parameter layout
in a future version of the plugin, reload the preset.
**Expected:** Either the old preset still loads cleanly (parameters
preserved, new ones default), or the user sees a clear "preset
incompatible" message.
**Actual:** `getStateInformation` writes the bare APVTS state via
`copyXmlToBinary` with no plugin version stamp. `setStateInformation`
checks only the root tag name. There is no schema version field. If a
future version renames `overEasy` → `softKnee` (which it must, per
QA-TM-001 trademark scrub), every existing user preset will silently
drop the soft-knee setting back to default and provide no diagnostic.
**Location:** `src/PluginProcessor.cpp:210-222` and
`src/PresetManager.cpp:73-98`
**Recommendation:** Add a `<plugin version="1"/>` attribute to the root
state tree before serializing. On restore, check the version and either
migrate older states forward or warn loudly. This becomes acute when
the trademark scrub (QA-TM-001) renames `overEasy` to `softKnee`.

---

### QA-CONF-005: Parameter ID `overEasy` is brand-adjacent — schedule rename + state migration
**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context / repro:** Inspect parameter IDs in `createParameterLayout`.
**Expected:** Per the trademark hygiene rules, no brand-adjacent
vocabulary in code, IDs, or strings. (Already filed as QA-TM-001 for
the trademark surface; this entry is the *state-restoration* angle of
the same rename.)
**Actual:** Parameter ID is the literal string `overEasy`
(`src/PluginProcessor.cpp:48`). When QA-TM-001 lands the rename to e.g.
`softKnee`, every existing user preset will lose its setting because
APVTS keys preset XML by parameter ID. There is no migration shim.
**Location:** `src/PluginProcessor.cpp:47-50`
**Recommendation:** When the rename lands, `setStateInformation` should
look for the legacy ID `overEasy` in the loaded XML and copy its value
into the new ID before calling `replaceState`. This is a backwards
compat shim, not a permanent path. Bumping `ParameterID{"...", 1}` to
version 2 will not solve this — that JUCE versioning is for VST3
parameter ID stability across hosts, not for state migration.

---

### QA-CONF-006: No-alloc test does not cover `MW160Processor::processBlock`
**Severity:** LOW
**Owner:** owner:dev-agent
**Context / repro:** `tests/test_no_alloc.cpp` exercises
`mw160::Compressor::processSample` / `processSampleLinked` only. The
JUCE-side `processBlock` (which adds `getRawParameterValue` reads,
`getMagnitude` calls, `getWritePointer`, atomic stores) is not in the
allocation fence.
**Expected:** A regression guard on the actual audio entry point of the
plugin so that any future refactor that introduces an allocation in the
JUCE-side glue (e.g., a `juce::String` for a debug log, a
`std::vector` for some derived state) is caught at CI time.
**Actual:** Refactors of `MW160Processor::processBlock` rely on review
discipline alone. By inspection the current implementation is alloc-free,
so this is a coverage gap, not an active bug.
**Location:** `tests/test_no_alloc.cpp` and `src/PluginProcessor.cpp:125-201`
**Recommendation:** Add a fourth Catch2 case that constructs a
`MW160Processor`, calls `prepareToPlay(48000, 512)`, allocates a stereo
`juce::AudioBuffer<float>` of 512 samples (outside the tracker), then
runs ~100 blocks through `processBlock` inside the
`AllocationTracker` and asserts zero allocations.

---

### QA-CONF-007: Per-sample `std::exp` / `std::pow` in audio thread
**Severity:** LOW
**Owner:** owner:dev-agent
**Context / repro:** Inspect `Ballistics::processSample` and
`Compressor::applyCompression`.
**Expected:** Audio-thread DSP avoids transcendentals on the hot path
where reasonable. Not a correctness issue, only a performance / battery
concern.
**Actual:** `Ballistics::processSample` calls `std::pow` and `std::exp`
on every attack-branch sample. `Compressor::applyCompression` calls
`std::log10` (`rms_dB = 20 * log10(rmsLinear)`) and `std::pow` (`gain =
10^(dB/20)`) on every sample. None of these allocate or block, so
they're thread-safe; they're a perf concern only. JUCE 7 ships SIMD
helpers and lookup tables for these ops; the existing performance test
(`tests/test_performance.cpp`) does not flag this as out of budget at
the moment, so this is "track for future optimization" rather than
"fix now."
**Location:** `src/dsp/Ballistics.cpp:30-33`,
`src/dsp/Compressor.cpp:75, 87`
**Recommendation:** No action required immediately. If a future
profiling pass shows the compressor as hot, replace the dB↔linear path
with a fast log/exp approximation or a precomputed table.

---

## Out of scope for this agent (filed elsewhere)

- The trademark scrub of `OverEasy`, `dbx 160A`, `Blackmer 200`, `DBX
  160` references in code, comments, and parameter labels is
  CRITICAL and is QA-TM-001. This conformance pass intentionally does
  not re-file it. Note however that QA-CONF-005 above is the
  *state-migration* component of the same rename and must be done
  *with* the trademark scrub, not after it, or every user preset breaks.
- Loading the VST3 in `juce::AudioPluginHost` was not attempted: the
  AudioPluginHost is not pre-built in this tree, only its source is
  vendored under
  `build/_deps/juce-src/extras/AudioPluginHost`. Building it would
  take ~15 min and is beyond this pass. pluginval's
  `--validate-in-process` exercises the same JUCE plugin loading path
  as AudioPluginHost would.

