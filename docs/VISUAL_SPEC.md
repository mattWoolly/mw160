# mw160 Visual Specification

**Status:** authoritative for editor implementation
**Owner of decisions:** design-research-agent (this document)
**Owner of source:** design-impl-agent (PluginEditor, CustomLookAndFeel, LedMeter, any new components)

This document is the implementable visual contract for the mw160 plugin
editor. It commits to specific numbers, colors, fonts, and rendering
approaches so the impl agent can build the editor without further
guesswork. Rationale and the generic visual language live in
`REFERENCE.md` (read §3 first); this document does not duplicate that
material.

When this document and `REFERENCE.md` disagree, this document wins for
the editor implementation, and `REFERENCE.md` should be updated with a
documented exception.

---

## 0. Naming and trademark hygiene (binding)

The impl agent inherits these constraints from the orchestrator and
must enforce them in every artifact:

- The only product/working name that may appear anywhere in the editor,
  in source comments, in asset filenames, in preset names, or in
  tooltips is **`mw160`**.
- No manufacturer names, model numbers, or trademarked feature names —
  not even as legacy comments. Any brand-adjacent string in
  `src/PluginEditor.h` and `src/PluginEditor.cpp` is a violation and
  must be removed in the impl pass. The control is **`KNEE`** with
  states **HARD** and **SOFT**.
- The existing source comments referencing specific hardware (e.g. the
  doxygen blocks in `PluginEditor.h` and `LedMeter.h`) must be rewritten
  to reference only "a classic VCA feedback compressor" or "1U rack
  audio processor" in generic terms.
- Subtitle text on the faceplate is **`VCA COMPRESSOR`** — no model
  designators, no era references.
- See §13 (Non-goals) for the full do-not-do list.

---

## 1. Window / canvas dimensions

### 1.1 Default size

**900 × 360 px.**

This is the canonical size at scale factor 1.0. The aspect ratio is
2.5 : 1. A literal 1U rack ratio (≈ 8.75 : 1) would force the window to
be unusably short on a typical 1080p plugin host, so the editor takes
the 1U *visual language* (centered horizontal layout, brushed faceplate
band, rack ears) and applies it at a usable plugin aspect. State this
trade-off in the impl agent's commit message: the design is "1U-rack
*inspired*", not 1U-rack *literal*.

### 1.2 Resizable

Yes. Constrained by an aspect ratio lock.

- **Aspect ratio:** locked at 2.5 : 1 (= 900 / 360).
- **Min size:** 720 × 288 px.
- **Max size:** 1440 × 576 px.
- **Resize handle:** standard JUCE bottom-right corner resizer.

Use `juce::ResizableCornerComponent` plus `setResizable(true, true)`
and a `juce::ComponentBoundsConstrainer` with `setFixedAspectRatio(2.5)`.
All layout in `resized()` must compute regions from `getWidth()` and
`getHeight()` — no hard-coded pixel offsets except where this spec
explicitly says so.

### 1.3 Coordinate system in this document

Numeric bounds in §2 are given **at the default 900 × 360 size**. The
impl agent expresses them at runtime as proportions of the current
width and height so that resizing works without re-laying-out manually.

---

## 2. Layout regions

Top-down map at default size. All bounds are `(x, y, w, h)` in pixels.

```
+--------------------------------------------------------------+  y=0
|  HEADER: faceplate band, logo, fasteners, threshold LEDs     |  60 px
+--------------------------------------------------------------+  y=60
|                                                              |
|  BODY                                                        |  240 px
|   ┌──────────┬───────────────────────────┬──────────────┐    |
|   │ METER    │ KNOB ROW                  │ SWITCH       │    |
|   │ STRIP    │ (4 large rotaries)        │ CLUSTER      │    |
|   │          │                           │ + METER MODE │    |
|   └──────────┴───────────────────────────┴──────────────┘    |
|                                                              |
+--------------------------------------------------------------+  y=300
|  FOOTER: preset browser, save / del, version text            |  60 px
+--------------------------------------------------------------+  y=360
```

### 2.1 Header — `(0, 0, 900, 60)`

Purpose: faceplate identity. Brushed-metal band with engraved-look
"mw160" wordmark, "VCA COMPRESSOR" sublabel, four corner fasteners
(see §9), and the two threshold-state LEDs (BELOW / ABOVE).

Sub-regions inside the header at default size:

- **Left fastener group:** `(8, 8, 44, 44)` — two screw glyphs vertically
  centered inside, at `(16, 16)` and `(16, 36)` (radius 4 each).
- **Wordmark + sublabel column:** `(64, 6, 360, 48)`. Centered text
  block left-aligned to `x = 80`. "mw160" 28 pt over "VCA COMPRESSOR"
  10 pt tracking +2.
- **Threshold LEDs strip:** `(440, 14, 200, 32)`. Two LED+label pairs
  laid out horizontally, "BELOW" then "ABOVE" with 16 px gap.
- **Right fastener group:** `(848, 8, 44, 44)` — mirror of the left
  group.
- **Header / body separator:** `(8, 59, 884, 1)` solid hairline in
  `#2a2a2a` (panel border).

### 2.2 Meter strip — `(8, 68, 156, 224)`

Purpose: input level, output level, and gain reduction. The strip is a
single recessed panel inside the body region.

Inside the strip:

- **Strip background panel** (rounded 4 px): `(8, 68, 156, 224)`.
- **Three vertical LED ladders**, each `(W=28, H=176)`:
  - IN  ladder bounds: `(24, 96, 28, 176)`.
  - OUT ladder bounds: `(64, 96, 28, 176)`.
  - GR  ladder bounds: `(112, 96, 28, 176)`. Slightly offset right by
    8 px so the GR meter visually separates from the in/out pair.
- **Top labels** above each ladder, centered on its column, font
  9 pt bold tracking +1: `IN`, `OUT`, `GR` at `y = 76, h = 14`.
- **Bottom dB scale ticks** under each ladder at `y = 274, h = 14`,
  font 8 pt: top tick label "+6" / "0" / "-12" / "-24" / "-48" — these
  are screen-printed at the *side* of the LED ladder, not under it; see
  §6 for tick placement.

### 2.3 Knob row — `(184, 68, 520, 224)`

Purpose: the four rotary controls — `THRESHOLD`, `RATIO`, `OUTPUT`,
`MIX`. Centered horizontally inside this region.

- **Panel** (rounded 4 px) `(184, 68, 520, 224)`, surface color
  `#222222`, 1 px hairline border `#323232`.
- **Four equal columns** of width 130 px starting at `x = 184`:
  - col0: `(184, 68, 130, 224)` — THRESHOLD
  - col1: `(314, 68, 130, 224)` — RATIO
  - col2: `(444, 68, 130, 224)` — OUTPUT
  - col3: `(574, 68, 130, 224)` — MIX
- Inside each column:
  - **Top label** (knob name): `(col_x + 8, 80, 114, 16)`, 11 pt bold
    tracking +1, color text-bright.
  - **Knob**: centered, diameter 96 px, bounds
    `(col_x + 17, 104, 96, 96)`.
  - **Value readout**: `(col_x + 8, 208, 114, 18)`, 11 pt mono digits,
    color text-bright. The readout shows the current value with units
    (`-18.0 dB`, `4.0:1`, `+6.0 dB`, `100%`).
  - **Unit caption** (small grey, beneath value): `(col_x + 8, 228, 114,
    14)`, 9 pt, color text-dim.
- The MIX knob is the modern-addition control (REFERENCE.md §2.5) and
  must carry a small "(MIX)" caption in text-dim under its label so the
  user knows it is not part of the historical control set.

### 2.4 Switch cluster + meter mode — `(720, 68, 172, 224)`

Purpose: the binary toggles and the meter source selector.

- **Panel** (rounded 4 px) `(720, 68, 172, 224)`, same surface as knob
  row.
- **KNEE switch** (HARD / SOFT, 2-state pill) at `(736, 92, 140, 36)`.
- **STEREO LINK switch** (OFF / ON, 2-state pill) at `(736, 136, 140, 36)`.
- **BYPASS switch** (OFF / ON, 2-state pill, red LED when bypassed)
  at `(736, 180, 140, 36)`.
- **Meter mode selector** (3-state segmented: IN | OUT | GR) at
  `(736, 232, 140, 36)`. This selector determines what the meter strip
  emphasizes if the impl agent later chooses a single-meter variant; in
  the current 3-meter design it merely highlights the corresponding
  ladder with a brighter top label. Implement it as a small segmented
  control even in the 3-meter layout, because (a) it preserves the
  hardware identity of "one meter, mode-switched" and (b) it lets a
  later flavor variant switch to a single VU sweep without re-laying
  out.

### 2.5 Footer — `(0, 300, 900, 60)`

Purpose: preset browser and version line.

- **Footer band** (gradient surface, slightly lifted from background)
  `(0, 300, 900, 60)`.
- **Preset label** "PRESET" at `(16, 312, 60, 14)`, 9 pt bold tracking +1.
- **Preset combo box** at `(16, 328, 540, 24)`. Width is the largest
  region inside the footer.
- **Save button** at `(564, 328, 64, 24)`, label "SAVE".
- **Delete button** at `(636, 328, 64, 24)`, label "DEL".
- **Version text** right-justified at `(708, 332, 176, 16)`, 9 pt
  text-dim, content "mw160  v0.x.y" (impl agent reads version from
  CMakeLists.txt or a shared header).

---

## 3. Color palette

Final hex values. The impl agent must use these exact constants and
must rewrite or extend `src/gui/CustomLookAndFeel.h` so the colors below
are the only color literals in the editor — no inline `juce::Colour`
construction in `PluginEditor.cpp`.

| Role                       | Hex        | Used for                                                |
|----------------------------|------------|---------------------------------------------------------|
| `bgDeep`                   | `#161616`  | Window background behind everything                    |
| `bgPanel`                  | `#1e1e1e`  | Body background; recessed strip background             |
| `surfaceLow`               | `#222222`  | Sub-panels (knob row panel, switch cluster panel)      |
| `surfaceHigh`              | `#2a2a2a`  | Footer band; combo / button base                       |
| `faceplateLow`             | `#3a3a3a`  | Brushed-metal header gradient bottom                   |
| `faceplateMid`             | `#5a5a5a`  | Brushed-metal header gradient mid                      |
| `faceplateHigh`            | `#6e6e6e`  | Brushed-metal header gradient top                      |
| `faceplateScratch`         | `#7a7a7a`  | Hairline brush-noise highlights on header              |
| `borderHairline`           | `#323232`  | 1 px panel borders                                     |
| `borderRecessed`           | `#0c0c0c`  | Inner shadow line for recessed strips                  |
| `screwBody`                | `#3c3c3c`  | Fastener body                                          |
| `screwHighlight`           | `#8a8a8a`  | Fastener slot rim highlight                            |
| `screwSlot`                | `#0a0a0a`  | Fastener cross-slot                                    |
| `knobMetalHigh`            | `#6c6c6c`  | Knob metallic gradient top                             |
| `knobMetalLow`             | `#2c2c2c`  | Knob metallic gradient bottom                          |
| `knobRing`                 | `#1a1a1a`  | Outer ring around the knob                             |
| `knobRingHighlight`        | `#5a5a5a`  | Top-edge highlight on the ring                         |
| `knobPointer`              | `#f2f2f2`  | Pointer line                                           |
| `knobArcTrack`             | `#2a2a2a`  | Unfilled portion of the arc track                      |
| `knobArcFill`              | `#c8a45c`  | Filled portion of the arc (warm amber)                 |
| `knobArcFillBypass`        | `#5a5a5a`  | Arc fill while plugin is bypassed                      |
| `textBright`               | `#ececec`  | Primary legend, knob labels, wordmark                  |
| `textMid`                  | `#b8b8b8`  | Value readouts                                         |
| `textDim`                  | `#7a7a7a`  | Secondary legend, units, version line                  |
| `textInk`                  | `#0c0c0c`  | Engraved-look text on bright faceplate                 |
| `ledGreen`                 | `#27d05a`  | "Below threshold", link active, signal-present         |
| `ledGreenDark`             | `#0e3a1a`  | LED green when unlit (rim only, dark recess)           |
| `ledYellow`                | `#ffc830`  | Pre-warning, mid-range meter segments                  |
| `ledYellowDark`            | `#3a300e`  | LED yellow unlit                                       |
| `ledAmber`                 | `#ff8a1c`  | Warning, GR meter segments                             |
| `ledAmberDark`             | `#3a220c`  | LED amber unlit                                        |
| `ledRed`                   | `#ff3826`  | "Above threshold", red zone, bypass active             |
| `ledRedDark`               | `#3a0e0a`  | LED red unlit                                          |
| `meterFaceCream`           | `#f0e8cf`  | Reserved — VU face if a sweep meter is later added     |
| `meterRedZone`             | `#c81818`  | Reserved — VU red zone                                 |
| `accentEngagedGlow`        | `#c8a45c`  | Subtle bloom around active arcs and engaged switches   |

### 3.1 Migration from existing palette

The current `CustomLookAndFeel.h` palette is acceptable as a starting
point but is too monochrome and the accent (`#888888`) reads as "dim
text" rather than as an indicator of activity. The impl agent must:

1. Replace the inline color literals in `CustomLookAndFeel.h` with the
   roles above.
2. Replace the inline color literals in `LedMeter.h` with `ledGreen`,
   `ledYellow`, `ledRed`, `ledAmber` and their `*Dark` counterparts.
3. Replace the inline color literals scattered across `PluginEditor.cpp`
   (e.g. `0xff2c2c2c`, `0xffaaaaaa`) with palette roles.

---

## 4. Typography

### 4.1 Font choices

- **Primary sans:** **Inter** (SIL OFL). Used everywhere except the
  numeric value readouts.
- **Numeric / mono face:** **JetBrains Mono** (SIL OFL). Used only for
  the four knob value readouts so digits do not jitter as values change.

Both are open-licensed and ship as static `.ttf` files. They are
embedded in the binary as JUCE `BinaryData` resources via CMake's
`juce_add_binary_data`. The impl agent stores the font files at:

```
assets/fonts/Inter-Regular.ttf
assets/fonts/Inter-Medium.ttf
assets/fonts/Inter-Bold.ttf
assets/fonts/JetBrainsMono-Regular.ttf
```

and registers them in a static `Fonts` helper namespace inside
`src/gui/Fonts.h` that returns cached `juce::Typeface::Ptr` instances.

If either font cannot be added during impl (e.g. CMake binary data
issue), the fallback is JUCE's default sans for Inter and
`juce::Font::getDefaultMonospacedFontName()` for JetBrains Mono. Do not
fall back to a proprietary system font.

### 4.2 Sizes and weights

| Use                         | Family            | Weight  | Size (pt at 1.0×) | Tracking |
|-----------------------------|-------------------|---------|-------------------|----------|
| Wordmark "mw160"            | Inter             | Bold    | 28                | 0        |
| Faceplate sublabel          | Inter             | Medium  | 10                | +2       |
| Threshold LED labels        | Inter             | Medium  | 10                | +1       |
| Knob top labels             | Inter             | Bold    | 11                | +1       |
| Knob value readouts         | JetBrains Mono    | Regular | 13                | 0        |
| Knob unit captions          | Inter             | Regular | 9                 | 0        |
| Switch labels               | Inter             | Bold    | 11                | +1       |
| Meter strip top labels      | Inter             | Bold    | 9                 | +1       |
| Meter scale ticks           | Inter             | Regular | 8                 | 0        |
| Preset label "PRESET"       | Inter             | Bold    | 9                 | +1       |
| Preset combo entries        | Inter             | Regular | 12                | 0        |
| Footer version text         | Inter             | Regular | 9                 | 0        |
| Tooltips                    | Inter             | Regular | 11                | 0        |

JUCE expresses tracking via `juce::Font::withExtraKerningFactor()`. The
"+1" / "+2" above are *pixel* tracking values; convert to kerning factor
as `extra / fontSize`. Encapsulate this conversion inside the `Fonts`
helper.

### 4.3 Wordmark rendering

The wordmark "mw160" on the header is rendered as Inter Bold 28 in
`textInk` (not `textBright`) on top of the brushed-metal gradient,
giving an engraved appearance. To strengthen the engraved look, draw it
twice:

1. First pass at `(x+0, y+1)` in `#ffffff` at 12% alpha (highlight).
2. Second pass at `(x, y)` in `textInk` (`#0c0c0c`).

This is a 2-line, 5-color trick that the impl agent must implement
inside the editor's `paint()` directly, not in the LookAndFeel.

---

## 5. Knob design

### 5.1 Finish

**Brushed metal** with vertical gradient and a tight inner highlight.
Not glossy; not flat. The intent is "milled aluminum knob with
anodized cap" — period-correct for the class without copying any
specific cap.

### 5.2 Geometry

- **Diameter:** 96 px at default scale (per §2.3).
- **Travel angle:** 270° total. Start angle = 7π/6 (≈ −150°), end angle
  = π/3 + 2π = 7π/3 (≈ +120°), measured clockwise from 12 o'clock.
- **Outer ring:** 1.5 px stroke in `knobRing` with a 0.5 px highlight
  edge in `knobRingHighlight` on the top half (use a path for the top
  arc only).
- **Body fill:** linear gradient `knobMetalHigh` → `knobMetalLow`,
  vertical, with a thin (1 px) horizontal "brushed" highlight band at
  35% from the top in `#7a7a7a` at 18% alpha.
- **Inner cap:** a slightly smaller circle (radius × 0.78) filled with a
  flat `#1a1a1a`, 0.8 px stroke `#3c3c3c`. This contains the pointer.

### 5.3 Pointer

A single straight line (not a notch in the rim, not a skirt indicator).

- Length: 0.62 × radius.
- Width: 3.0 px.
- Color: `knobPointer` (`#f2f2f2`).
- Origin: starts at radius × 0.18 from center (does not start at the
  exact center) and extends outward.
- End cap: rounded.
- Drawn after the inner cap, so it sits visually on top of the cap.

### 5.4 Arc track

Drawn *outside* the knob body (`trackRadius = radius + 6`).

- **Track stroke** in `knobArcTrack`, 3.5 px, full 270° travel arc.
- **Fill stroke** in `knobArcFill` (or `knobArcFillBypass` when the
  plugin is bypassed), 3.5 px, drawn from start angle to current angle.
- For the RATIO knob, the arc fill is drawn from the *zero point*
  (1:1 ratio at travel start) so the arc grows to the right; for the
  THRESHOLD knob, the arc fill is drawn from the rightmost point
  (0 dB threshold position) and grows to the left as the user pulls
  threshold down. For OUTPUT and MIX, the arc grows from the start.
- The fill stroke ends with a small 4 × 4 px filled circle ("LED tip")
  in `accentEngagedGlow` at the current angle's outer position to give
  a soft engaged-glow.

### 5.5 Hover and active

- **Hover** (`isMouseOver()`): the arc fill brightens by +12% lightness;
  the pointer brightens to `#ffffff`.
- **Active** (`isMouseButtonDown()`): a thin 1 px outer ring at radius
  × 1.18 in `accentEngagedGlow` at 40% alpha appears, faintly
  highlighting that the knob is being dragged.
- **Disabled / bypassed:** arc fill switches to `knobArcFillBypass`;
  pointer dims to `textMid`.

### 5.6 drawRotarySlider construction order

The impl agent's `CustomLookAndFeel::drawRotarySlider` must draw in
this exact order so layering is correct:

1. Arc track (`knobArcTrack`).
2. Arc fill (color depends on parameter direction — see §5.4).
3. Arc fill tip dot.
4. Knob outer ring (1.5 px).
5. Knob body fill (gradient).
6. Brushed highlight band (1 px).
7. Inner cap fill + stroke.
8. Pointer.
9. Hover / active overlays if applicable.

---

## 6. Meter design

### 6.1 Decision

**Vertical LED ladder** for IN, OUT, and GR. **No analog VU sweep
needle in the baseline editor.** The cream-face VU face colors in §3
(`meterFaceCream`, `meterRedZone`) are reserved for a future flavor
variant and the impl agent must not draw a VU face in this pass.

Rationale: the editor already has `LedMeter.h` which builds the LED
ladder, the existing 3-meter layout fits the body region cleanly, and a
VU sweep would compete visually with the four knobs in the limited
360 px height. The mode selector in §2.4 still exists so a future
single-meter variant can drop in.

### 6.2 LED ladder spec

For each ladder:

- **Width:** 28 px (per §2.2).
- **Height:** 176 px (per §2.2).
- **Inset (recessed look):** 2 px inner shadow border in
  `borderRecessed`.
- **Segment count:** 28 segments per IN/OUT ladder; 20 segments per
  GR ladder. (Current code uses 20 / 16 — increase per this spec.)
- **Segment pitch (dB per segment):**
  - IN/OUT ladder: range −60 dBFS → +6 dBFS = 66 dB over 28 segments
    ≈ **2.36 dB/segment**. Round display thresholds to integer dB.
  - GR ladder: range 0 dB → −20 dB over 20 segments = **1 dB/segment**.
    Below −20 dB GR, the ladder is fully lit and the bottom segment
    flashes red as an "out of range" indicator.
- **Segment shape:** rounded-rect, 1 px corner radius, full ladder
  width minus 2 px on each side.
- **Segment gap:** 2.0 px vertical between segments (current code uses
  1.5 — bump to 2.0 for clearer per-segment readability).
- **Lit colors (IN/OUT, bottom-up):**
  - segments below −12 dBFS → `ledGreen`
  - segments −12 to −3 dBFS → `ledYellow`
  - segments above −3 dBFS → `ledRed`
- **Lit colors (GR, top-down):**
  - segments above −6 dB GR → `ledAmber`
  - segments below −6 dB GR → `ledRed` (deeper GR = more aggressive)
- **Unlit segments:** the corresponding `*Dark` color from §3 at 100%
  opacity, plus a 0.5 px stroke in `borderHairline` to read as a real
  recessed segment rather than a transparent ghost.
- **Per-lit-segment glow:** a soft 1 px outer rectangular halo at 30%
  alpha of the lit color, drawn before the segment fill. Without this,
  the meters look painted-on rather than emissive.

### 6.3 Ballistics (visual, GUI thread)

- **Update rate:** 30 Hz (matches existing `startTimerHz(30)`).
- **Attack:** instantaneous on rise (snap to peak).
- **Decay:** 0.85 hold-per-frame for IN/OUT (matches existing
  `kDecay` constant). For GR, decay 0.80 hold-per-frame toward 0.
- **No interpolation between segments** — segments quantize to the dB
  pitch above. Smooth interpolation defeats the LED ladder identity.

### 6.4 Tick labels

Drawn to the *side* of each ladder, not above or below, so the user can
read levels without breaking eye line on the meters.

- IN and OUT ladders share a single tick column drawn between them at
  `x ≈ 56` and `x ≈ 96`. The shared column shows: `+6`, `0`, `-6`,
  `-12`, `-24`, `-48`. Color: `textDim`. Font 8 pt.
- GR ladder has its own tick column on its right side at `x = 144`,
  showing: `0`, `-3`, `-6`, `-12`, `-20`. Color: `textDim`.

### 6.5 What the meters display

- **IN ladder:** input level in dBFS, post-input-gain, pre-detector.
- **OUT ladder:** output level in dBFS, post-makeup-gain.
- **GR ladder:** current gain reduction in dB (0 = no reduction,
  negative downward).

The processor already exposes these via `getMeterInputLevel()`,
`getMeterOutputLevel()`, `getMeterGainReduction()` (see existing
`PluginEditor::timerCallback()`); the impl agent does not need to add
new processor APIs.

### 6.6 Meter mode selector behavior

The 3-state segmented selector in §2.4 only adjusts the *brightness* of
the corresponding ladder's top label and tick column (the selected one
is `textBright`, the others are `textDim`). It does not hide ladders.
This preserves the "one mode at a time" identity from the hardware
without losing the value of seeing all three at once.

---

## 7. Toggle switches

### 7.1 Visual language

**Pill-shaped 2-state push switches with embedded LED.** Not bat-handle
toggles, not flat checkboxes. Each switch is a horizontal rounded-rect
"pill" with the label inside and a small LED at the left edge.

### 7.2 Geometry

- **Switch bounds:** 140 × 36 px (per §2.4).
- **Corner radius:** 6 px.
- **LED:** 8 px diameter circle, centered vertically, at `x = pillX + 12`.
- **Label:** `Inter Bold 11 pt +1`, vertically centered, left-aligned at
  `x = pillX + 28`, color depends on state.

### 7.3 States

| State                | Pill fill   | Pill stroke   | LED color        | Label color   |
|----------------------|-------------|---------------|------------------|---------------|
| OFF                  | `#1a1a1a`   | `#323232` 1 px| corresponding `*Dark` | `textDim` |
| ON                   | `#262626`   | `#3c3c3c` 1 px| `ledGreen` (default) | `textBright` |
| ON (BYPASS only)     | `#2a1a1a`   | `#3c2222` 1 px| `ledRed`         | `textBright`  |
| HOVER (any)          | +6% lighter | unchanged     | unchanged        | unchanged     |

### 7.4 Per-switch labels and LED color

| Switch            | OFF label | ON label | ON LED color  |
|-------------------|-----------|----------|---------------|
| KNEE              | `HARD`    | `SOFT`   | `ledGreen`    |
| STEREO LINK       | `OFF`     | `ON`     | `ledGreen`    |
| BYPASS            | `OFF`     | `ON`     | `ledRed`      |

The KNEE switch is a 2-state toggle whose label *changes* between HARD
and SOFT rather than reading "KNEE: ON". This is more readable and
matches the hardware idiom of a labeled selector.

### 7.5 Meter mode segmented selector

The 3-state IN | OUT | GR selector (§2.4) is a separate visual idiom:

- Three equal segments inside a single pill, dividers in `borderHairline`.
- Selected segment fill: `surfaceHigh`, label `textBright`.
- Unselected segment fill: `bgPanel`, label `textDim`.
- Hover on a non-selected segment: fill `#262626`.

---

## 8. Preset browser

### 8.1 Idiom

**Inline horizontal browser** in the footer, not a slide-out panel,
not a modal dialog. The user sees: label, dropdown, save, delete in a
single row.

### 8.2 Layout

See §2.5 for exact bounds. The combo box is the largest element so
preset names are not truncated.

### 8.3 Visual

- **Combo box:** rounded 4 px, fill `surfaceHigh`, border
  `borderHairline`, text `textBright`, arrow glyph in `accentEngagedGlow`
  drawn as a small downward chevron (8 × 5 px) at the right edge.
- **Save / Del buttons:** rounded 4 px, fill `surfaceHigh`, border
  `borderHairline`, label `textBright`. Hover: fill `#303030`. Active:
  fill `accentEngagedGlow` at 25% alpha, label `textBright`.

### 8.4 Save / delete behavior

The current behavior using `juce::AlertWindow` for save/delete prompts
is acceptable visually but the alert windows must be themed via the
LookAndFeel — set the alert window background to `bgPanel` and text to
`textBright` so the prompt does not flash a default-themed dialog. The
impl agent must override
`LookAndFeel_V4::createAlertWindow` or set
`AlertWindow::backgroundColourId` and `AlertWindow::textColourId` on
the LookAndFeel.

### 8.5 Categories / separators

The existing `refreshPresetList()` adds a separator between factory and
user presets. Keep this. The popup menu should render the separator as
a 1 px line in `borderHairline` with 4 px vertical padding.

---

## 9. Fasteners / screws / panel details

### 9.1 Decision

**Yes — draw four corner fasteners on the header faceplate.** They are
a defining feature of the 1U rack visual language (REFERENCE.md §3.1).

### 9.2 Geometry

Four screws total, two on the left header sub-region and two on the
right. Per §2.1:

- Left top:    center `(16, 16)`, radius 5.
- Left bottom: center `(16, 36)`, radius 5.
- Right top:   center `(884, 16)`, radius 5.
- Right bottom:center `(884, 36)`, radius 5.

### 9.3 Rendering

Each screw is drawn procedurally as five paint operations:

1. Filled circle in `screwBody` (`#3c3c3c`).
2. 1 px outer ring in `borderHairline`.
3. Inner radial gradient `#1a1a1a` → `#4a4a4a`, top-to-bottom.
4. Cross-slot: two perpendicular rectangles 6 × 1.2 px in `screwSlot`,
   rotated **17°** (intentionally off-axis so screws look "tightened"
   rather than placed). Use a deterministic per-screw rotation derived
   from screw index so the four screws are not all the same angle —
   suggest 17°, −22°, 8°, −13°.
5. Highlight: a 0.8 px arc on the top-left of the slot in
   `screwHighlight` at 60% alpha, suggesting an overhead light source.

The screws are drawn in `paint()`, not as `Component`s.

### 9.4 No other panel hardware

- No serial-number badge.
- No UL plate.
- No "Made in" engraving.
- No model-number plate.
- No grille texture, no rack ear cutouts beyond the four screws.

---

## 10. DPI awareness / scaling

### 10.1 Approach

The editor uses **JUCE's built-in scale factor** plus **vector-only
graphics** (per §12). All sizes in this document are at scale 1.0.
JUCE's `Component::setSize` drives the host scale; the impl agent does
not need to add manual `@2x` asset paths because there are no raster
assets.

### 10.2 Font scaling

Inter and JetBrains Mono are vector fonts and scale cleanly. The impl
agent must use `juce::Font(typefacePtr, sizeInPoints, styleFlags)` so
that font sizes are in points, not pixels — JUCE then handles host
scaling.

### 10.3 Stroke widths

All stroke widths in this document are at scale 1.0. To prevent
sub-pixel blurriness at non-integer scales, snap stroke widths to
device pixels by passing the result of
`juce::roundToInt(strokeWidth * scale) / scale` for any 1 px hairline.
Wrap this in a `Geometry::snap1px()` helper inside `src/gui/Geometry.h`.

### 10.4 No raster assets

The editor ships **zero PNG / JPG / BMP files**. There are no `@2x`
variants needed because there are no raster files in the first place.
Fonts are the only binary assets.

---

## 11. Accessibility

### 11.1 Touch / click target sizes

- Knobs: 96 × 96 px hit target (full body). Above WCAG 2.1 minimum
  44 × 44 px.
- Toggle switches: 140 × 36 px hit target. The 36 px height is the
  minimum dimension, above the 44 × 44 minimum on the long axis but
  below it on the short axis. The impl agent must add 4 px of vertical
  hit padding via `juce::Component::setExplicitFocusOrder` and
  `addChildComponent` of an invisible padding component, so the
  effective hit zone is 140 × 44 px.
- Preset buttons (Save / Del): 64 × 24 px visual, 64 × 36 px hit target
  via the same vertical-padding trick.

### 11.2 Contrast ratios

The palette in §3 was chosen so that primary text combinations meet
WCAG 2.1 AA contrast (4.5:1 for normal text, 3:1 for large text):

- `textBright` (`#ececec`) on `bgPanel` (`#1e1e1e`): 14.6:1 — AAA.
- `textBright` (`#ececec`) on `surfaceLow` (`#222222`): 13.6:1 — AAA.
- `textMid` (`#b8b8b8`) on `surfaceLow` (`#222222`): 8.7:1 — AAA.
- `textDim` (`#7a7a7a`) on `bgPanel` (`#1e1e1e`): 3.8:1 — AA for large
  text, below AA for small body text. `textDim` is **only allowed for
  text 11 pt and larger** in this spec; the impl agent must use
  `textMid` for any 9 pt body text on dark backgrounds.
- `textInk` (`#0c0c0c`) on `faceplateMid` (`#5a5a5a`): 5.6:1 — AA.

### 11.3 Color-blind friendliness

The IN/OUT ladders use green/yellow/red, which fails for protanopia and
deuteranopia. To compensate, the position of the lit segments (height)
is the primary information channel, not the color. The user can read
"how high the column goes" without distinguishing the colors. The GR
ladder uses amber/red, which is also distinguishable by saturation.
This is acceptable for a metering UI but the impl agent should leave a
backlog entry to consider a high-contrast monochrome theme variant.

### 11.4 Keyboard / accessibility hooks

- Each knob must have a JUCE `setTitle()` call with the parameter name
  for screen readers.
- The `setHelpText()` field on each knob carries the parameter range
  and units.
- Tab focus order: presets → THRESHOLD → RATIO → OUTPUT → MIX → KNEE →
  STEREO LINK → BYPASS → meter mode.

---

## 12. Rendering approach

### 12.1 Decision

**Procedural JUCE `Graphics` calls.** No external SVG asset files. No
embedded SVG path strings. The entire editor is drawn from primitives
(`fillRect`, `drawEllipse`, `Path`, `ColourGradient`) inside `paint()`
and inside the LookAndFeel.

### 12.2 Rationale

- The editor is small enough (a few panels, four knobs, three meters,
  three switches) that procedural drawing is faster to write than
  authoring SVGs.
- Procedural code can react to state (hover, bypass, knob value)
  without per-state asset variants.
- No asset loader code needed; no `BinaryData` for graphics.
- The only `BinaryData` resources are the four font files in §4.1.

### 12.3 Where the drawing lives

| Component                          | Owns drawing                         |
|------------------------------------|--------------------------------------|
| Header faceplate, fasteners, LEDs  | `MW160Editor::paint()`               |
| Body panel backgrounds             | `MW160Editor::paint()`               |
| Footer band                        | `MW160Editor::paint()`               |
| Knobs                              | `CustomLookAndFeel::drawRotarySlider`|
| Toggle pills                       | `CustomLookAndFeel::drawToggleButton`|
| Meter mode segmented selector      | New `MeterModeButton` component      |
| LED ladders                        | `LedMeter::paint()` (existing)       |
| Combo box, buttons                 | `CustomLookAndFeel` defaults + recolor |

### 12.4 Asset directory

`assets/` does not yet exist; the impl agent creates it with this
structure:

```
assets/
  fonts/
    Inter-Regular.ttf
    Inter-Medium.ttf
    Inter-Bold.ttf
    JetBrainsMono-Regular.ttf
```

These are wired into the binary via `juce_add_binary_data` in
`CMakeLists.txt`.

### 12.5 No assets to NOT include

- No `.svg` files.
- No `.png` files.
- No `.jpg`, `.bmp`, `.tif` files.
- No "panel.png" texture.
- No font fallback using a system-installed proprietary font.

---

## 13. Non-goals (binding do-not-do list)

The impl agent MUST NOT include any of the following in the editor:

- Any manufacturer name, product name, or model number, in any
  language, in any visual element, source comment, or string literal.
- Any trademarked feature name. The soft knee control is `KNEE` with
  states `HARD` and `SOFT`.
- Any "feature pill" or callout that quotes a hardware manual.
- Any serial-number plate, UL plate, FCC ID, "Made in" engraving, or
  similar regulatory mimicry.
- Any photographic or photo-traced texture. All visuals are procedural
  vector graphics or vector fonts.
- Any cream-paper VU face in the *baseline* editor. (Reserved for a
  future flavor variant; the palette includes the colors but the
  drawing must not happen in this pass.)
- Any rack-ear cutouts, ventilation grilles, or audio-jack glyphs on
  the faceplate.
- Any retro-CRT scanlines, vinyl crackle, or "vintage" filter overlay.
- Any reference to specific historical units in source comments — the
  inline doxygen comments in `PluginEditor.h` and `LedMeter.h` that
  mention specific hardware must be rewritten.
- Any preset name that references a specific hardware unit by name.
  Preset names should describe sonic intent, e.g. "Drum Bus Glue",
  "Vocal Tighten", "Bass DI Punch".
- Any system-installed proprietary font as a runtime fallback.

---

## 14. Implementation handoff

### 14.1 Files the impl agent will touch

- `src/PluginEditor.h` — strip legacy comments and brand-adjacent strings;
  rename toggle button to `kneeButton`; add `bypassButton`,
  `meterModeButton`; add `Fonts` include.
- `src/PluginEditor.cpp` — full rewrite of `paint()` and `resized()`
  per this spec; remove inline color literals.
- `src/gui/CustomLookAndFeel.h` — rewrite the palette section; rewrite
  `drawRotarySlider` per §5; rewrite `drawToggleButton` per §7; add
  combo box + alert window theming per §8.
- `src/gui/LedMeter.h` — bump segment counts per §6.2; replace inline
  colors with palette roles; add per-segment glow halo.
- `src/gui/Fonts.h` — **new**. Cached typeface accessors and the
  tracking-to-kerning helper.
- `src/gui/Geometry.h` — **new**. The `snap1px(scale)` helper.
- `src/gui/MeterModeButton.h` — **new**. The 3-state segmented control.
- `src/gui/Screws.h` — **new**. Procedural screw drawing helper.
- `assets/fonts/*.ttf` — **new**. Font files.
- `CMakeLists.txt` — register `assets/fonts/*.ttf` as binary data.

### 14.2 Things the impl agent does NOT touch

- `src/PluginProcessor.h` / `src/PluginProcessor.cpp` — no DSP changes.
- `src/dsp/*` — no DSP changes.
- `src/PresetManager.*` — no preset format changes.
- The APVTS parameter IDs — `threshold`, `ratio`, `outputGain`, `mix`,
  `softKnee`, `stereoLink`, `bypass`. The parameter ID uses neutral
  vocabulary in the APVTS; the *UI label* is `KNEE / HARD / SOFT`. The
  impl agent must add a source comment at the APVTS parameter declaration
  explaining why the ID and the label diverge.

### 14.3 Verification checklist for the impl agent

Before completing the impl pass, verify:

- [ ] Editor opens at exactly 900 × 360.
- [ ] Editor resizes between 720 × 288 and 1440 × 576 with aspect lock.
- [ ] All four knobs are 96 px diameter at default size.
- [ ] All three meters are visible with the segment counts in §6.2.
- [ ] All three switches render in their OFF state by default; BYPASS
      shows red LED when toggled.
- [ ] No string literal anywhere in the editor source contains a
      manufacturer or model name.
- [ ] No `0xff…` color literal exists in `PluginEditor.cpp` (all colors
      come from `CustomLookAndFeel` palette roles).
- [ ] Inter and JetBrains Mono fonts load from BinaryData.
- [ ] The four corner screws are visible at default size.
- [ ] Tab focus order matches §11.4.
- [ ] Plugin builds VST3, AU, and Standalone targets without warnings.

---

## Changelog

| Version | Date       | Author                | Notes                                                                                    |
|---------|------------|-----------------------|------------------------------------------------------------------------------------------|
| 1.0     | 2026-04-11 | design-research-agent | Initial spec. 900×360 default, 2.5:1 lock, 4-knob row, 3 LED ladders, procedural draw.   |
