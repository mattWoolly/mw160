# mw160 Reference Document

**Scope.** This document describes, in neutral and generic terms, the expected
behavior and visual language of a classic feedback-topology VCA dynamic range
compressor of the kind commonly built as a 1U rack processor from the late
1970s through the early 1980s. It intentionally names no manufacturer, model,
product, or trademarked feature. References are to open-source DSP literature
only. The project working name throughout is **mw160**.

All descriptions below characterize a *class* of hardware (VCA feedback
compressors of that era), not any specific instrument.

---

## 1. DSP Behavior

### 1.1 Topology: feedback (return-path) detection

The target behavior is a **feedback** compressor, not feedforward. In a
feedback topology, the level detector reads the compressor's own *output*
(post-VCA) and steers the gain-control element from there. The practical
consequences are:

- The gain reduction curve is inherently soft even without an explicit
  soft-knee function, because the feedback loop settles to an equilibrium
  point that depends on the instantaneous loop gain.
- Program-dependent behavior emerges naturally: the loop response time
  depends on how far the detector is from equilibrium.
- At very high ratios the loop gain is high, so the detector must be well
  conditioned (RMS-ish, not raw peak) to avoid oscillation or pumping.
- Static gain-reduction curves measured on such a unit always look slightly
  gentler than the nominal ratio near threshold, because the loop has not
  yet converged when a tone crosses threshold.

See Zölzer (2011), *DAFX: Digital Audio Effects*, ch. 4 "Dynamic Range
Control," and Reiss & McPherson (2014), *Audio Effects: Theory,
Implementation and Application*, ch. 6, for textbook treatments of
feedforward vs. feedback topology and their distinct side-effects.

### 1.2 Level detector

Expected behavior: a **true-RMS** (power-averaging) detector with a time
constant on the order of ~20 ms. Key properties:

- Squared-signal exponential moving average, `rms² = α · x² + (1−α) · rms²`,
  with `α = 1 − exp(−1 / (τ · fs))` and `τ ≈ 20 ms`.
- The detector output in linear domain is `sqrt(rms²)`; convert to dB for
  the gain computer with a small floor (`1e-30f`) to avoid `log(0)`.
- At a sustained sinusoid at 0 dBFS, the detector should converge to
  `−3.01 dB` (the 1/√2 RMS of a unit-amplitude sine).
- At a DC input of 0.5, it should converge to exactly 0.5.
- Settling time from silence to a sustained tone should be ~3τ (≈ 60 ms)
  to reach 95%.
- Detector **state must persist across processBlock boundaries**. Any
  detector whose state is implicitly tied to block layout (e.g., reset or
  smoothed per-block instead of per-sample) will produce block-size
  dependent output and is incorrect.

True RMS differs from peak or quasi-peak detection in that it tracks
signal *energy* rather than excursion, so two waveforms with the same RMS
but different crest factors produce the same gain reduction. This is the
correct target for this class of unit.

Reference: Giannoulis, Massberg & Reiss (2012), "Digital Dynamic Range
Compressor Design — A Tutorial and Analysis," *JAES* 60(6), § 2.2 on
detector topologies and their error vs. true-RMS.

### 1.3 Program-dependent ballistics

The defining sonic character of this class of unit is that **attack and
release times are not fixed** — they depend on the magnitude and shape of
the input program. In a feedback-topology analog implementation this falls
out of the circuit naturally (RC-like detector plus a log-domain VCA in a
servo loop); in a digital implementation it must be modeled explicitly.

Expected behavior:

- **Attack time scales inversely with transient magnitude.** Large, fast
  transients (e.g., a snare hit 30 dB above threshold) clamp in
  single-digit milliseconds. Smaller excursions (10 dB above threshold)
  take more like 10–20 ms. A reasonable, literature-consistent mapping:
  - 10 dB step  → ≈ 15 ms to 90% of target
  - 20 dB step  → ≈  5 ms
  - 30 dB step  → ≈  3 ms
  - Clamped to the range [1 ms, 50 ms] to prevent runaway.
- **Release is approximately a constant rate in dB/sec**, not a constant
  time constant. A target of ~125 dB/sec is consistent with period
  literature for this class of unit. Practically, a 10 dB release
  completes in ~80 ms, and a 50 dB release in ~400 ms. The rate should
  stay constant to within ~10–15% across the 5–50 dB GR range.
- **Release is linear in the dB domain**, not exponential in linear
  amplitude. This is the single most important detail for reproducing the
  characteristic "punch" of the class.
- Ballistics are **fixed / program-dependent, not user-adjustable**, in
  hardware of this lineage. Exposing attack and release as user knobs is
  a stylistic departure, not a faithful reproduction.

Giannoulis et al. (2012), § 2.3–2.4, gives closed-form expressions for
attack and release filters and discusses program-dependent variants.

### 1.4 Soft-knee detector behavior

The knee region — the transition from 1:1 through full ratio near
threshold — must be **smooth and continuous** in both the static curve
and its first derivative. A kinked static curve is a symptom of a
switched detector path or a discontinuous gain computer and will be
audible as a click when a signal grazes threshold.

A quadratic interpolation across a knee width `W` (typically 6–12 dB) is
a standard construction:

- For input level `x` below `T − W/2`: gain reduction = 0 dB
- For input level `x` above `T + W/2`: gain reduction = full ratio
- In the knee region: `GR = (1/R − 1) · (x − T + W/2)² / (2W)`

The soft-knee mode is often wired to a front-panel switch in hardware
of this class, selectable against a harder curve. It is a *gain-computer*
feature, not a *detector* feature. The detector topology, time constant,
and feedback path are unchanged.

Reference: Giannoulis et al. (2012), § 3 on knee shapes, incl. the
quadratic form above.

### 1.5 THD under gain reduction

A faithful emulation of this class of unit should exhibit **small but
nonzero program-dependent harmonic distortion that grows with gain
reduction**. Key points:

- At low GR (≤ 3 dB), THD should be under ~0.02% — inaudible on program
  material.
- At moderate GR (10–20 dB), THD typically climbs to ~0.1–0.2%.
- The spectrum should be **even-order dominant** (2nd harmonic stronger
  than 3rd), reflecting the asymmetric transfer of a typical analog
  gain-control element of this lineage.
- THD should not change the DC level (no net asymmetric offset) and must
  not introduce denormals, NaN, or Inf anywhere in the signal path.

This coloration is a property of the VCA stage in the signal path, not
the detector. In code it is typically modeled with a small asymmetric
waveshaper whose drive scales with current gain reduction.

### 1.6 Sidechain HPF behavior (optional)

Some hardware of this class wires a fixed or switchable high-pass filter
into the detector path to prevent sub-bass energy from modulating the
compressor. If a sidechain HPF is implemented, typical cutoffs are
60–120 Hz, 2nd order. The HPF affects only the detector side — the audio
path is unfiltered.

### 1.7 Makeup gain staging

Output gain (makeup gain) is applied **post-VCA, post-saturation**, as a
pure linear scale. Consequences:

- Output gain should be additive (in dB) relative to threshold and ratio
  — adjusting output gain must not change the static compression curve.
- Output gain should be **smoothed per-sample** (20 ms linear ramp) to
  avoid zipper noise during automation.
- In a parallel/mix configuration, makeup gain should apply to the
  *blended* signal, not to the wet path alone, so that dry and wet
  contributions are affected symmetrically.

### 1.8 Stereo linking

Expected behavior when linked: the detector sums squared L and R
(`linked² = (L² + R²) / 2` or `L² + R²`, either is defensible) and
feeds a single shared detector. Both channels then receive **identical**
gain reduction. Consequences:

- A signal present only on L still produces GR on R, so the stereo image
  does not collapse under asymmetric content.
- Toggling stereo link during playback must not produce a click. Either
  smooth between linked and unlinked states over ~20 ms, or blend the GR
  values rather than switching detectors.
- Mono input ignores the stereo-link control.

---

## 2. Control Set

The canonical control set for a unit of this class is **minimal** — this
is part of its visual and operational identity.

### 2.1 Rotary controls

| Control      | Range (typical)              | Unit | Notes |
|--------------|------------------------------|------|-------|
| Threshold    | −40 to +20                   | dBu  | Reference level conventions vary; dBu is historically accurate for balanced line gear, dBFS is the sensible digital equivalent. The chosen convention must be **documented in the UI and the manual** — ambiguity here is a UX bug. |
| Ratio        | 1:1 → ∞:1 (continuous)       | :1   | Must reach **infinity** (limiter mode) at the top of the travel. Some hardware of this class extended *past* infinity into negative ratios (dynamics inversion); that is a signature capability of the class and should be considered optional but within scope. |
| Output gain  | −20 to +20                   | dB   | Post-everything linear scale; see §1.7. |

### 2.2 Switches and buttons

- **Knee mode** (hard / soft) — selects between the two gain-computer
  curves described in §1.4. Toggling during playback must be glitch-free.
- **Stereo link** (on / off) — §1.8.
- **Bypass** — sample-accurate, with no latency discrepancy between
  bypassed and engaged states. Bypass must not produce a click.
- **Meter mode** (input / output / gain reduction) — selects what the
  front-panel meter displays. See §2.3.

### 2.3 Metering

Hardware of this class uses a **single front-panel meter** with a mode
switch selecting input, output, or gain reduction. A reasonable digital
equivalent is three independent meters (so the user can see all three at
once) — or a single meter plus a mode switch, for visual fidelity.

- GR meter direction: deeper GR displays as a longer bar or deeper
  needle deflection (inverted vs. input/output).
- Ballistics on the meter itself: fast attack (instant rise) with a
  slower decay (typ. ~85% hold-per-frame at 30 Hz), approximating a VU
  ballistic for the input/output meters.
- Meter state updates must be written from the audio thread via
  `std::atomic<float>` and read from a GUI timer — no locks, no
  allocations, no logging on the audio thread.

### 2.4 Fixed parameters (typically not user-facing on hardware of this class)

- **Attack time** — program-dependent, see §1.3. Exposing it as a knob
  is a stylistic departure and changes the identity of the unit.
- **Release time** — same.
- **Detector RMS window** — fixed at ~20 ms in hardware; exposing it
  would let the user make the unit behave as a peak limiter or a slow
  leveler, both of which are outside the target class.
- **Knee width** — typically fixed internally (6–12 dB) in hardware, not
  exposed.

A faithful implementation should **not** expose these. A "flavor" mode
intended as a departure from the baseline may expose them, but the
baseline preset for the unit should be a button-press away.

### 2.5 Dry/wet mix (modern addition)

Classic hardware of this lineage did not ship with a dry/wet mix knob —
parallel compression was done at the console. A digital implementation
may include a mix control as a quality-of-life addition, but it should
be clearly labeled as such, and the default should be 100% wet so that
the out-of-the-box behavior matches what a user familiar with the
hardware would expect.

---

## 3. Visual Language

This section describes the industrial design language of 1U rack audio
processors of the late-1970s / early-1980s era in generic terms.

### 3.1 Faceplate

- **Form factor:** 1U, 19" rack. Aspect ratio of the faceplate is
  approximately 8.75 : 1 (484 mm × 44 mm). A plugin UI that preserves
  this aspect at a usable size typically sits around 800–900 px wide by
  roughly 1/8 that tall, plus additional rows of controls arranged
  below if the front panel gets too crowded.
- **Material:** brushed aluminum or matte-painted steel. Common color
  treatments: natural brushed silver, matte black, dark gray, occasionally
  a saturated accent color (blue, burgundy, tan) for the main section.
  Screen-printed legend in contrasting ink (white on black, black on
  silver).
- **Fasteners:** four Phillips-head machine screws at the corners of the
  rack ears are standard. They are visual, not purely functional — their
  presence contributes to the recognizable industrial feel.

### 3.2 Controls

- **Rotary pots:** large (≈ 25–35 mm) round knobs with a **single
  pointer line** or skirt indicator. Metal or metal-look material
  finish. Knob color often contrasts with the panel (black knob on
  silver panel or vice versa). Travel is typically ~270° (not a full
  rotation).
- **Skirts:** some knobs have a skirt with ticks every few dB; the
  legend is screen-printed on the panel around the skirt.
- **Switches:** toggle switches (flat-lever or bat-handle) for binary
  choices like stereo link and bypass. Sometimes illuminated
  push-buttons for mode selection.
- **LED indicators:** small round LEDs (red, green, amber) for
  threshold indication, bypass state, limit-active state, etc. Warm
  glow with subtle bloom when lit, dark-with-rim-only when unlit.

### 3.3 Meter

- **Large central VU or LED meter.** The two idioms coexist in this
  era: some units use a mechanical moving-coil VU meter with cream or
  black dial face, others use a vertical LED ladder with stepped
  segments. A plugin rendering may use either — or both side by side,
  one for VU-style ballistics and one for peak/GR.
- VU face: off-white cream (`#f5efd9` or similar), black tick marks,
  red zone above 0 VU, black needle with a counterweight.
- LED ladder face: individual horizontal or vertical segments in green /
  amber / red depending on level. Segment pitch ≈ 2 dB.

### 3.4 Typography

Use **free / libre** typefaces only. Acceptable open-licensed options
that evoke period hardware legend:

- **Inter** (SIL OFL) — clean modern sans, works well for small legend.
- **Roboto Condensed** (Apache 2.0) — narrower caps for dense legend.
- **IBM Plex Sans** (SIL OFL) — period-adjacent industrial feel.
- **Space Mono** (SIL OFL) — for numeric readouts where a monospace
  font suits.

Do **not** use proprietary foundry fonts, and do not trace or
approximate any trademarked type from specific period hardware.

### 3.5 Color palette (generic, starting point)

| Role              | Hex       | Notes |
|-------------------|-----------|-------|
| Background panel  | `#1e1e1e` | Dark baseline so a "painted-steel" faceplate sits over it. |
| Panel surface     | `#282828` | Slightly lifted from background. |
| Brushed-metal mid | `#5a5a5a` | Main knob material. |
| Brushed-metal low | `#383838` | Gradient low end for knob shading. |
| Panel border      | `#3a3a3a` | 1 px hairline separator. |
| Text bright       | `#e0e0e0` | Primary legend. |
| Text dim          | `#888888` | Secondary legend / unlit LED labels. |
| Accent            | `#888888` | Knob arc fill, default. Can be replaced with a saturated accent for a specific variant. |
| LED green         | `#00cc44` | "Below threshold" / "engaged" / "link active". |
| LED amber         | `#ff8800` | Warning / approaching limit. |
| LED yellow        | `#ffcc00` | Info / pre-warning. |
| LED red           | `#ee2200` | "Above threshold" / "limiting hard". |

These are a starting point only and should be refined in a
`VISUAL_SPEC.md` with specific decisions about knob finish (matte vs.
gloss), meter style (VU vs. LED ladder vs. both), and whether the
faceplate uses a saturated accent color.

### 3.6 Non-goals (anti-requirements)

- **No manufacturer logo, product name, model number, or trademarked
  "feature name"** anywhere on the faceplate, in the code, in the
  assets, in the preset names, or in the text of this document. The
  only visible product name is the working name `mw160`.
- **No serial-number badge, UL plate, or any screen-printed detail that
  reproduces a specific manufacturer's visual mark.**
- **No "hardware photograph" textures** that could be traced back to a
  specific unit. SVG / procedural graphics only.

---

## 4. References (open-source DSP literature)

1. Zölzer, U. (ed.) (2011). *DAFX: Digital Audio Effects* (2nd ed.).
   Wiley. Ch. 4, "Dynamic Range Control" — foundational treatment of
   compressor topologies, detectors, and static curves.

2. Reiss, J. D. & McPherson, A. (2014). *Audio Effects: Theory,
   Implementation and Application*. CRC Press. Ch. 6, "Dynamics
   Processing" — textbook treatment of feedforward vs. feedback
   compressors, VCA modeling, and ballistics.

3. Giannoulis, D., Massberg, M. & Reiss, J. D. (2012). "Digital Dynamic
   Range Compressor Design — A Tutorial and Analysis." *Journal of the
   Audio Engineering Society* 60(6), 399–408. The canonical reference
   for digital compressor static curves (including the quadratic
   soft-knee form used here) and the standard smoothed-peak /
   smoothed-RMS detector architectures.

4. Smith, J. O. (2011). *Physical Audio Signal Processing*. W3K
   Publishing / online at ccrma.stanford.edu. Relevant sections on
   one-pole smoothing filters, log-domain ballistics, and waveshaper
   harmonic analysis.

5. Välimäki, V. & Reiss, J. D. (2016). "All About Audio Equalization:
   Solutions and Frontiers." *Applied Sciences* 6(5), 129. Background
   on the one-pole / log-domain filters that underpin §1.2 and §1.3
   implementations.

No commercial product manuals, service documentation, or
manufacturer-authored white papers are cited. Any future additions to
this list must likewise be from open literature.

---

## 5. Traceability to the project

This REFERENCE.md is the authoritative description of **what the mw160
project is trying to be**, in neutral terms. QA agents in the validation
passes compare the shipping plugin's behavior and visual language back
to this document, and any gap is a backlog entry.

If any decision in the plugin diverges from this document (e.g., a
different knee width, a different release rate), the divergence belongs
here as a documented exception with justification, not silently in the
code.
