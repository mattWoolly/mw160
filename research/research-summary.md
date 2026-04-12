# MW160 -- Research Summary

**Date:** 2026-04-05
**Status:** Research complete. Ready for planning.

---

## Product Goals

MW160 is an audio plugin (VST3/AU/AAX via JUCE 7) emulating the **dbx 160 VCA compressor**. The baseline target is a faithful hardware emulation of the original 160 VU's signal behavior with the 160A's expanded control set. Where appropriate and explicitly in scope, the elements that create the signature "punch" should be identified and tastefully exaggerated for a hip-hop-oriented sound.

### Emulation scope
- **Signal path**: Input stage -> Blackmer VCA (gain + subtle coloration) -> Output stage
- **Topology**: Feedforward compressor with true RMS detection
- **Timing**: Program-dependent attack/release with no user A/R controls (matching the 160/160A)
- **Controls**: Threshold (-40 to +20 dBu), Ratio (1:1 to Infinity+ negative ratios), Output Gain (-20 to +20 dB), OverEasy toggle, Stereo Link, Mix
- **Metering**: Input level, output level, gain reduction (switchable or simultaneous)
- **Knee**: Hard knee (default) + OverEasy soft knee (switchable)

---

## Core Behaviors to Reproduce

These are ranked by impact on the perceived character of the compressor:

### 1. Program-dependent attack/release (CRITICAL)
The single most important behavior. Attack gets faster for harder transients (15ms/10dB, 5ms/20dB, 3ms/30dB). Release operates at a roughly constant rate of ~125 dB/sec. This is what makes the 160 "punch" -- the initial transient passes through before the compressor clamps, and the release breathes naturally.

### 2. Hard knee compression (CRITICAL)
The abrupt onset of compression at the threshold is central to the aggressive, snappy character. OverEasy (soft knee) should be a switchable alternative.

### 3. Feedforward topology with RMS detection (CRITICAL)
The detector sees the uncompressed input signal (feedforward). RMS detection responds to signal energy, not peaks. This combination produces compression that tracks perceived loudness and handles extreme ratios without instability.

### 4. VCA coloration (IMPORTANT)
The discrete Blackmer 200 VCA adds subtle, predominantly even-order harmonic distortion. This is the "warmth" and "graininess." Total THD < 0.2% -- subtle but audible on percussive material. Gain-dependent: more coloration at deeper gain reduction.

### 5. Ratio continuity through infinity to negative (IMPORTANT)
The 160A's ratio control sweeps smoothly from 1:1 through infinity:1 into negative ratios. This is a distinctive feature and should be preserved.

### 6. Independent threshold / output gain (IMPORTANT)
These controls do not interact. Output gain is pure post-compression makeup. This simplicity is part of the UX.

---

## Must-Have Technical Constraints

1. **No heap allocation on the audio thread.** All buffers pre-allocated in `prepareToPlay()`. Parameters via `std::atomic<float>` (APVTS). Meter values via atomics.
2. **Zero latency in baseline mode.** The original hardware has zero look-ahead. Report 0 samples to the host. (Look-ahead can be an optional mode later.)
3. **C++17/20, JUCE 7, CMake build.** Standard JUCE plugin architecture.
4. **All changes narrow and test-backed.** Each PR should be focused and include validation.
5. **Stereo with mono-compatible processing.** Support mono and stereo buses. Stereo link sums RMS (True RMS Power Summing).

---

## Risks and Unknowns

### High risk
1. **Program-dependent timing accuracy.** The junction-impedance-based time constant behavior of the original detector is nonlinear and signal-dependent. A simplified model (constant-rate release + magnitude-dependent attack) may not capture the full character. Mitigation: prototype early, A/B test against hardware recordings.

2. **OverEasy knee width.** No authoritative documentation specifies the exact knee width in dB. The 160X/160A manuals describe the behavior qualitatively. Mitigation: start with 6-12 dB knee, tune by ear against hardware/plugin references.

### Medium risk
3. **VCA saturation character.** The specific harmonic profile of the Model 200 is not fully documented (only Cytomic's measurements of a different Blackmer variant exist). Mitigation: use a tunable waveshaper, calibrate against reference recordings.

4. **RMS detector time constant.** The 22uF + 909K values give ~20ms, but the actual behavior includes the nonlinear junction impedance. Mitigation: start with 20ms exponential averaging, refine with A/B testing.

5. **Negative ratio behavior.** The exact behavior of dynamics inversion at negative ratios needs careful testing to avoid signal instability or artifacts. Mitigation: clamp maximum gain reduction, test with extreme settings.

### Low risk
6. **JUCE plugin hosting compatibility.** Standard JUCE AudioProcessor pattern is well-understood.
7. **Build system.** CMake + JUCE 7 is well-documented.
8. **Metering.** Straightforward atomic-based GUI communication.

---

## Recommended Implementation Phases

### Phase 0: Project Skeleton
- CMake build system with JUCE 7
- AudioProcessor subclass with APVTS parameter definitions
- Empty processBlock that passes audio through
- Basic GUI with parameter sliders
- CI pipeline (build + basic test)
- Catch2 or GoogleTest framework wired up

### Phase 1: Core Compression Engine
- RMS detector (exponential moving average of squared signal)
- Gain computer (hard knee, threshold, ratio)
- Program-dependent ballistics (magnitude-dependent attack, constant-rate release)
- Apply gain reduction (linear domain multiplication)
- Unit tests: gain computer math, detector response to known signals, timing verification

### Phase 2: Extended Features
- OverEasy soft knee (quadratic interpolation per Giannoulis)
- Negative ratio support (Infinity+ mode)
- Stereo link (RMS power summing)
- Dry/wet mix control
- Metering (input, output, gain reduction via atomics)

### Phase 3: Character and Tuning
- VCA saturation modeling (subtle even-order waveshaper)
- A/B testing against hardware recordings and reference plugins (UAD, Waves, Stillwell Major Tom)
- Tuning of: RMS time constant, attack/release curves, saturation amount, OverEasy knee width
- Optional: input stage coloration

### Phase 4: Polish and Production
- Full GUI design (hardware-inspired or modern)
- Preset system
- Optional oversampling (if saturation creates audible aliasing)
- Optional look-ahead mode
- Performance optimization and profiling
- Integration tests (full signal path, known input -> expected output)
- Plugin validation (AU/VST3/AAX hosting tests)

---

## Recommended Test / Validation Strategy

### Unit tests (per component)
- **Gain computer**: Known input levels and thresholds -> expected gain reduction. Test hard knee, soft knee, all ratio ranges including infinity and negative.
- **RMS detector**: Sine wave at known amplitude -> expected RMS level. Step function -> expected settling time.
- **Ballistics**: Step increase/decrease -> verify attack/release timing against published specs (15ms/10dB attack, 125 dB/sec release).
- **Parameter smoothing**: Verify no discontinuities on parameter change.

### Integration tests (full signal path)
- **Unity gain**: Ratio 1:1 -> output matches input (within floating-point tolerance).
- **Brick wall limiting**: Ratio infinity:1 -> output never exceeds threshold (within attack time).
- **Known compression**: Fixed sine wave, known threshold and ratio -> verify output level matches expected compressed level.
- **Transient preservation**: Drum hit through compressor -> verify initial transient peak is preserved relative to sustained level.

### A/B validation (against reference)
- Compare MW160 output against UAD dbx 160 and/or Waves dbx 160 with matched settings on:
  - Sine sweeps at various levels
  - Kick drum samples
  - Snare drum samples
  - Full drum loops
  - Bass guitar
- Metrics: spectral difference, envelope shape correlation, THD measurement

### Regression tests
- Golden file tests: known input WAV -> expected output WAV (bit-exact or within tolerance).
- Automate in CI.

---

## What Should Be Prototyped First

1. **Program-dependent ballistics** -- this is the highest-risk, highest-impact element. Build a standalone test that feeds impulses and step functions through the attack/release model, plots the gain reduction envelope, and compares against the published timing specs.

2. **Gain computer with ratio sweep** -- verify the math handles the full range from 1:1 through infinity to negative ratios without discontinuities.

3. **RMS detector tuning** -- feed known drum samples through the detector, plot the envelope, compare against the expected behavior (smooth energy tracking, not peak-following).

---

## Hip-Hop Flavor Considerations (When Explicitly In Scope)

The elements that create the signature punch and could be tastefully exaggerated:

1. **Transient emphasis**: The attack time allows the initial transient through. Slightly slowing the attack onset (by 1-2ms) could enhance the "crack" on snares and "knock" on kicks.

2. **Low-mid weight**: The VCA adds subtle body in the 100-300 Hz range. A very gentle saturation emphasis in this range could enhance the "weight" that hip-hop producers prize.

3. **Pumping character**: At aggressive settings (6:1+, deep GR), the constant-rate release creates rhythmic breathing. The release rate could be made very slightly slower (~100-110 dB/sec instead of 125) to enhance this effect.

4. **Snappier hard knee**: A slightly steeper knee transition (overshoot on the gain reduction, then settle) could enhance the aggressive character.

These are **not baseline behaviors** -- they would be optional tuning or a separate "character" mode. The baseline must faithfully model the hardware.

---

## What the Planner Agent Should Do Next

1. **Define the Phase 0 ticket set**: project skeleton, CMake build, JUCE boilerplate, CI, test framework.
2. **Define the Phase 1 ticket set**: core DSP components (RMS detector, gain computer, ballistics, gain application), each as a narrow, testable unit of work.
3. **Sequence the backlog**: Phase 0 -> Phase 1 -> Phase 2 -> Phase 3 -> Phase 4, with clear acceptance criteria for each ticket.
4. **Identify the first prototype task**: program-dependent ballistics model with test harness, as this is the highest-risk element.
