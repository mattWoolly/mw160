# QA-UX findings (2026-04-11)

## Summary

The mw160 plugin exposes a minimal control set that is broadly aligned with
REFERENCE.md §2 (3 rotary controls + soft-knee toggle + stereo-link toggle +
mix). Identity-defining anti-requirements from §2.4 (no exposed attack /
release / RMS-window / knee width) are correctly observed. Mix defaults to
100% wet per §2.5. The metering uses the "three independent meters" idiom
which is explicitly allowed by §2.3, and GR direction is correct.

The UX has three serious problems:

1. **Threshold unit semantics are wrong.** The parameter is labeled `dBu`
   but the underlying detector and meter operate in dBFS. There is no level
   calibration anywhere in the signal path. A user setting threshold to
   `0 dBu` will experience compression starting around `0 dBFS`, not the
   `+4 dBu = ~ -18 dBFS` they would expect from the label. This is the
   single biggest first-impression bug.
2. **Ratio does not reach infinity.** The range is `[1.0, 60.0]` and the
   gain computer applies the textbook `slope = 1/R - 1` formula
   uniformly, so at the top of the travel the slope is `-0.9833`, not
   `-1.0`. A 10 dB excess produces 9.83 dB of GR — the user can never
   actually limit. REFERENCE.md §2.1 explicitly requires reaching ∞:1.
3. **No bypass control.** REFERENCE.md §2.2 lists bypass as required.
   The plugin does not expose a bypass parameter and does not implement
   `getBypassParameter()`. Users get the host-synthesized bypass only,
   with no on-faceplate indicator.

In addition: a brand-named user-visible string (the soft knee toggle
display name) was leaking the brand into the GUI — this is covered by
the already-filed QA-TM-001 but is also a UX concern. There are no
tooltips on any control. Negative-ratio (dynamics inversion) capability
is unimplemented (MEDIUM, called optional in REFERENCE.md). Preset save
has no validation of name characters and silently overwrites duplicates.
LED indicators in the header compare a dBFS-domain meter to a "dBu"-
labeled threshold, so they inherit the unit mismatch.

Counts: 2 CRITICAL, 4 HIGH, 5 MEDIUM, 4 LOW.

## Control-set audit (vs REFERENCE.md §2)

| Control          | Present? | Range correct?        | Units correct?              | Notes |
|------------------|----------|-----------------------|-----------------------------|-------|
| Threshold        | yes      | yes (-40..+20)        | NO — labeled dBu, behaves dBFS | See QA-UX-001. Range is correct numerically; the unit label is wrong relative to the signal path. |
| Ratio            | yes      | partial (1..60)       | yes (`:1`)                  | Top of travel does NOT reach ∞ in DSP. See QA-UX-002. |
| Output gain      | yes      | yes (-20..+20)        | yes (dB)                    | Smoothed; default 0 dB. OK. |
| Knee mode (hard/soft) | yes  | n/a                   | n/a                         | Bool toggle, previously brand-named — see QA-UX-004. |
| Stereo link      | yes      | n/a                   | n/a                         | Bool, default on. OK behaviorally. |
| Bypass           | NO       | n/a                   | n/a                         | See QA-UX-003. |
| Meter mode (in/out/GR) | n/a (three independent meters) | n/a | n/a | §2.3 explicitly permits the "three independent meters" idiom; not a bug. |
| Mix              | yes      | yes (0..100 %)        | yes (%)                     | Default 100 % per §2.5. OK. |
| Negative ratio (dynamics inversion) | NO | n/a            | n/a                         | Optional per §2.1. See QA-UX-009. |
| Attack           | (correctly absent) | n/a         | n/a                         | §2.4. OK. |
| Release          | (correctly absent) | n/a         | n/a                         | §2.4. OK. |
| Detector RMS window | (correctly absent) | n/a      | n/a                         | §2.4. OK. |
| Knee width       | (correctly absent — fixed at 10 dB internally) | n/a | n/a | §2.4. OK. |

## Findings

### QA-UX-001: Threshold parameter labeled "dBu" but operates in dBFS
**Severity:** CRITICAL
**Owner:** owner:dev-agent
**Context:** REFERENCE.md §2.1 explicitly calls out that the threshold unit
convention must be unambiguously documented in the UI. The plugin labels
the threshold knob `dBu` (`PluginProcessor.cpp:31`) but the entire signal
path is dBFS-relative: the RMS detector squares raw float samples
(`RmsDetector.h`, `Compressor.cpp:74-76` converts via `20*log10(rms)`),
so the level fed to `GainComputer::computeGainReduction` is in
dBFS-relative units. The `threshold_dB` value handed to the gain computer
is the raw parameter value, with no `+4 dBu = -18 dBFS` calibration
shift applied anywhere.
**Expected:** REFERENCE.md §2.1 — "The chosen convention must be
documented in the UI and the manual — ambiguity here is a UX bug." For a
digital plugin the sensible convention is dBFS; if dBu is preserved, the
calibration constant must actually be applied in the DSP.
**Actual:** Label says `dBu` (range -40..+20), DSP treats it as dBFS
(range -40..+20). A user dialing in `0 dBu` of threshold expects roughly
`-18 dBFS` of compression onset; they actually get `0 dBFS` onset. Off by
~18 dB across the entire range.
**Location:** `src/PluginProcessor.cpp:26-31` (param decl);
`src/dsp/Compressor.cpp:74-80` (no calibration applied);
`src/dsp/RmsDetector.h` (squares raw float samples — confirms dBFS
domain); `src/FactoryPresets.h:8` (`// dBu, range: [-40, +20]` — the
factory presets are also expressed in this misleading convention).

Recommended fix: change the label to `dBFS`, document this in any user
manual, and rebalance the factory presets so they hit reasonable GR
levels on typical program material in the new range. Alternatively (less
recommended): apply a `+18 dB` reference shift so `0 dBu` actually maps
to `-18 dBFS` in the comparator.

### QA-UX-002: Ratio parameter does not reach infinity:1 at the top of travel
**Severity:** CRITICAL
**Owner:** owner:dev-agent
**Context:** REFERENCE.md §2.1 explicitly requires that the ratio control
"must reach **infinity** (limiter mode) at the top of the travel." The
parameter range is `[1.0, 60.0]` and `GainComputer::computeGainReduction`
computes `slope = 1/ratio - 1` for all `ratio > 1`. At ratio=60 the
slope is `-0.9833`, not `-1.0`. A 10 dB input excess produces 9.83 dB
of GR (slope -0.983), so the unit cannot literally limit.
**Expected:** At the top of travel, output should be pinned at threshold
regardless of input level (GR == input excess, slope -1.0).
**Actual:** Top of travel gives a 60:1 ratio that lets ~1.7 % of signal
through above threshold. The "Brick Wall" factory preset pins ratio at
60.0 with a name implying brick-wall limiting; the actual behavior is
59:1, not ∞:1.
**Location:** `src/PluginProcessor.cpp:34-38` (param range 1.0..60.0);
`src/dsp/GainComputer.cpp:14-23` (uniform formula, no `ratio == max`
short-circuit); `src/dsp/GainComputer.h:9-13` (docstring lies — claims
"60.0 = infinity:1 (limiter — output pinned at threshold)" but the code
doesn't implement that special case); `src/FactoryPresets.h:24` (Brick
Wall preset).

Recommended fix: in `GainComputer::computeGainReduction`, branch on
`ratio >= kInfinityRatioThreshold` (e.g., `>= 50.0`) and set
`slope = -1.0f` directly. Optionally extend the range past 60 into a
"negative ratio" zone (see QA-UX-009).

### QA-UX-003: No bypass parameter
**Severity:** HIGH
**Owner:** owner:dev-agent
**Context:** REFERENCE.md §2.2 explicitly lists bypass as a required
switch on the faceplate, with the note "sample-accurate, with no latency
discrepancy between bypassed and engaged states."
**Expected:** A bypass button on the faceplate, an APVTS bool param
named e.g. `bypass`, and an override of
`AudioProcessor::getBypassParameter()` to expose it to hosts.
**Actual:** No `bypass` parameter, no GUI bypass button, no
`getBypassParameter()` override. Users get only the host-synthesized
bypass — which works in most DAWs but provides no on-faceplate
indication, and may not be sample-accurate against arbitrary blocks.
**Location:** `src/PluginProcessor.cpp:22-65` (createParameterLayout
omits bypass); `src/PluginProcessor.h:8-69` (no
`getBypassParameter()`); `src/PluginEditor.h:51-52` (no bypass button).

### QA-UX-004: Brand-named user-visible parameter display name (soft knee toggle)
**Severity:** HIGH
**Owner:** owner:dev-agent
**Context:** Trademark scrub is tracked under QA-TM-001 (CRITICAL,
codebase-wide). Calling it out separately here as a UX finding because
it bleeds into the user's first impression of the GUI.
**Expected:** Per project ground rules, neutral vocabulary in user-
visible strings: "Soft Knee" (or "Knee Mode") instead of the legacy
brand-adjacent term.
**Actual:** APVTS parameter display name used a brand-adjacent term
(`PluginProcessor.cpp:50`) — this string is what hosts display in
their generic-control UI. The `setSoftKnee` method on Compressor and the
`softKneeParam_` member also carried the brand in their prior form.
**Location:** `src/PluginProcessor.cpp:47-50`;
`src/PluginEditor.h:51`; `src/dsp/Compressor.h:33`;
`src/dsp/Compressor.cpp:43-46`. The parameter ID `"softKnee"` (renamed
from the legacy brand-adjacent ID) is a state-compatibility concern:
the migration shim in `setStateInformation` handles old presets.

### QA-UX-005: No tooltips or hover help text on any control
**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context:** A first-time user unfamiliar with the reference hardware
class has to guess what "Compression" means (is it ratio? is it amount?)
or what the difference between soft knee off and on is.
**Expected:** Per usability principle, every control should have a
short tooltip describing what it does and what its unit is. JUCE
provides `setTooltip()` on `Component` plus `TooltipWindow`.
**Actual:** No call to `setTooltip` anywhere in the codebase, no
`TooltipWindow` instance.
**Location:** `src/PluginEditor.cpp` (entire constructor) — none of
the sliders, toggle buttons, or preset controls have tooltips.

### QA-UX-006: Threshold-indicator LEDs inherit the dBFS/dBu unit mismatch
**Severity:** HIGH
**Owner:** owner:dev-agent
**Context:** Follow-on from QA-UX-001. The "BELOW" / "ABOVE" LEDs in
the header compare `displayedInputLevel_` (dBFS, derived from
`getMeterInputLevel()`) against the `threshold` parameter (labeled dBu
but actually dBFS). Until QA-UX-001 is fixed, the LEDs work but they
disagree with what the user thinks the threshold knob means. After
QA-UX-001 is fixed, this site needs to be updated to use whatever
unit/scale convention is chosen.
**Expected:** LED comparator and threshold parameter must be in the
same domain. If the parameter changes to dBFS, the LEDs are already
correct and need no change. If dBu is preserved with calibration, the
LED comparison must apply the same calibration shift.
**Actual:** `PluginEditor.cpp:136` reads `threshold` raw and compares
to `displayedInputLevel_` raw — both currently in the same (dBFS) frame
but inconsistent with the displayed unit on the threshold knob.
**Location:** `src/PluginEditor.cpp:135-148` (timerCallback LED logic).

### QA-UX-007: Knob label says "COMPRESSION" but parameter is named "Compression" / id "ratio" — inconsistent vocabulary across surfaces
**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context:** A user who reads the host's parameter list sees
"Compression" (the APVTS display name from
`PluginProcessor.cpp:35`); the GUI knob also shows "COMPRESSION"
(`PluginEditor.cpp:58`); but the parameter ID is `"ratio"`, automation
data references `"ratio"`, and the unit is `:1`. "Compression" is also
ambiguous — it could mean amount, ratio, or threshold to a beginner.
**Expected:** Consistent vocabulary. Pick one of {"Ratio", "Compression
Ratio"} and use it everywhere — APVTS display name, GUI knob label, and
documentation. "Ratio" is the universal vocabulary in this product
class.
**Actual:** Mixed naming. Parameter id `"ratio"`, display name
`"Compression"`, knob label `"COMPRESSION"`.
**Location:** `src/PluginProcessor.cpp:33-38`;
`src/PluginEditor.cpp:58`.

### QA-UX-008: Slider textbox does not display unit suffix in some hosts / standalone
**Severity:** LOW
**Owner:** owner:dev-agent
**Context:** Parameters use
`AudioParameterFloatAttributes().withLabel("dBu")` etc. JUCE's default
`AudioParameterFloat::getText()` includes the label suffix, but the
custom `juce::Slider` in the editor uses
`Slider::TextBoxBelow, false, 72, 18` — width 72 is tight for values
like `-40.0 dBFS`. Need to verify visually, but worth flagging.
**Expected:** Unit visibly displayed under each rotary knob.
**Actual:** Possibly truncated or missing — needs visual check on
standalone build at default size.
**Location:** `src/PluginEditor.cpp:9-11` (textbox dimensions).

### QA-UX-009: Negative-ratio (dynamics inversion) capability not exposed
**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context:** REFERENCE.md §2.1: "Some hardware of this class extended
*past* infinity into negative ratios (dynamics inversion); that is a
signature capability of the class and should be considered optional but
within scope."
**Expected:** Optional (not blocking), but if implemented, the ratio
range would extend past the limiter point into negative ratios where
output decreases as input rises above threshold.
**Actual:** Not present. The header comment in `GainComputer.h:13` is
aspirational — it claims `>60.0` produces negative ratios, but (a) the
parameter range is capped at 60.0 so the user cannot reach there, and
(b) the current `1/ratio - 1` formula produces an asymptote of -1.0 as
ratio increases, not actual negative slopes.
**Location:** `src/dsp/GainComputer.h:9-14` (incorrect docstring);
`src/PluginProcessor.cpp:36` (range cap).

### QA-UX-010: Preset save has no name validation, silently overwrites duplicates, ignores write failures
**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context:** User preset save flow needs to handle invalid characters,
duplicate names, and read-only filesystems gracefully.
**Expected:** Reject or sanitize names containing path separators
(`/`, `\`), dots (`.`, `..`), reserved Windows names (`con`, `prn`,
etc.); confirm overwrite when a preset of the same name already
exists; report write failures to the user.
**Actual:** `PresetManager::saveUserPreset` (lines 73-98):
- Concatenates the name with `.xml` and passes it directly to
  `dir.getChildFile(name + ".xml")`. JUCE will refuse path separators
  but accepts `..` and reserved names; cross-platform safety not
  guaranteed.
- No duplicate-name check — silently overwrites.
- `xml->writeTo(file)` returns a bool that is discarded; failure (e.g.,
  read-only filesystem, disk full, permission denied) produces no user
  feedback.
- `dir.createDirectory()` return value also ignored.

The editor's `onSavePreset` only checks `name.isNotEmpty()` after a
`trim()` — no further validation.
**Location:** `src/PresetManager.cpp:73-98`;
`src/PluginEditor.cpp:189-213`.

### QA-UX-011: Preset delete confirmation does not say "factory presets cannot be deleted" — and silently no-ops on factory selection
**Severity:** LOW
**Owner:** owner:dev-agent
**Context:** `MW160Editor::onDeletePreset` (lines 215-243) early-returns
if `idx < 0 || pm.isFactoryPreset(idx)`. The user gets no feedback —
clicking Delete with a factory preset selected does nothing visible.
**Expected:** Either disable the Delete button when a factory preset
is selected (better), or show a brief "Factory presets cannot be
deleted" message.
**Actual:** Silent no-op.
**Location:** `src/PluginEditor.cpp:215-220`.

### QA-UX-012: Default preset on load is the first factory ("Kick Punch")
**Severity:** LOW
**Owner:** owner:dev-agent
**Context:** When a fresh plugin instance is created, the APVTS
defaults take effect: threshold=0, ratio=1, gain=0, knee=hard,
mix=100 %, link=on. With ratio=1 the compressor is a passthrough — a
new user dropping the plugin on a track hears nothing change. There is
no automatic "default to a useful preset" on instantiation.
**Expected:** Either the default state should be a useful starting
point (e.g., the "Drum Bus Glue" or "Gentle Leveling" preset), or the
preset combobox should default to a sensible factory and load it on
first instantiation.
**Actual:** Defaults give passthrough behavior. Bad first impression.
**Location:** `src/PluginProcessor.cpp:26-65` (default values);
`src/PluginProcessor.cpp:73-84` (program handling).

### QA-UX-013: Factory preset coverage is drum-heavy, no clear vocal preset
**Severity:** LOW
**Owner:** owner:dev-agent
**Context:** REFERENCE.md does not require a specific preset list, but
basic UX practice is to ship with presets covering the main use cases.
**Expected:** At least one preset each for drums, drum bus, vocal,
bass, parallel, gentle/leveling, and limiter.
**Actual:** Of 7 factory presets, 5 are drum-oriented (Kick Punch,
Snare Snap, Drum Bus Glue, Parallel Smash implies drums, Brick Wall is
generic) plus Bass Control and Gentle Leveling. No "Vocal" preset.
"Gentle Leveling" could serve, but is not labeled as such.
**Location:** `src/FactoryPresets.h:18-24`.

### QA-UX-014: GR meter has no scale legend
**Severity:** LOW
**Owner:** owner:dev-agent
**Context:** `LedMeter` for GR runs 0..-40 dB (`LedMeter.h:34-37`),
but the GUI does not draw any tick marks or numbers next to the meter.
A user cannot tell whether the bar shows -3 dB or -30 dB of GR without
knowing the segment count internally.
**Expected:** Tick marks or numeric labels at e.g. 0, -5, -10, -20,
-40 dB.
**Actual:** Plain LED ladder, no legend.
**Location:** `src/PluginEditor.cpp:309-331` (resized — only sets
bounds, no tick painting); `src/gui/LedMeter.h:25-81` (paint method —
draws segments only).
