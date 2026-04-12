# Design Review findings (2026-04-11)

## Summary

- **Build status:** Clean. Both `MW160_Standalone` and `MW160_VST3`
  targets relink successfully from the current source tree in Release
  config. Only LTO-wrapper noise is emitted. **Important build-system
  finding surfaced during this review:** `cmake --build build --target
  MW160` alone does *not* relink the Standalone or VST3 artefacts --
  it stops at the `MW160_SharedCode` static library. The binaries under
  `build/MW160_artefacts/Release/Standalone/MW160` and
  `build/MW160_artefacts/Release/VST3/MW160.vst3/.../MW160.so` were
  stale at the start of this review (dated 2026-04-07) even though
  source files were from today. `MW160_Standalone` and `MW160_VST3` are
  the correct subtargets to drive. See DESIGN-REVIEW-001 below.
- **Rendering approach used:** **B (Xvfb + Standalone + xwd).** A custom
  headless host was deemed too much yak-shaving given that xvfb-run +
  xwd + xwdtopnm + PIL was already on the box. Approach A would have
  required a custom CMake target. Approach B produced a clean rendered
  frame after rebuilding the Standalone target.
- **Spec-compliance verdict:** The editor implements the majority of
  VISUAL_SPEC.md §§1-13. All major layout regions are in place and
  dimensioned per spec. Palette, knob layer order, toggle pill idiom,
  meter segment counts, and screw rendering all match. The significant
  new drift surfaced by actual rendering is a **header wordmark /
  sublabel vertical collision** (DESIGN-REVIEW-002) and a few smaller
  textual / layout issues enumerated below. None of the new findings
  reach CRITICAL severity; DESIGN-REVIEW-001 (build system) is HIGH
  because it can easily cause stale-binary confusion in CI.

## Rendering method used

Approach **B** (Xvfb + Standalone + xwd).

Sequence:

1. Rebuilt the Standalone and VST3 subtargets explicitly
   (`--target MW160_Standalone`, `--target MW160_VST3`) after
   discovering the shared-code-only build left stale plugin binaries
   behind.
2. Launched `/home/mwoolly/projects/mw160/build/MW160_artefacts/Release/Standalone/MW160`
   against a pre-existing `xvfb-run -a -s "-screen 0 1600x900x24"`
   display on `:99`.
3. Located the X11 window via `xwininfo -root -tree` and captured it
   with `xwd -id <win-id>`.
4. Converted the XWD dump with `xwdtopnm` and saved the PNM as PNG via
   a short Python/PIL one-liner.
5. Cropped the JUCE Standalone host chrome (title bar, menu, audio-muted
   warning bar) to isolate the exact 900x360 editor region using
   pixel-boundary probing.

Rendered artefacts:

- `/home/mwoolly/projects/mw160/qa-artifacts/editor-default-900x360.png`
  -- full Standalone window capture (908x424: editor + host chrome).
- `/home/mwoolly/projects/mw160/qa-artifacts/editor-900x360.png`
  -- cropped to exactly the 900x360 editor area for spec comparison.

Min-size (720x288) and max-size (1440x576) renders were **not**
produced because `xdotool` / `wmctrl` were not available to resize the
JUCE Standalone host window from outside. The aspect lock and min/max
size limits are instead verified statically by code inspection of
`aspectConstrainer_.setSizeLimits(720, 288, 1440, 576)` and
`.setFixedAspectRatio(kRefW / kRefH)` in `src/PluginEditor.cpp:254-256`.
The impl agent's scaleR/scaleI helpers make the layout proportional by
construction, so proportional correctness at non-default sizes follows
from the default-size correctness with high confidence.

## Spec-compliance matrix (vs VISUAL_SPEC.md §§1-13)

| Spec section | Status | Notes |
|--------------|--------|-------|
| §1 Window/canvas | PASS | 900x360 default confirmed by pixel measurement. Aspect lock + min/max via `ComponentBoundsConstrainer` verified in source. `setResizable(true, true)` set. |
| §2 Layout regions | PARTIAL | All five regions (header, meter strip, knob row, switch cluster, footer) are at the spec bounds. Header wordmark/sublabel vertical layout clashes (DESIGN-REVIEW-002). Meter tick column too narrow for "-48" label (DESIGN-REVIEW-003). |
| §3 Palette | PASS | `src/gui/Palette.h` defines every role from §3.1 with exact hex values. `bgPanel` at (170,200) measured as rgb(30,30,30) = #1e1e1e; `surfaceLow` sampled inside knob/switch panels as rgb(34,34,34) = #222222. No `0xff…` literals in `PluginEditor.cpp` (grepped, zero matches). Four per-state switch hex literals survive inside `CustomLookAndFeel::drawToggleButton`, which the impl agent documented; those are spec-sourced from §7.3's state table. |
| §4 Typography | PARTIAL | Font helper at `src/gui/Fonts.h` falls through to JUCE defaults because Inter / JetBrains Mono .ttf files are not checked in and `juce_add_binary_data` is not wired in `CMakeLists.txt`. Already tracked under DESIGN-IMPL-002. The wordmark two-pass engrave draw in §4.3 is implemented per spec in `PluginEditor.cpp:490-508`. |
| §5 Knobs | PASS | Four 96 px rotaries (`scaleI(colX + 17, 104, 96, 96, ...)`). `drawRotarySlider` follows the 9-step layer order from §5.6. Arc track, fill, LED tip, outer ring, body gradient, brushed highlight band, inner cap, pointer, active overlay are all present and match the rendered output. Rotary range is set to 1.25π / 2.75π = 270° (§5.2 intent). Already noted spec prose / formula reconciliation under DESIGN-IMPL-005. |
| §6 Meters | PARTIAL | 28/28/20 segment counts confirmed (`LedMeter` ctor calls in `PluginEditor.h:85-87`). Amber/red GR split at -6 dB implemented in `LedMeter.h:82-95`. Per-segment glow halo implemented. **However** the IN/OUT shared tick column is drawn `8 px` wide (`shared_w = 8.0f * sx`) at `shared_x = 54.0f * sx`, which is too narrow to fit the `-48` label; see DESIGN-REVIEW-003. |
| §7 Toggle switches | PARTIAL | Pill-with-LED idiom is correctly implemented in `CustomLookAndFeel::drawToggleButton`. HARD/SOFT label toggling via property map works. **Label wording drifts from spec §7.4** for STEREO LINK ("LINK OFF" / "LINK ON" instead of "OFF" / "ON") and BYPASS ("ACTIVE" / "BYPASS" instead of "OFF" / "ON"). The impl agent already flagged this as "Deviation 1" in their report. It is a real spec drift and the orchestrator needs to pick strict spec vs the impl's readability argument. See DESIGN-REVIEW-004. |
| §8 Preset browser | PASS | Inline horizontal browser in the footer at the correct bounds (`scaleI(16, 328, 540, 24, ...)` etc.). Combo, SAVE, DEL, version label all visible and positioned per §2.5 / §8.2. AlertWindow theming override is set via colour IDs in `CustomLookAndFeel` constructor per §8.4. |
| §9 Fasteners / screws | PASS | Four corner screws at (16,16), (16,36), (884,16), (884,36), drawn with `mw160::Screws::drawScrew` per §9.3 (body, outer ring, radial gradient, rotated cross-slot, highlight arc). Per-screw rotation angles match the §9.3 suggestion (17°, -22°, 8°, -13°). Pixel-sampled screw centers read as near-black (~13/13/13) because the slot crossing lands exactly at center, consistent with the spec. |
| §10 DPI awareness / scaling | PARTIAL | Vector-only rendering ✓. `scaleI` / `scaleR` helpers do proportional mapping. `snap1px` helper is present in `src/gui/Geometry.h` but unused (DESIGN-IMPL-003). Since `cmake --build --target MW160` does not relink plugin binaries, resize limits at non-default scales were not live-verified; the `ComponentBoundsConstrainer` setup is correct in source. |
| §11 Accessibility | PARTIAL | Tab focus order set 1..9 per §11.4 (matches spec order). `setTitle` / `setHelpText` set on knobs and toggles. `textDim` 9pt captions under knob value readouts violate the §11.2 "textDim only ≥ 11 pt" rule; see DESIGN-REVIEW-005. Hit-target padding deferred (DESIGN-IMPL-004). |
| §12 Rendering approach | PASS | Procedural JUCE Graphics only. No `BinaryData` graphics; fonts are the only (not-yet-wired) binary asset. No SVG. No raster assets under `assets/`. |
| §13 Non-goals | PARTIAL | The editor surface is clean (zero brand strings in `PluginEditor.{h,cpp}`, `CustomLookAndFeel.h`, `LedMeter.h`, `Palette.h`, `Fonts.h`, `Geometry.h`, `Screws.h`, `MeterModeButton.h`). **However** `src/PluginProcessor.cpp:55` still sets the parameter *display name* to `"OverEasy"`, which is the string that propagates to the host's parameter list in VST3 and AU. That is a visible brand leak on the plugin's user-facing surface even though the UI *label* is KNEE/HARD/SOFT. See DESIGN-REVIEW-006 -- this overlaps with QA-TM-001 but is worth calling out separately because the spec explicitly allows the APVTS *ID* to stay and was silent on the display name. |

## Findings

### DESIGN-REVIEW-001: `cmake --build --target MW160` leaves plugin binaries stale

**Severity:** HIGH
**Owner:** owner:design-agent / owner:build-agent

**Context:** At the start of this review I ran
`cmake --build /home/mwoolly/projects/mw160/build --config Release --target MW160 --parallel`
per the orchestrator instructions. The command completed successfully
(`[ 28%] Built target MW160_DSP`, `[100%] Built target MW160`) but:

- `build/MW160_artefacts/Release/Standalone/MW160` was dated
  **2026-04-07 04:06** (five days stale).
- `build/MW160_artefacts/Release/VST3/MW160.vst3/Contents/x86_64-linux/MW160.so`
  was also dated **2026-04-07 04:06**.
- Source files in `src/` were dated 2026-04-11 today (impl agent's pass).
- `build/MW160_artefacts/Release/libMW160_SharedCode.a` was dated
  2026-04-11 (freshly built).

Rendering the stale Standalone against Xvfb produced the *previous*
editor layout (vertical two-column, raw "OverEasy" button, old
LedMeter), which was briefly misinterpreted as a total impl regression
until the mtime check surfaced the mismatch. Re-running with
`--target MW160_Standalone` (and separately `--target MW160_VST3`)
relinked the artefacts correctly and the Xvfb render then showed the
new editor.

**Expected:** `--target MW160` should be a sufficient convenience target
that rebuilds *all* plugin formats (or at least the ones in the
`FORMATS` list in `CMakeLists.txt`), OR the orchestrator's build step
needs to explicitly enumerate format subtargets.

**Actual:** `--target MW160` only rebuilds `MW160_SharedCode.a` (the
intermediate static library) and does not relink the per-format
artefacts. Downstream tooling that inspects the linked binaries (e.g.
pluginval against the VST3, Standalone-based visual review like this
pass, or any packaging step) will silently use stale binaries.

**Location:** `CMakeLists.txt`; build system target graph.
**Evidence:** mtime diff in `build/MW160_artefacts/Release/` captured
via `stat -c '%y'` during this review. No artefact on disk for this,
but the discrepancy was reproduced on demand.

**Suggested fix:** Add an explicit `add_custom_target(MW160_All ALL
DEPENDS MW160_VST3 MW160_AU MW160_Standalone)` or similar umbrella
target, and either point the orchestrator at that target or update the
orchestrator's build command to call each format subtarget
individually. Whichever path is chosen, the CI `build-and-validate`
workflow should be audited to confirm it does not hit the same
silent-stale trap.

---

### DESIGN-REVIEW-002: Wordmark and sublabel vertically overlap in the header

**Severity:** MEDIUM
**Owner:** owner:design-agent

**Context:** I rendered the editor against Xvfb and cropped to the exact
900x360 editor region. I then scanned the header column at x=60..440,
y=0..60 for dark-ink pixels (`textInk #0c0c0c`) to locate the wordmark
glyph bounding box and the sublabel ink box separately.

Measurements:

- Wordmark "mw160" ink bbox: x=66..145, y=15..48. Glyph height = **34 px**,
  glyph width = 80 px.
- Sublabel "VCA COMPRESSOR" ink bbox (broadened threshold r<50 to catch
  the 75%-alpha composited ink): x=64..173, y=**30..48**.

The sublabel's ink starts at y=30, which is **18 pixels inside the
wordmark's ink area** (y=15..48). In the rendered image the effect is
that "VCA COMPRESSOR" sits partially underneath / tangled with the
descender-free baseline of the "mw160" wordmark rather than cleanly
below it.

Root cause: in `PluginEditor.cpp:493` the wordmark `drawText` target rect
is built as `Rectangle<float>(64*sx, 6*sy, 360*sx, 34*sy)` -- a 34 px
height with a 28 pt bold font rendered at the fallback font (Inter is
not yet embedded). The fallback bold's actual pixel ascender+descender
comes out larger than 28 px once anti-aliased, pushing the glyph bottom
past y=40. The sublabel area immediately follows at `(64, 38, 360, 16)`
(`PluginEditor.cpp:504`). With no gap between 34-tall wordmark box and
38-start sublabel box, a single pixel of padding is the only separation
between them, and the actual rendered glyphs exceed their nominal boxes.

**Expected:** VISUAL_SPEC.md §2.1 prescribes a "Wordmark + sublabel
column: `(64, 6, 360, 48)`" with "`mw160` 28 pt over `VCA COMPRESSOR`
10 pt". In a clean header the 28 pt wordmark and 10 pt sublabel sit
comfortably stacked inside 48 px of vertical budget, not overlapping.

**Actual:** The impl splits the 48 px column as `(..., 6, ..., 34)` for
the wordmark and `(..., 38, ..., 16)` for the sublabel, which leaves
no vertical gap between them. With the fallback bold font the wordmark
glyph actually consumes 34 px of ink and the sublabel draws on top of
its lower third.

**Location:** `src/PluginEditor.cpp:493-507`.
**Evidence:** `/home/mwoolly/projects/mw160/qa-artifacts/editor-900x360.png`
(look at the header, left side, around "mw160 / VCA COMPRESSOR").

**Suggested fix:** Either

1. Keep the wordmark `drawText` area at `(64, 6, 360, 48)` per spec and
   move the sublabel slightly outside the wordmark area (e.g.
   `(64, 44, 360, 14)`) so the 10 pt caption clearly sits *below* a 28 pt
   wordmark that is vertically centered within its own ~34 px ink; or
2. Render both in a single pass using a `TextLayout` with a GlyphArrangement
   that computes exact ascent/descent and stacks the two baselines with
   an explicit gap; or
3. Wait until Inter is embedded (DESIGN-IMPL-002) -- Inter's metrics will
   likely be slightly tighter than the fallback and the clash may
   disappear without layout changes -- but still keep an explicit gap so
   this is not fragile against future font swaps.

This will become much more visible once Inter is embedded, because
Inter Bold at 28 pt sits taller in its em-box than most JUCE fallback
sans-serifs.

---

### DESIGN-REVIEW-003: IN/OUT meter shared tick column is too narrow for "-48" label

**Severity:** LOW
**Owner:** owner:design-agent

**Context:** VISUAL_SPEC.md §6.4 says "IN and OUT ladders share a single
tick column drawn between them at `x ≈ 56` and `x ≈ 96`. The shared
column shows: `+6`, `0`, `-6`, `-12`, `-24`, `-48`."

The impl defines:

```
const float shared_x = 54.0f * sx;
const float shared_w = 8.0f  * sx;
```

(`src/PluginEditor.cpp:601-602`)

8 px is narrower than a 3-glyph label (`"-48"`, `"-12"`, `"-24"`) at
8 pt in any reasonable sans-serif. The `drawText` call uses
`Justification::centred`, so labels are centered inside an 8 px box,
causing glyph overflow on both sides or visual clipping at the column
edge.

**Expected:** The shared tick column is wide enough to fit
3-character dB labels without truncation.
**Actual:** The column is 8 px wide. Labels render but may clip or
visually overlap the adjacent ladder fills when rendered with a
non-fallback font.
**Location:** `src/PluginEditor.cpp:601-607`.
**Evidence:** `/home/mwoolly/projects/mw160/qa-artifacts/editor-900x360.png`
(look between the IN and OUT ladders). In the fallback-font render the
ticks are readable but the digit kerning clearly overflows the 8 px box.

**Suggested fix:** Widen to `shared_w = 16.0f * sx` (still fits inside
the ~18 px gap between the IN ladder's right edge at x=52 and the OUT
ladder's left edge at x=64 at 900 px reference width) or 14 px. Also
consider nudging the IN/OUT ladder spacing: the current layout packs
the three ladders very close together (IN at x=24, OUT at x=64, GR at
x=112), leaving little room for tick text. §2.2 gives the spec's
prescribed positions; those are the bounds the impl should not violate.

---

### DESIGN-REVIEW-004: Switch labels drift from spec wording

**Severity:** LOW
**Owner:** owner:design-agent

**Context:** The impl agent's "Deviation 1" re-worded two switches:

| Switch | Spec §7.4 OFF | Spec §7.4 ON | Impl OFF | Impl ON |
|--------|---------------|--------------|----------|---------|
| STEREO LINK | `OFF` | `ON` | `LINK OFF` | `LINK ON` |
| BYPASS | `OFF` | `ON` | `ACTIVE` | `BYPASS` |

The impl argues this is needed for affordance when two switches sit
next to each other. The live render confirms both drifted labels.

**Expected:** VISUAL_SPEC.md §7.4 table.
**Actual:** Rendered pills show `LINK ON` and `ACTIVE`. The spec has no
affordance allowance for these differences.
**Location:** `src/PluginEditor.cpp:185-196`.
**Evidence:** `/home/mwoolly/projects/mw160/qa-artifacts/editor-900x360.png`
(switch cluster at the right side of the body).

**Suggested fix:** The impl agent's argument is reasonable but the spec
is binding. Two options:

1. Revert to strict spec (`OFF` / `ON` for both STEREO LINK and BYPASS)
   and accept that readers must infer affordance from the adjacent
   label column on the left of the pill.
2. Update VISUAL_SPEC.md §7.4 to explicitly allow longer-form labels
   and keep the impl as-is.

Either resolution is fine -- this finding is filed so the
spec/implementation pair stops disagreeing.

---

### DESIGN-REVIEW-005: Knob unit captions use `textDim` at 9 pt, violating contrast rule

**Severity:** LOW
**Owner:** owner:design-agent

**Context:** VISUAL_SPEC.md §11.2 explicitly states: "`textDim` is only
allowed for text 11 pt and larger in this spec; the impl agent must use
`textMid` for any 9 pt body text on dark backgrounds."

The impl's `setupUnitLabel` in `PluginEditor.cpp:72-80` sets:

```
label.setFont(mw160::Fonts::interRegular(9.0f));
label.setColour(juce::Label::textColourId, mw160::Palette::textDim);
```

-- that is 9 pt body text in `textDim`, directly violating §11.2. The
`textDim` contrast ratio on bgPanel is 3.8:1, which fails WCAG 2.1 AA
for body text (need 4.5:1).

Additionally the `PRESET` label in the footer (`PluginEditor.cpp:150`)
is set to `textMid` at 9 pt -- which is correct per §11.2. So the bug
is specifically in the unit captions, not a blanket issue.

**Expected:** §11.2 -- 9 pt captions use `textMid` on dark background.
**Actual:** Unit captions ("dB", "ratio", "dB", "(mix)") use `textDim`.
**Location:** `src/PluginEditor.cpp:72-80`.
**Evidence:** Static analysis of `setupUnitLabel`; rendered image
shows unit captions are indeed barely visible at the bottom of each
knob column (`/home/mwoolly/projects/mw160/qa-artifacts/editor-900x360.png`).

**Suggested fix:** Change the `textColourId` in `setupUnitLabel` from
`mw160::Palette::textDim` to `mw160::Palette::textMid`. One-line change.

---

### DESIGN-REVIEW-006: `"OverEasy"` parameter display name leaks to host UI

**Severity:** HIGH
**Owner:** owner:qa-agent (overlaps QA-TM-001) / owner:design-agent

**Context:** VISUAL_SPEC.md §14.2 explicitly permits the APVTS
*parameter ID* `"overEasy"` to stay for state-restore compatibility,
with the requirement that "only the *UI label* changes to
`KNEE / HARD / SOFT`". The impl agent followed this: the editor UI uses
KNEE/HARD/SOFT, and an explanatory comment was added at the parameter
declaration.

However, at `src/PluginProcessor.cpp:54-55` the parameter is declared:

```
juce::ParameterID{"overEasy", 1},
"OverEasy",
```

The second argument to `AudioParameterBool` / `AudioParameterChoice`
is the parameter's *display name*, which is separate from the ID and
is what shows up in the host's parameter automation list (Ableton
Live's parameter menu, Logic's Smart Controls, Studio One's control
linking, Cubase's generic editor, etc.). That display name is
currently `"OverEasy"` -- a brand term that the spec's §0 and §13
prohibit on the plugin's user-facing surface.

The spec §14.2 talks about the *ID* staying and the *UI label*
changing, but is silent on the APVTS *display name*. The impl agent
(understandably) read this as permission to leave the display name
alone since it is "pre-existing" and tracked under QA-TM-001. I am
filing this as a separate finding because:

1. The display name is genuinely user-facing (hosts show it in
   parameter dropdowns, generic controls, CV-ratio mappings, etc.)
   even though the plugin's own editor hides it.
2. QA-TM-001 is the CRITICAL umbrella ticket for DSP-layer brand scrub,
   and the display name sits in `PluginProcessor.cpp` which is *not*
   part of the DSP layer -- so it risks getting lost between the
   design review scope and the QA-TM-001 scope.
3. Fixing the display name without touching the ID is a one-line,
   zero-state-risk change (the ID is what state trees serialize).

**Expected:** Parameter display name is a neutral term -- most obvious
choice is `"Knee"` with the `AudioParameterChoice`/`AudioParameterBool`
display strings being `"Hard"`/`"Soft"`, or simply `"Knee Shape"` for
a boolean. No brand term is rendered by the host.

**Actual:** VST3 and AU parameter tables show `"OverEasy"` to the user.

**Location:** `src/PluginProcessor.cpp:54-55`.
**Evidence:** Grep confirmation (see `src/PluginProcessor.cpp:55`).

**Suggested fix:** Change the display name string from `"OverEasy"` to
`"Knee"` (or `"Knee Shape"`). Keep the `ParameterID{"overEasy", 1}`
alone. One-line change, no state-migration impact.

---

### DESIGN-REVIEW-007: Knob metallic finish reads as near-flat under current palette

**Severity:** LOW (observation, not a spec violation)
**Owner:** owner:design-agent (informational)

**Context:** The knob body gradient goes
`knobMetalHigh #6c6c6c -> knobMetalLow #2c2c2c` vertically, with a
1 px brushed highlight band at 35% from top. Rendered against the
surrounding `surfaceLow #222222` panel, the knob reads as a mostly
uniform dark disc -- the brushed-metal signal the spec wants to
convey (§5.1 "milled aluminum knob with anodized cap -- period-correct
for the class without copying any specific cap") is weak.

This is a **subjective observation** and I am not filing it as a drift
from spec -- the impl has faithfully used the exact hex values in §3.
It is worth noting for the design-research-agent that the palette as
currently specified may undershoot the "milled aluminum" intent, and a
future spec revision could consider widening the gradient contrast
(e.g. `knobMetalHigh -> #858585` or similar) without changing the
procedural draw code.

**Expected:** Knob reads as brushed metal with visible gradient.
**Actual:** Gradient is present but subtle; knob reads as dark disc.
**Location:** `src/gui/Palette.h:35-36`; VISUAL_SPEC.md §3 role table.
**Evidence:** `/home/mwoolly/projects/mw160/qa-artifacts/editor-900x360.png`
(look at any of the four knobs in the center).

**Suggested fix:** None at this pass -- flagging for a future palette
tuning session. The correct mechanism is a spec update, not an editor
fix.

---

## Cross-check vs the impl agent's self-reported gaps

I verified each of the impl agent's self-filed gaps (DESIGN-IMPL-001..007)
against the rendered output and the source:

- **DESIGN-IMPL-001 (BYPASS visual-only):** Confirmed. No `"bypass"`
  key in the APVTS layout (`grep "bypass"` shows only the impl's
  explanatory comments). The button renders but has no attachment.
- **DESIGN-IMPL-002 (fonts not embedded):** Confirmed. `assets/fonts/`
  is empty; `CMakeLists.txt` has no `juce_add_binary_data` call. The
  fallback JUCE bold shows in the rendered image and is what causes
  DESIGN-REVIEW-002 to bite harder than it otherwise would.
- **DESIGN-IMPL-003 (snap1px unused):** Confirmed.
  `grep "snap1px" src/` only matches the declaration, no call sites.
- **DESIGN-IMPL-004 (hit-target padding):** Confirmed.
  `setBounds` for toggles is `140 x 36`; no invisible padding children
  are added. Below WCAG minimum short axis.
- **DESIGN-IMPL-005 (270 deg vs spec formulae):** Confirmed. The impl
  uses `1.25 * pi` / `2.75 * pi` which is 270°. Spec prose §5.2 still
  reads "7pi/6 -> 7pi/3 = 7pi/6 rad total = 210°" which is
  self-contradictory. Spec prose needs an update; impl is correct.
- **DESIGN-IMPL-006 (tick columns not mode-aware):** Confirmed. The
  impl's `onModeChange` callback only updates the three meter labels'
  colours, not the tick text drawn from `paint()`. The tick text is
  drawn unconditionally in `textDim`.
- **DESIGN-IMPL-007 (DSP brand scrub):** Out-of-scope per orchestrator;
  tracked under QA-TM-001. No re-file. DESIGN-REVIEW-006 above
  surfaces one sub-issue (the parameter *display name*) that is
  adjacent to but not covered by QA-TM-001 as narrowly scoped.

All seven are accurate. No corrections to the impl agent's
self-assessment.

---

## Summary of new findings (not already filed by design-impl)

| ID | Severity | One-line |
|----|----------|---------|
| DESIGN-REVIEW-001 | HIGH | `cmake --target MW160` leaves Standalone/VST3 binaries stale; use format subtargets |
| DESIGN-REVIEW-002 | MEDIUM | Header wordmark and sublabel vertically overlap in the rendered editor |
| DESIGN-REVIEW-003 | LOW | IN/OUT meter shared tick column is 8 px wide, too narrow for `-48` |
| DESIGN-REVIEW-004 | LOW | STEREO LINK / BYPASS labels drift from spec §7.4 wording |
| DESIGN-REVIEW-005 | LOW | Knob unit captions use `textDim` at 9 pt, violating §11.2 contrast rule |
| DESIGN-REVIEW-006 | HIGH | `"OverEasy"` parameter display name leaks to host UI (adjacent to QA-TM-001 but not covered) |
| DESIGN-REVIEW-007 | LOW (info) | Knob metal gradient reads near-flat under current palette -- spec tuning suggestion |

Counts by severity of *new* findings:

- CRITICAL: 0
- HIGH: 2 (DESIGN-REVIEW-001 build system, DESIGN-REVIEW-006 param display name)
- MEDIUM: 1 (DESIGN-REVIEW-002 wordmark overlap)
- LOW: 4 (DESIGN-REVIEW-003, -004, -005, -007)

Total new findings: **7**.
