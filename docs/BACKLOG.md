# MW160 -- Implementation Backlog

**Created:** 2026-04-05
**Status:** Ready for execution
**Prerequisite:** Create JIRA project with key `MW160` on postscript.atlassian.net

---

## Phases

| Phase | Name | Goal | Tickets |
|-------|------|------|---------|
| 0 | Project Skeleton | Buildable, testable plugin with CI and passthrough audio | MW160-1 through MW160-5 |
| 1 | Core Compression Engine | Minimum viable compressor matching classic VCA compressor behavior | MW160-6 through MW160-10 |
| 2 | Extended Features | Complete the reference hardware control set (soft knee, stereo link, mix, metering) | MW160-11 through MW160-14 |
| 3 | Character & Calibration | VCA saturation and tuning against hardware reference | MW160-15 through MW160-17 |
| 4 | UI, Polish & Production | GUI, presets, optimization, validation | MW160-18 through MW160-22 |

---

## Phase 0: Project Skeleton

### MW160-1: Initialize CMake build system with JUCE 7

**Priority:** Highest
**Depends on:** Nothing
**Risk:** Low

**Summary:** Set up the CMake build system that fetches JUCE 7 (via FetchContent or CPM) and defines the plugin target for VST3, AU, and standalone formats.

**Scope:**
- Top-level `CMakeLists.txt` with `cmake_minimum_required(VERSION 3.22)`
- Fetch JUCE 7 via `FetchContent` from the official GitHub repo
- Define plugin target using `juce_add_plugin()` with:
  - Plugin name: MW160
  - Formats: VST3, AU, Standalone
  - Company: mw Audio (or placeholder)
  - Plugin code / manufacturer code placeholders
- Create `src/` directory with placeholder `PluginProcessor.h`, `PluginProcessor.cpp`, `PluginEditor.h`, `PluginEditor.cpp`
- Verify the project configures and builds successfully on macOS and Linux

**Acceptance criteria:**
- [ ] `cmake -B build && cmake --build build` succeeds
- [ ] Standalone target launches and shows an empty window
- [ ] VST3 binary is produced in the build output

---

### MW160-2: Create AudioProcessor skeleton with APVTS

**Priority:** Highest
**Depends on:** MW160-1
**Risk:** Low

**Summary:** Implement `MW160Processor` with all parameter definitions via `AudioProcessorValueTreeState`. The `processBlock` should pass audio through unmodified (unity gain passthrough).

**Scope:**
- `MW160Processor` inherits `juce::AudioProcessor`
- APVTS with all six parameters:
  - `threshold`: float, -40 to +20, default 0, "dBu"
  - `ratio`: float, 1.0 to 60.0 (where 60 = infinity:1; values > 60 map to negative ratios), default 1.0
  - `outputGain`: float, -20 to +20, default 0, "dB"
  - `softKnee`: bool, default off
  - `stereoLink`: bool, default on
  - `mix`: float, 0 to 100, default 100, "%"
- `prepareToPlay()` initializes sample rate and block size
- `processBlock()` passes audio through unmodified
- Support mono and stereo bus layouts
- `createEditor()` returns a `juce::GenericAudioProcessorEditor` (temporary)

**Acceptance criteria:**
- [ ] Plugin loads in a DAW or standalone, shows generic parameter UI
- [ ] Audio passes through at unity gain (verified by ear or measurement)
- [ ] All six parameters are visible and adjustable in the generic editor
- [ ] Mono and stereo bus layouts are accepted

---

### MW160-3: Set up test framework with Catch2

**Priority:** Highest
**Depends on:** MW160-1
**Risk:** Low

**Summary:** Integrate Catch2 (v3) into the CMake build and create a test runner target. Include one trivial test to verify the framework works.

**Scope:**
- Fetch Catch2 v3 via `FetchContent`
- Create `tests/` directory
- Define a `MW160Tests` executable target linked against Catch2 and the plugin's DSP source (compiled as a static library or object library, NOT linked against the JUCE plugin target directly)
- Create `tests/test_smoke.cpp` with a trivial passing test
- Add `ctest` integration

**Acceptance criteria:**
- [ ] `cmake --build build --target MW160Tests` compiles
- [ ] `ctest --test-dir build` runs and reports 1 passing test
- [ ] Test target can `#include` plugin source headers

---

### MW160-4: Set up GitHub Actions CI

**Priority:** High
**Depends on:** MW160-3
**Risk:** Low

**Summary:** Create a GitHub Actions workflow that builds the plugin and runs tests on push and PR.

**Scope:**
- `.github/workflows/build-and-validate.yml`
- Matrix: macOS-latest (Xcode/Clang), ubuntu-latest (GCC)
- Steps: checkout, configure CMake, build, run tests
- Cache CMake build and JUCE fetch for faster runs
- Build standalone + VST3 targets
- Run Catch2 test suite

**Acceptance criteria:**
- [ ] Push to any branch triggers the workflow
- [ ] Build succeeds on both macOS and Linux
- [ ] Tests run and pass on both platforms
- [ ] Build artifacts are retained (optional but nice)

---

### MW160-5: Create minimal placeholder GUI with parameter controls

**Priority:** Medium
**Depends on:** MW160-2
**Risk:** Low

**Summary:** Replace the generic editor with a minimal custom `MW160Editor` that has labeled sliders for all parameters and a placeholder layout. This is NOT the final GUI -- just enough to test parameters during development.

**Scope:**
- `MW160Editor` inherits `juce::AudioProcessorEditor`
- `juce::Slider` + `juce::Label` for threshold, ratio, output gain, mix
- `juce::ToggleButton` for soft knee and Stereo Link
- `SliderAttachment` / `ButtonAttachment` via APVTS
- Simple vertical or grid layout, ~400x300 window
- No custom look-and-feel, no graphics

**Acceptance criteria:**
- [ ] All six parameters have corresponding controls in the editor
- [ ] Moving a slider updates the APVTS parameter value
- [ ] Saving/loading plugin state preserves parameter values
- [ ] Editor renders without visual glitches at default size

---

## Phase 1: Core Compression Engine

### MW160-6: Implement RMS level detector

**Priority:** Highest
**Depends on:** MW160-3 (needs test framework)
**Risk:** Medium (RMS time constant tuning)

**Summary:** Build a true RMS level detector as a standalone DSP class. This computes the RMS energy of the input signal using exponential moving average of the squared signal, matching the reference hardware's ~20ms detection time constant.

**Scope:**
- New class: `RmsDetector` in `src/dsp/RmsDetector.h/.cpp`
- Interface:
  - `void prepare(double sampleRate)` -- compute coefficient from sample rate and 20ms time constant
  - `void reset()` -- zero internal state
  - `float processSample(float input)` -- returns current RMS level in linear domain
  - `float getLevel_dB()` -- returns current level in dB (convenience)
- Coefficient: `coeff = exp(-1.0 / (0.020 * sampleRate))`
- Internal state: `rmsSquared` (exponential average of input^2)
- Output: `sqrt(rmsSquared)`
- Avoid `log(0)` with a small epsilon floor (1e-30f)

**Unit tests (`tests/test_rms_detector.cpp`):**
- [ ] 1 kHz sine at 0 dBFS -> RMS converges to ~-3.01 dB (1/sqrt(2))
- [ ] DC signal at 0.5 -> RMS converges to 0.5 (within tolerance)
- [ ] Step from silence to 0 dBFS sine -> settling time within 3x tau (~60ms to reach 95%)
- [ ] `reset()` zeros the state
- [ ] Zero input -> RMS stays at or near zero

**Acceptance criteria:**
- [ ] All unit tests pass
- [ ] Class is header-only or compiled into the DSP library target
- [ ] No heap allocation in `processSample()`

---

### MW160-7: Implement gain computer (hard knee + full ratio range)

**Priority:** Highest
**Depends on:** MW160-3
**Risk:** Medium (negative ratio edge cases)

**Summary:** Build the gain computer that converts an input level (dB) to a gain reduction value (dB) given the threshold and ratio. Must handle the full range: 1:1 through infinity:1 to negative ratios.

**Scope:**
- New class: `GainComputer` in `src/dsp/GainComputer.h/.cpp`
- Interface:
  - `float computeGainReduction(float inputLevel_dB, float threshold_dB, float ratio)` -- returns gain reduction in dB (negative = attenuation)
  - A separate method or flag for soft knee (Phase 2, stubbed here)
- Hard knee: below threshold = 0 dB GR; above threshold = `excess * (1/R - 1)`
- Ratio encoding: 1.0-59.99 = normal ratios; 60.0 = infinity:1; >60.0 maps to negative ratios
  - Helper to convert parameter value to effective ratio for the formula
  - At infinity:1, gain reduction = -excess (output stays at threshold)
  - Negative ratios: gain reduction exceeds excess (dynamics inversion)
- Clamp maximum gain reduction to -60 dB to prevent runaway at negative ratios

**Unit tests (`tests/test_gain_computer.cpp`):**
- [ ] Input below threshold -> 0 dB gain reduction (any ratio)
- [ ] Input 10 dB above threshold, 2:1 -> -5 dB gain reduction
- [ ] Input 10 dB above threshold, 4:1 -> -7.5 dB gain reduction
- [ ] Input 10 dB above threshold, infinity:1 -> -10 dB gain reduction
- [ ] Negative ratio -> gain reduction exceeds excess
- [ ] Gain reduction never exceeds -60 dB clamp
- [ ] Input exactly at threshold -> 0 dB gain reduction

**Acceptance criteria:**
- [ ] All unit tests pass
- [ ] Ratio sweep from 1:1 through infinity to negative produces no discontinuities
- [ ] No heap allocation in compute path

---

### MW160-8: Implement program-dependent ballistics

**Priority:** Highest
**Depends on:** MW160-3
**Risk:** HIGH (most critical element for the 160's character)

**Summary:** Build the program-dependent attack/release envelope follower. This is the single most important DSP element -- it determines the 160's signature punch. Attack speed scales inversely with transient magnitude; release runs at a constant ~125 dB/sec rate.

**Scope:**
- New class: `Ballistics` in `src/dsp/Ballistics.h/.cpp`
- Interface:
  - `void prepare(double sampleRate)`
  - `void reset()`
  - `float processSample(float targetGR_dB)` -- smooths target gain reduction, returns current GR
- Attack behavior (target more negative than current = louder transient):
  - Attack time scales inversely with magnitude of change
  - Target: 15ms for 10 dB, 5ms for 20 dB, 3ms for 30 dB
  - Use exponential smoothing with magnitude-dependent coefficient
  - Clamp attack time to [1ms, 50ms] range
- Release behavior (target less negative than current = signal getting quieter):
  - Constant rate: ~125 dB/sec
  - Linear ramp in dB domain: `currentGR += releaseRate_dBperSample`
  - Stop when current reaches target (don't overshoot past 0)

**Unit tests (`tests/test_ballistics.cpp`):**
- [ ] 10 dB step increase: attack reaches 90% of target within ~15ms (within 20% tolerance)
- [ ] 20 dB step increase: attack reaches 90% of target within ~5ms (within 20% tolerance)
- [ ] 30 dB step increase: attack reaches 90% of target within ~3ms (within 20% tolerance)
- [ ] 10 dB release: completes in ~80ms (within 20% tolerance)
- [ ] 50 dB release: completes in ~400ms (within 20% tolerance)
- [ ] Release rate is approximately constant in dB/sec across different GR depths
- [ ] `reset()` returns state to 0 dB GR

**Acceptance criteria:**
- [ ] All timing tests pass within tolerance
- [ ] Attack is faster for larger transients (monotonically)
- [ ] Release rate is constant (within 10% across 5-50 dB range)
- [ ] No heap allocation in `processSample()`

**Notes:** This is the highest-risk ticket. If the simplified model doesn't match the hardware character well enough, a junction-impedance model (Phase 3 refinement) may be needed. The initial implementation should be structured to allow the smoothing algorithm to be swapped.

---

### MW160-9: Integrate core compression pipeline in processBlock

**Priority:** Highest
**Depends on:** MW160-2, MW160-6, MW160-7, MW160-8
**Risk:** Medium

**Summary:** Wire the three DSP components (RMS detector, gain computer, ballistics) together inside `MW160Processor::processBlock()` to create a working compressor. This is the integration point -- each component was unit-tested independently; now they work together.

**Scope:**
- In `prepareToPlay()`:
  - Initialize `RmsDetector`, `GainComputer`, `Ballistics` with sample rate
  - Pre-allocate per-channel state arrays (2 channels max)
- In `processBlock()`, per sample per channel:
  1. Read input sample
  2. Feed to `RmsDetector::processSample()` -> get RMS level
  3. Convert to dB
  4. Feed to `GainComputer::computeGainReduction()` with current threshold and ratio
  5. Feed target GR to `Ballistics::processSample()` -> get smoothed GR
  6. Convert smoothed GR from dB to linear gain: `gain = pow(10, GR_dB / 20)`
  7. Multiply input sample by gain
  8. Apply output gain (from parameter)
  9. Write to output buffer
- Read threshold, ratio, output gain from APVTS atomic pointers
- For now: process each channel independently (stereo link comes in Phase 2)
- For now: no dry/wet mix (comes in Phase 2)

**Integration tests (`tests/test_compressor_integration.cpp`):**
- [ ] Ratio 1:1 -> output matches input (within floating-point tolerance)
- [ ] Sine at +10 dB above threshold, 4:1 ratio -> output level matches expected compressed level (within 1 dB)
- [ ] Silence in -> silence out (no noise added)
- [ ] Output gain of +6 dB boosts signal by 6 dB (within 0.5 dB)

**Acceptance criteria:**
- [ ] Plugin compresses audio when threshold is lowered below signal level
- [ ] Ratio control changes compression depth
- [ ] Output gain control works independently
- [ ] No clicks, pops, or artifacts during steady-state compression
- [ ] All integration tests pass

---

### MW160-10: Add parameter smoothing

**Priority:** High
**Depends on:** MW160-9
**Risk:** Low

**Summary:** Add per-sample parameter smoothing to threshold, ratio, and output gain to prevent zipper noise and clicks when parameters change during playback.

**Scope:**
- Add `juce::SmoothedValue<float>` members for:
  - `thresholdSmoothed` (linear smoothing, 20ms)
  - `ratioSmoothed` (linear smoothing, 20ms)
  - `outputGainSmoothed` (multiplicative smoothing, 20ms) -- multiplicative because it's a gain factor
- In `prepareToPlay()`: call `reset(sampleRate, 0.020)` on each
- At the start of `processBlock()`: set target values from APVTS
- In the per-sample loop: call `getNextValue()` for each smoothed parameter
- Do NOT smooth the gain reduction itself -- the ballistics filter already handles that

**Tests (`tests/test_parameter_smoothing.cpp`):**
- [ ] Abrupt threshold change during processing produces no discontinuity > 0.1 dB between consecutive samples
- [ ] Abrupt output gain change produces no click (no sample-to-sample jump > 1 dB)
- [ ] After smoothing settles, parameter value matches target exactly

**Acceptance criteria:**
- [ ] Sweeping any knob during playback produces no audible clicks or zipper noise
- [ ] All smoothing tests pass
- [ ] Parameter automation from DAW is smooth

---

## Phase 2: Extended Features

### MW160-11: Implement soft knee

**Priority:** High
**Depends on:** MW160-7, MW160-9
**Risk:** Medium (knee width needs tuning)

**Summary:** Add the soft knee mode to the gain computer, switchable via the `softKnee` parameter. Uses quadratic interpolation per Giannoulis et al.

**Scope:**
- Extend `GainComputer` with a `computeGainReduction()` overload or mode that accepts a knee width parameter
- Soft knee formula (Giannoulis):
  - Below `T - W/2`: 0 dB GR
  - Above `T + W/2`: full ratio applied
  - In knee region: quadratic interpolation `(1/R - 1) * (x - T + W/2)^2 / (2*W)`
- Default knee width: 10 dB (tunable constant, can be refined in Phase 3)
- Wire to the `softKnee` boolean parameter in `processBlock()`

**Unit tests (`tests/test_gain_computer.cpp` -- extend existing):**
- [ ] soft knee: input well below threshold -> 0 dB GR
- [ ] soft knee: input well above threshold -> same GR as hard knee
- [ ] soft knee: input in knee region -> GR is between 0 and full-ratio GR
- [ ] soft knee: gain reduction curve is continuous (no jumps at knee boundaries)
- [ ] soft knee: at threshold center, GR is approximately half of hard-knee GR

**Acceptance criteria:**
- [ ] soft knee toggle switches between hard and soft knee in real time
- [ ] Soft knee produces audibly gentler compression onset
- [ ] No discontinuity when toggling soft knee during playback
- [ ] All tests pass

---

### MW160-12: Implement stereo link (RMS power summing)

**Priority:** High
**Depends on:** MW160-9
**Risk:** Low

**Summary:** When stereo link is enabled, sum the squared RMS signals from both channels before computing gain reduction, so both channels receive identical GR. This matches the reference hardware's stereo link behavior.

**Scope:**
- When `stereoLink` is on and buffer has 2 channels:
  1. Compute `inputSquared` for both channels
  2. Average or sum the squared values: `linkedSquared = (L^2 + R^2) / 2`
  3. Feed the linked value to a single shared `RmsDetector`
  4. Use the shared GR for both channels
- When `stereoLink` is off: process each channel independently (existing behavior)
- When mono: ignore stereo link (single channel)

**Unit tests (`tests/test_stereo_link.cpp`):**
- [ ] Stereo link on + matched L/R signal -> identical GR on both channels
- [ ] Stereo link on + signal on L only -> both channels get GR (based on combined energy)
- [ ] Stereo link off + signal on L only -> only L gets GR, R passes through
- [ ] Mono input -> stereo link parameter has no effect

**Acceptance criteria:**
- [ ] Stereo-linked compression sounds cohesive (no image shift)
- [ ] Toggling stereo link during playback produces no artifacts
- [ ] All tests pass

---

### MW160-13: Implement dry/wet mix control

**Priority:** Medium
**Depends on:** MW160-9
**Risk:** Low

**Summary:** Add parallel compression support via the mix parameter. At 100%, output is fully compressed. At 0%, output is dry. Intermediate values blend linearly.

**Scope:**
- After computing compressed output sample, blend with dry input:
  `output = dry * (1 - mixAmount) + compressed * mixAmount`
- `mixAmount` = `mix / 100.0f` (parameter is 0-100%)
- Apply output gain AFTER the mix (so output gain affects both dry and wet equally -- matching the hardware's post-compression makeup gain concept)
- Smooth the mix parameter with `SmoothedValue` (20ms, linear)

**Unit tests (`tests/test_mix.cpp`):**
- [ ] Mix = 100% -> output equals compressed signal
- [ ] Mix = 0% -> output equals dry input (bypasses compression)
- [ ] Mix = 50% -> output is average of dry and compressed
- [ ] Output gain applies equally at any mix setting

**Acceptance criteria:**
- [ ] Mix knob sweeps smoothly with no artifacts
- [ ] Parallel compression sounds correct (retains punch of original + body of compressed)
- [ ] All tests pass

---

### MW160-14: Implement metering (input, output, gain reduction)

**Priority:** Medium
**Depends on:** MW160-9, MW160-5
**Risk:** Low

**Summary:** Publish input level, output level, and gain reduction to the GUI via atomic variables. Add meter display components to the editor.

**Scope:**
- Audio thread (in `processBlock()`):
  - Compute peak or RMS input level per block, write to `std::atomic<float> inputLevel`
  - Compute peak or RMS output level per block, write to `std::atomic<float> outputLevel`
  - Write current smoothed gain reduction to `std::atomic<float> gainReduction`
- GUI thread (in `MW160Editor`):
  - `juce::Timer` callback at ~30 Hz reads the three atomic values
  - Simple bar/level indicators for input, output, and GR
  - GR meter shows 0 to -40 dB range (inverted -- more GR = longer bar)
- No custom graphics -- simple colored rectangles or JUCE `Slider` in read-only mode

**Acceptance criteria:**
- [ ] Meters respond to audio in real time
- [ ] GR meter shows correct gain reduction (verified by setting known threshold/ratio)
- [ ] No audio thread contention (atomics only, no locks)
- [ ] Meters return to zero when audio stops
- [ ] GUI repaint does not cause audio glitches

---

## Phase 3: Character & Calibration

### MW160-15: Implement VCA saturation modeling

**Priority:** Medium
**Depends on:** MW160-9
**Risk:** Medium (character tuning is subjective)

**Summary:** Add the subtle even-order harmonic distortion characteristic of the discrete VCA gain element. This is the "warmth and graininess" of the original hardware. The amount of saturation should be gain-dependent (more at deeper GR).

**Scope:**
- New class: `VcaSaturation` in `src/dsp/VcaSaturation.h/.cpp`
- Soft asymmetric waveshaper producing predominantly 2nd harmonic:
  ```
  float saturate(float x, float amount) {
      return x + amount * (x*x * 0.05f - x*x*x * 0.02f);
  }
  ```
- `amount` parameter tied to current gain reduction: more GR = more saturation
  - Suggested mapping: `amount = clamp(abs(currentGR_dB) / 60.0f, 0, 1) * maxSaturation`
  - `maxSaturation` tuned to produce ~0.1-0.2% THD at moderate GR (10-20 dB)
- Applied AFTER gain reduction, BEFORE output gain (matching the signal path position of the VCA)
- Internal constant `maxSaturation` -- not a user-facing parameter (yet)

**Unit tests (`tests/test_vca_saturation.cpp`):**
- [ ] Zero input -> zero output (no DC offset)
- [ ] Low-level signal -> negligible distortion (< 0.01% THD)
- [ ] Moderate GR -> measurable but subtle distortion (0.05-0.2% THD)
- [ ] 2nd harmonic is dominant over 3rd harmonic (even-order character)
- [ ] Output level approximately matches input level (saturation is coloration, not gain change)

**Acceptance criteria:**
- [ ] Adds audible warmth on percussive material at moderate-to-heavy compression
- [ ] Does not audibly distort on clean, low-GR material
- [ ] THD measurement matches target range
- [ ] All tests pass

---

### MW160-16: Calibrate program-dependent timing against reference

**Priority:** High
**Depends on:** MW160-8, MW160-9
**Risk:** High (subjective + may require model refinement)

**Summary:** A/B test the ballistics model against reference recordings or reference plugin output. Adjust attack/release curves to match the published reference hardware specs and subjective character.

**Scope:**
- Create test fixtures with:
  - Known drum hits (kick, snare) at various levels
  - Step functions at 10 dB, 20 dB, 30 dB above threshold
- Record gain reduction envelope from MW160
- Compare against:
  - Published timing specs (15ms/10dB attack, 125 dB/sec release)
  - Reference plugin output if available (UAD, Waves)
- Adjust `Ballistics` constants as needed:
  - Attack scaling factor
  - Attack clamp range
  - Release rate
- Document any deviations from the simplified model and whether a junction-impedance model is warranted

**Acceptance criteria:**
- [ ] Attack timing within 20% of published specs for 10/20/30 dB steps
- [ ] Release rate within 15% of 125 dB/sec across 5-50 dB GR range
- [ ] Drum hits through the compressor preserve the characteristic "crack" (transient passes before clamping)
- [ ] Calibration results documented in `docs/calibration-notes.md`

---

### MW160-17: Calibrate gain staging and detector response

**Priority:** Medium
**Depends on:** MW160-6, MW160-9
**Risk:** Medium

**Summary:** Verify the full gain staging chain matches expected behavior. Ensure the RMS detector tracks perceived loudness correctly and the gain computer produces the right compression depth.

**Scope:**
- Test with calibrated signals:
  - 1 kHz sine at 0 dBFS, -6 dBFS, -12 dBFS, -20 dBFS
  - Pink noise at various levels
- Verify: threshold at 0 dB + ratio 4:1 + input at +10 dB -> exactly 7.5 dB of gain reduction (steady-state)
- Verify: output gain is additive and independent of threshold
- Tune RMS time constant if detector is too fast (peak-like) or too slow (sluggish)
- Verify metering accuracy: displayed levels match actual signal levels

**Acceptance criteria:**
- [ ] Steady-state gain reduction matches theoretical prediction within 0.5 dB
- [ ] Output gain is purely additive (no interaction with threshold or ratio)
- [ ] RMS detector tracks energy, not peaks (verified with different waveforms at same RMS level)
- [ ] Results documented

---

## Phase 4: UI, Polish & Production

### MW160-18: Design and implement custom GUI

**Priority:** Medium
**Depends on:** MW160-14
**Risk:** Low (cosmetic, no DSP risk)

**Summary:** Replace the placeholder editor with a custom-designed GUI. Style TBD -- could be hardware-inspired (vintage panel) or modern/minimal. Must include proper knobs, meters, and status indicators.

**Scope:**
- Custom `LookAndFeel` subclass for knobs, buttons, and meters
- Hardware-inspired or modern panel design
- Rotary knobs for Threshold, Ratio, Output Gain, Mix
- Toggle buttons for soft knee and Stereo Link
- LED-style indicators: above/below threshold (green/amber/red)
- Meter displays: input level, output level, gain reduction
- Resizable or fixed-size window (target ~600x400)
- All graphics rendered via JUCE `Graphics` (no external image assets initially)

**Acceptance criteria:**
- [ ] All parameters are controllable via the custom UI
- [ ] Meters display accurately
- [ ] UI renders correctly on macOS and Windows at 1x and 2x DPI
- [ ] Plugin state save/load preserves UI state
- [ ] Looks professional and cohesive

---

### MW160-19: Implement preset system

**Priority:** Low
**Depends on:** MW160-18
**Risk:** Low

**Summary:** Add factory presets covering common use cases from the hardware reference documentation.

**Scope:**
- Preset infrastructure: save/recall parameter sets via APVTS state
- Factory presets:
  - "Kick Punch" -- Hard knee, 4:1, threshold for 6-10 dB GR
  - "Snare Snap" -- Hard knee, 4:1-5:1, threshold for 4-6 dB GR
  - "Drum Bus Glue" -- soft knee, 4:1, moderate threshold
  - "Bass Control" -- soft knee, 3:1, 3-5 dB constant GR
  - "Parallel Smash" -- Hard knee, 6:1+, deep GR, 40-60% mix
  - "Gentle Leveling" -- soft knee, 2:1, light GR
  - "Brick Wall" -- Hard knee, Infinity:1
- Preset browser in the UI

**Acceptance criteria:**
- [ ] All factory presets load correctly
- [ ] User can save and recall custom presets
- [ ] Preset names display in DAW preset browser (where supported)

---

### MW160-20: Performance optimization and profiling

**Priority:** Medium
**Depends on:** MW160-15 (full signal path should be in place)
**Risk:** Low

**Summary:** Profile the processBlock and ensure the plugin meets real-time performance targets. Optimize hot paths if needed.

**Scope:**
- Profile `processBlock()` CPU usage at 44.1 kHz and 96 kHz
- Target: < 5% single-core CPU at 44.1 kHz, stereo, 512-sample buffer
- Verify no heap allocation in audio path (run with allocation-detecting tools or JUCE's `jassert` in debug builds)
- Optimize if needed:
  - Replace `std::pow`/`std::log10`/`std::exp` with fast approximations if they're hot
  - Consider block-based processing (compute GR for a block, then apply) if per-sample overhead is high
  - SIMD opportunities (unlikely needed at this scale)
- Add optional oversampling (2x, JUCE `dsp::Oversampling`) ONLY if saturation produces audible aliasing above ~16 kHz
  - If added: report latency to host via `setLatencySamples()`

**Acceptance criteria:**
- [ ] CPU usage within target at 44.1 kHz and 96 kHz
- [ ] No heap allocation detected in audio path
- [ ] No audible aliasing artifacts (verified with spectrum analyzer)
- [ ] Performance numbers documented

---

### MW160-21: Integration tests and golden file regression

**Priority:** High
**Depends on:** MW160-15, MW160-11, MW160-12
**Risk:** Low

**Summary:** Create end-to-end regression tests that feed known WAV inputs through the full plugin and compare against expected outputs. These catch unintended behavior changes during future development.

**Scope:**
- Test infrastructure: load WAV file -> run through `processBlock()` -> compare output WAV against golden reference
- Tolerance: within 0.01 dB RMS difference (or bit-exact if deterministic)
- Test cases:
  - 1 kHz sine, various thresholds and ratios
  - Drum loop, typical compression settings
  - Silence -> verify no noise added
  - Extreme settings (infinity:1, deep threshold)
- Golden files stored in `tests/golden/`
- Script to regenerate golden files when intentional changes are made

**Acceptance criteria:**
- [ ] All golden file tests pass in CI
- [ ] Test failures clearly indicate which parameter/signal combination deviated
- [ ] Regeneration script works and is documented

---

### MW160-22: Plugin validation across hosts

**Priority:** Medium
**Depends on:** MW160-18, MW160-21
**Risk:** Medium (host-specific quirks)

**Summary:** Validate the plugin loads and functions correctly in major DAWs and plugin hosts.

**Scope:**
- Test in:
  - JUCE AudioPluginHost (all formats)
  - Reaper (VST3)
  - Logic Pro (AU) -- macOS only
  - Ableton Live (VST3/AU)
  - Pro Tools (AAX) -- if AAX target is built
- Verify for each host:
  - Plugin loads without errors
  - Audio processing works correctly
  - Parameters automate correctly
  - Preset save/load works
  - Bypass works
  - Stereo and mono configurations
- Document any host-specific issues

**Acceptance criteria:**
- [ ] Plugin passes JUCE AudioPluginHost validation
- [ ] No crashes or errors in any tested host
- [ ] Automation works in at least 2 DAWs
- [ ] Known issues documented

---

## Dependency Graph

```
Phase 0 (sequential):
  MW160-1 (CMake) -> MW160-2 (Processor) -> MW160-5 (GUI placeholder)
  MW160-1 (CMake) -> MW160-3 (Tests)     -> MW160-4 (CI)

Phase 1 (fan-out from Phase 0, then converge):
  MW160-3 -> MW160-6 (RMS detector)    ─┐
  MW160-3 -> MW160-7 (Gain computer)   ─┼─> MW160-9 (Integration) -> MW160-10 (Smoothing)
  MW160-3 -> MW160-8 (Ballistics)      ─┘

Phase 2 (fan-out from MW160-9):
  MW160-9 -> MW160-11 (soft knee)
  MW160-9 -> MW160-12 (Stereo link)
  MW160-9 -> MW160-13 (Mix)
  MW160-9 + MW160-5 -> MW160-14 (Metering)

Phase 3 (fan-out from MW160-9):
  MW160-9 -> MW160-15 (VCA saturation)
  MW160-9 -> MW160-16 (Timing calibration)
  MW160-9 -> MW160-17 (Gain staging calibration)

Phase 4 (depends on earlier phases):
  MW160-14 -> MW160-18 (Custom GUI) -> MW160-19 (Presets)
  MW160-15 -> MW160-20 (Performance)
  MW160-15 + MW160-11 + MW160-12 -> MW160-21 (Golden files)
  MW160-18 + MW160-21 -> MW160-22 (Host validation)
```

---

## Execution Priority (top-down)

| Priority | Tickets | Rationale |
|----------|---------|-----------|
| **P0 -- Do first** | MW160-1, MW160-2, MW160-3 | Can't do anything without a buildable, testable project |
| **P1 -- Unblock DSP** | MW160-4, MW160-6, MW160-7, MW160-8 | CI + the three core DSP components (independent, can parallelize) |
| **P2 -- Core integration** | MW160-9, MW160-10, MW160-5 | Wire the engine together; placeholder GUI for manual testing |
| **P3 -- Feature complete** | MW160-11, MW160-12, MW160-13, MW160-14 | Complete the reference hardware control set |
| **P4 -- Character** | MW160-15, MW160-16, MW160-17 | What makes it sound like a 160 vs a generic compressor |
| **P5 -- Ship** | MW160-18, MW160-19, MW160-20, MW160-21, MW160-22 | Polish, validate, release |

---

## Open Questions & Assumptions

1. **JIRA project:** The `MW160` project does not exist on `postscript.atlassian.net`. It needs to be created before tickets can be filed.
2. **AAX format:** AAX requires a PACE iLok account and signing. Assume AAX is deferred; build VST3 + AU + Standalone initially.
3. **Windows CI:** GitHub Actions Windows runners with MSVC are slower and more complex. Assume macOS + Linux CI initially; add Windows later.
4. **Soft knee width:** Research suggests 6-12 dB. Defaulting to 10 dB; will need A/B tuning in Phase 3.
5. **RMS time constant:** Starting at 20ms per hardware component values. May need adjustment in Phase 3.
6. **Test framework:** Research says "Catch2 or GoogleTest TBD." This backlog assumes Catch2 v3 (simpler CMake integration, lighter weight). Switch to GoogleTest if preferred.
7. **Hip-hop flavor enhancements:** Not scoped in any ticket. The baseline must be faithful first. A future Phase 5 could add a "Character" mode with the flavor adjustments described in the research summary.

---

# QA Findings --- 2026-04-11 Orchestrated Pass

**Scope.** This section is populated by a coordinated QA + visual remediation
pass run on 2026-04-11. The original implementation backlog (Phases 0--4
above) has shipped; this section captures defects, conformance gaps, and
design drift found against `docs/REFERENCE.md`.

**Severity levels.** `CRITICAL` (ship blocker / legal risk) > `HIGH` (user-
visible correctness bug) > `MEDIUM` (correctness under edge conditions,
polish gap) > `LOW` (cosmetic, nice-to-have).

**Owner tags.** `owner:dev-agent` (code fix), `owner:design-agent` (visual
fix), `owner:ci-agent` (workflow / infra), `owner:docs-agent` (doc fix).

**Entry format.** Each entry MUST include: severity, owner tag, repro /
context, expected, actual, file:line if relevant. Entries written as
sub-sections below are consolidated from per-agent findings files in
`qa-findings/`.

---

## CRITICAL --- Trademark and brand reference scrub

### QA-TM-001: Pervasive brand / trademark references throughout repo

**Severity:** CRITICAL
**Owner:** `owner:dev-agent`
**Found by:** orchestrator (pre-agent repo scan)
**Tags:** legal, trademark, refactor, hygiene

**Context.** The repository working name is `mw160` and the plugin is an
"inspired by" VCA feedback compressor. Per project policy no manufacturer,
product, or trademarked-feature name may appear in code, UI strings, file
names, documentation, assets, or commit messages. A scan for brand/trademark
terms (case-insensitive) across all source, test, doc, CMake, CI, and script
files returned **175 occurrences across 33 files**.

**Observed hot spots (not exhaustive):**

- `research/vca-compressor-hardware-reference.md` --- formerly
  the hardware-reference file (formerly named after the reference model);
  the file contained 38 in-content references. Renamed, and all internal
  brand terms replaced with neutral language.
- `docs/BACKLOG.md` (this file, original Phase 0--4 section) --- 25
  occurrences, incl. "classic VCA compressor" in Phase 1 description,
  "reference hardware" in Phase 2 description, "discrete VCA" in MW160-15,
  "soft knee" as a named ticket and parameter throughout. (These have since
  been neutralized in this scrub pass.)
- `docs/calibration-notes.md` --- 4 references, incl. a section titled
  "Reference Specs (Published Hardware)" — requires renaming to neutral form.
- `src/dsp/Ballistics.h:8` --- doc comment named brand.
- `src/dsp/Ballistics.cpp:29` --- inline comment named brand.
- `src/dsp/Compressor.h`, `src/dsp/Compressor.cpp`, `src/dsp/GainComputer.h`,
  `src/dsp/RmsDetector.h`, `src/dsp/VcaSaturation.h`, `src/dsp/VcaSaturation.cpp`,
  `src/FactoryPresets.h`, `src/PluginProcessor.{h,cpp}`, `src/PluginEditor.{h,cpp}`,
  `src/PresetManager.cpp`, `src/gui/LedMeter.h` --- 24 further occurrences
  across the main source tree (mostly comments and string literals for
  the soft-knee feature).
- All 17 test files --- 47 occurrences, predominantly the legacy brand-adjacent
  parameter ID and brand-adjacent test names.
- `.github` / CI is clean.
- `CMakeLists.txt` is clean (company name already neutralized to
  "mw Audio", product name to "MW160").

**Parameter ID migration hazard.** The JUCE APVTS legacy parameter ID
was persisted inside user preset files and saved DAW sessions. The
rename to `"softKnee"` would silently break state restoration for every
existing user without a migration path. The
dev-agent MUST ship a migration path: accept the old ID when loading
state, map it to the new ID, log a single one-shot notice, and write
the new ID on next save. This is the only safe way to scrub the
parameter.

**Expected.** Zero occurrences of brand/trademark terms in the repo,
case-insensitive, across every file type. The neutral replacement
vocabulary is documented in `docs/REFERENCE.md`:

- brand-adjacent soft knee terms --> `softKnee` (parameter ID, UI label,
  variable names, comments, test names)
- "classic VCA compressor" / "reference hardware" (prose descriptions)
- "discrete VCA" or "VCA gain element" (circuit-class references)

**Actual.** 175 brand/trademark references were found across 33 files at
the time of the 2026-04-11 QA scan. Detailed file list above; the full
authoritative list was regenerable with a case-insensitive search for
the four brand stems.

**Scope boundary.** The 2026-04-11 QA pass surfaced this work; a
dedicated follow-on ticket (JIRA: `MW160-24` suggested) executes the
scrub with the migration path above. The QA orchestrator pass and all
Phase 2 / 3 agents used neutral vocabulary in any *new* content they
wrote, so the gap did not grow while the backlog was being consumed.

**Verification.** After the scrub:
1. A case-insensitive search for brand stems across the full repo
   returns zero hits.
2. Loading a pre-scrub saved state restores the legacy parameter value
   to the renamed `softKnee` parameter (migration test).
3. All Catch2 tests still pass (the rename is behavior-preserving).
4. `pluginval --strictness-level 10` passes post-rename.

---

## CRITICAL (remaining)

### QA-UX-001: Threshold parameter labeled `dBu` but DSP operates in `dBFS`

**Severity:** CRITICAL
**Owner:** `owner:dev-agent`
**Source:** qa-ux agent
**Context:** The APVTS parameter declares label `dBu`
(`PluginProcessor.cpp:31`) but the detector and gain computer consume
raw-float `dBFS` (`RmsDetector.h`, `Compressor.cpp:74-80`). No
calibration shift is applied anywhere. A user dialing `0 dBu` expects
compression onset around `-18 dBFS`; they get onset at `0 dBFS`.
The factory preset file (`FactoryPresets.h:8`) also uses the misleading
`dBu` convention, so every factory preset is off by ~18 dB relative to
the user's intuition.
**Expected:** REFERENCE.md §2.1 — "The chosen convention must be
documented in the UI and the manual — ambiguity here is a UX bug."
**Actual:** Label says `dBu`, DSP treats as `dBFS`. ~18 dB lie.
**Location:** `src/PluginProcessor.cpp:26-31`; `src/dsp/Compressor.cpp:74-80`;
`src/dsp/RmsDetector.h`; `src/FactoryPresets.h:8`.
**Fix direction:** Change label to `dBFS` and rebalance factory presets,
*or* apply a +18 dB reference shift in the comparator so `0 dBu` actually
maps to `-18 dBFS`. Recommendation: `dBFS` — it is the sensible
convention for a digital plugin and requires no stateful calibration.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-001.

### QA-UX-002: Ratio parameter does not reach infinity:1 at top of travel

**Severity:** CRITICAL
**Owner:** `owner:dev-agent`
**Source:** qa-ux agent
**Context:** Parameter range is `[1.0, 60.0]`. `GainComputer` applies
`slope = 1/ratio − 1` uniformly. At ratio = 60 the slope is `−0.9833`,
not `−1.0` — a 10 dB excess produces 9.83 dB of GR. The "Brick Wall"
factory preset, which is named to imply brick-wall limiting, actually
delivers 59:1. The header docstring in `GainComputer.h:9-14` is
aspirational — it claims `60.0 = ∞:1` and `>60.0 = negative ratios`, but
neither is implemented and the parameter range caps at 60.0 so the
negative-ratio zone is unreachable from the UI.
**Expected:** REFERENCE.md §2.1 — ratio knob must reach ∞:1 (limiter
mode) at the top of travel.
**Actual:** Top of travel is 59:1; limiter mode is unreachable.
**Location:** `src/PluginProcessor.cpp:34-38`; `src/dsp/GainComputer.cpp:14-23`;
`src/dsp/GainComputer.h:9-13`; `src/FactoryPresets.h:24` ("Brick Wall"
preset).
**Fix direction:** In `GainComputer::computeGainReduction`, short-circuit
to `slope = -1.0f` when `ratio >= kInfinityRatioThreshold` (e.g. ≥ 50).
Optionally extend the range past 60 into the negative-ratio zone per
REFERENCE.md §2.1 (see QA-UX-009).
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-002.

### QA-CI-002: pluginval strictness 10 will fail CI on all platforms (linked to QA-CONF-001)

**Severity:** CRITICAL (gates every CI run after the extended workflow lands)
**Owner:** `owner:dev-agent`
**Source:** orchestrator (linked from qa-conformance)
**Context:** The extended `build-and-validate.yml` workflow runs
`pluginval --validate-in-process --strictness-level 10` on Linux, macOS
universal, and Windows. QA-CONF-001 (bool-parameter state restoration)
is reproducible under pluginval at every strictness level. Therefore the
first run of the extended workflow will be red on all three platforms
until QA-CONF-001 is fixed — not because of any new issue introduced by
the workflow bump, but because strictness 10 is unambiguous and the
old strictness-5 run was already hitting the same defect.
**Expected:** Green CI across the three-platform matrix at strictness 10
post-fix.
**Actual:** All three platforms will fail pluginval on QA-CONF-001 until
the bool state-restoration bug is resolved.
**Location:** `src/PluginProcessor.cpp:217-222` (the buggy
`setStateInformation`); `.github/workflows/build-and-validate.yml` (the
workflow that now fails loudly).
**Linked to:** QA-CONF-001 (parent defect), QA-TM-001 (the `softKnee`
parameter ID migration must land *with* the bool fix to avoid
double-breaking user state), QA-CI-001 (workflow not yet executed).
**Full finding:** `qa-findings/ci-status.md` → QA-CI-002.

---

## HIGH

### QA-DSP-001: ParameterSmoother re-triggers ramp every block → block-size-dependent output

**Severity:** HIGH
**Owner:** `owner:dev-agent`
**Source:** qa-dsp agent
**Context:** `MW160Processor::processBlock` calls
`compressor_[ch].setThreshold/setRatio/setOutputGain/setSoftKnee/setMix`
at the top of every block. `ParameterSmoother::setTarget` only
early-outs when `newTarget == target_` AND `countdown_ == 0`; while the
smoother is mid-ramp, an identical-target call re-enters the body and
recomputes `step_` from a fresh `stepsToRamp_` countdown. Every block
therefore restarts the 20 ms ramp, and small blocks never let the ramp
reach the target.
**Expected:** REFERENCE.md §1.2 — identical input at any block size must
produce identical output within float tolerance.
**Actual:** Measured up to 0.19 absolute output delta (≈1.7 dB) for
block sizes < 1024 samples at 48 kHz. Larger blocks mask the bug.
**Location:** `src/dsp/ParameterSmoother.h:36-55`; `src/PluginProcessor.cpp:141-148`.
**Fix direction:** Early-out unconditionally when `newTarget == target_`
(the smoother is already heading there), or track previous requested
target separately from the current ramp target.
**Evidence:** `qa-artifacts/probe_output.txt`, `qa-artifacts/mw160_probe.cpp`.
**Full finding:** `qa-findings/qa-dsp.md` → QA-DSP-001.

### QA-DSP-002: `RmsDetector` permanently poisoned by a single NaN or Inf input sample

**Severity:** HIGH
**Owner:** `owner:dev-agent`
**Source:** qa-dsp agent
**Context:** `RmsDetector::processSampleFromSquared` has no NaN guard.
A single NaN (or Inf, which squares to Inf and later to NaN) latches
`runningSquared_` to NaN forever. `Compressor::applyCompression` tests
`rmsLinear > 0`; NaN is not `> 0`, so the branch picks the silence floor
and the gain computer reports 0 dB GR. The audio path keeps running but
**compression is silently disabled** for the rest of the session.
**Repro:** After feeding a single NaN into a freshly settled detector,
then 1 second of 0 dBFS RMS sine (threshold −20, ratio 4:1, expected
~15 dB GR), metered GR is 0.000 dB and output RMS equals input RMS.
**Expected:** REFERENCE.md §1.5 — "must not introduce denormals, NaN,
or Inf anywhere in the signal path"; reasonable extension: isolated
NaN/Inf must not poison persistent state.
**Actual:** Single NaN → detector dead indefinitely, no recovery.
**Location:** `src/dsp/RmsDetector.cpp:21-25`; `src/dsp/Compressor.cpp:74-77`.
**Fix direction:** Clamp non-finite inputs to 0 before the EMA update,
or `if (!std::isfinite(...)) reset()` on the running state. Add a Catch2
case in `test_rms_detector.cpp` that injects NaN and verifies recovery
within ~3τ.
**Full finding:** `qa-findings/qa-dsp.md` → QA-DSP-002.

### QA-DSP-003 / QA-UX-003 / DESIGN-IMPL-001: No bypass parameter (three-angle finding)

**Severity:** HIGH
**Owner:** `owner:dev-agent`
**Source:** qa-dsp, qa-ux, design-impl agents (deduplicated)
**Context:** REFERENCE.md §2.2 and VISUAL_SPEC.md §2.4 / §7.4 both
require a bypass control. `MW160Processor::createParameterLayout`
declares no `bypass` parameter, `MW160Processor` does not override
`getBypassParameter()`, and the previous editor had no bypass button.
The design-impl agent drew the BYPASS pill per the visual spec but could
not wire it (the orchestrator explicitly forbade adding new parameters
in this pass), so the pill renders correctly but toggles only a local
visual state.
**Expected:** A `juce::AudioParameterBool` "bypass" surfaced through
`getBypassParameter()`, wired to the editor's BYPASS pill, with
sample-accurate crossfade at parameter-change time.
**Actual:** No parameter; button is visual-only; host bypass falls back
to host-level behaviour with no on-faceplate indication and possibly
non-sample-accurate crossfade.
**Location:** `src/PluginProcessor.cpp:22-65`; `src/PluginProcessor.h`;
`src/PluginEditor.cpp` (BYPASS button wiring); `src/PluginEditor.h`.
**Fix direction:** (1) Add a `bypass` APVTS bool parameter with default
`false`. (2) Override `getBypassParameter()` to return it. (3) In
`processBlock`, crossfade the wet path toward dry over 20 ms on change
using the existing `ParameterSmoother` (note interaction with QA-DSP-001
— fix that first or the crossfade will also be block-size dependent).
(4) Add a `ButtonAttachment` in `PluginEditor` for the BYPASS pill.
**Full findings:** `qa-findings/qa-dsp.md` → QA-DSP-003;
`qa-findings/qa-ux.md` → QA-UX-003; `qa-findings/design-impl.md` →
DESIGN-IMPL-001.

### QA-CONF-001: Bool parameter state restoration fails pluginval @ strictness 10

**Severity:** HIGH
**Owner:** `owner:dev-agent`
**Source:** qa-conformance agent
**Context:** pluginval's "Plugin state restoration" group fails on at
least one of `softKnee` / `stereoLink` across 3 random-seed runs
(seed `0x443e57`: "softKnee not restored, expected 0, got 0.163635";
similar failures on runs 2 and 3). Float parameters (`threshold`,
`ratio`, `outputGain`, `mix`) restore cleanly every time. The
`setStateInformation` implementation uses the canonical `apvts.replaceState`
path, but pluginval observes that the bool parameter's underlying atomic
is not overwritten by the restore. Likely causes: (a) pluginval writes
non-snapped raw floats via the VST3 parameter API, `flushParameterValuesToValueTree`
captures the unsnapped value into the state tree, and subsequent
`replaceState` doesn't resnap; (b) `AudioParameterBool::setValue` may
snap on input but not on read, leaving the underlying atomic with a
pre-restore value; (c) `approximatelyEqual` short-circuit in
`setDenormalisedValue` skips the actual write when the snapped new
value "isn't different enough" from the raw old one.
**Expected:** Lossless state round-trip for every declared parameter,
including bools.
**Actual:** Floats round-trip; bools fail intermittently (value
typically stuck at whatever random-mutation pluginval wrote most
recently, e.g., 0.163635 / 0.435694 / 0.572227).
**Location:** `src/PluginProcessor.cpp:217-222`.
**Fix direction:** Either (a) in `setStateInformation`, iterate the
parameter layout after `replaceState` and explicitly re-apply each
parameter's value via `setValueNotifyingHost`, or (b) parse the XML
directly and write each child node's value into the matching parameter.
Pair the fix with a new unit test that mimics pluginval's
set→capture→mutate→restore cycle on each bool parameter
(see QA-CONF-006 for the test harness).
**Evidence:** `qa-artifacts/pluginval_strict10.log`,
`qa-artifacts/pluginval_run3.log`.
**Full finding:** `qa-findings/qa-conformance.md` → QA-CONF-001.

### QA-UX-004 / DESIGN-REVIEW-006: Brand-adjacent user-facing parameter display name (soft knee)

**Severity:** HIGH
**Owner:** `owner:dev-agent`
**Source:** qa-ux agent, design-review agent (deduplicated)
**Context:** The parameter *display name* at `PluginProcessor.cpp:55`
was a brand-adjacent string. This is a separate field from the parameter
*ID* (`"softKnee"`, renamed per QA-TM-001 / VISUAL_SPEC.md §14.2). The
display name is what hosts render in their parameter automation list
(Ableton, Logic Smart Controls, Studio One control linking, Cubase
generic editor, etc.) — so even though the plugin's own editor now shows
KNEE/HARD/SOFT, a brand-adjacent display name would still expose the term
on the user-facing surface via the host. QA-TM-001 covers a repo-wide
scrub but is scoped to comments and DSP-layer references; this particular
one-line fix is worth calling out separately because it is zero-state-risk
(ID is unchanged) and directly fixes a user-visible brand leak.
**Expected:** Neutral display name such as `"Knee"` or `"Knee Shape"`.
**Actual:** A brand-adjacent string was rendered by every VST3 / AU host.
**Location:** `src/PluginProcessor.cpp:54-55`.
**Fix direction:** One-line change to the `AudioParameterBool`
constructor's display-name argument. Keep the `ParameterID{"softKnee", 1}`
intact. This can land *independently of* the full QA-TM-001 scrub and
does not require the state-migration shim.
**Full findings:** `qa-findings/qa-ux.md` → QA-UX-004;
`qa-findings/design-review.md` → DESIGN-REVIEW-006.

### QA-UX-006: Threshold-indicator LEDs inherit the dBFS/dBu unit mismatch

**Severity:** HIGH
**Owner:** `owner:dev-agent`
**Source:** qa-ux agent
**Context:** `PluginEditor.cpp:136-148` (`timerCallback` LED logic)
reads `threshold` raw from the APVTS and compares to
`displayedInputLevel_` raw — both currently in the dBFS frame, but
inconsistent with the `dBu` label on the threshold knob. Follow-on from
QA-UX-001. Until QA-UX-001 is resolved, the LEDs are numerically correct
but the user has no way to interpret them consistently with the
threshold knob. After QA-UX-001 resolves: if the convention becomes
dBFS, no change needed here; if dBu with calibration is preserved, the
LED comparator must apply the same shift.
**Expected:** LED comparator and threshold parameter in the same domain
as labeled.
**Actual:** Both in dBFS while threshold claims dBu.
**Location:** `src/PluginEditor.cpp:135-148`.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-006.

### QA-CI-001: build-and-validate workflow extended, actual run pending

**Severity:** HIGH
**Owner:** `owner:ci-agent`
**Source:** orchestrator
**Context:** The workflow has been extended to satisfy all
task requirements (3-platform matrix, pluginval strictness 10,
artifact upload, per-OS pluginval steps, macOS universal) but the
orchestrator cannot run GitHub Actions from the local environment, so
none of the three jobs have been executed end-to-end in a clean CI
environment. Next push to any branch will produce the first real
per-platform pass/fail matrix.
**Expected:** Three jobs attempt to complete; orchestrator has a
per-platform matrix for the QA report.
**Actual:** Workflow is committed but never run in this pass.
**Location:** `.github/workflows/build-and-validate.yml`.
**Next step:** Allow the next push/PR to run the workflow and triage
the results. Any platform that fails to build or fails pluginval for a
reason other than QA-CONF-001 is itself a CRITICAL backlog entry per
the task spec.
**Full finding:** `qa-findings/ci-status.md` → QA-CI-001.

### DESIGN-REVIEW-001: `cmake --build --target MW160` leaves Standalone/VST3 binaries stale

**Severity:** HIGH
**Owner:** `owner:ci-agent`
**Source:** design-review agent
**Context:** The target `MW160` is the shared-code static library only.
Running `cmake --build build --config Release --target MW160 --parallel`
reports success and `[100%] Built target MW160`, but does **not** relink
the per-format plugin binaries. The review agent rendered the stale
Standalone from 2026-04-07 against Xvfb and briefly misread it as an
impl regression until the mtime check surfaced the mismatch. The correct
subtargets are `MW160_Standalone`, `MW160_VST3`, and `MW160_AU`. The CI
workflow's unqualified `cmake --build build --config Release --parallel`
does build all targets (no `--target` filter), so CI is not affected,
but any developer or future automation that targets `MW160` alone will
silently use stale binaries.
**Expected:** Either the `MW160` target transitively depends on all
format subtargets, or the build documentation explicitly says which
subtargets are needed.
**Actual:** `--target MW160` is a silent sharp edge.
**Location:** `CMakeLists.txt`.
**Fix direction:** Add `add_custom_target(MW160_All ALL DEPENDS MW160_VST3
MW160_AU MW160_Standalone)` (or a similar umbrella), and document it in
the project README. Audit the CI workflow to confirm it does not hit
this trap — it currently does not because it omits `--target`.
**Full finding:** `qa-findings/design-review.md` → DESIGN-REVIEW-001.

---

## MEDIUM

### QA-DSP-004: Implementation is feedforward; REFERENCE.md §1.1 specifies feedback

**Severity:** MEDIUM
**Owner:** `owner:dev-agent` / `owner:docs-agent`
**Source:** qa-dsp agent
**Context:** The code is unambiguously feedforward. `Compressor.h:14-15`
even labels the pipeline as "feedforward." Measured GR at threshold −20,
ratio 4:1, 20 dB excess matches feedforward theory (−15.000 dB) to
within 0.02 dB and is 6.4 dB off the closed-form feedback expectation
(−8.571 dB).
**Expected:** REFERENCE.md §1.1 calls for feedback topology.
**Actual:** Pure feedforward. Program-dependent character comes from
hand-modelled ballistics, not loop equilibrium.
**Location:** `src/dsp/Compressor.h:14-15`, `src/dsp/Compressor.cpp:53-64`.
**Fix direction:** (a) re-architect the topology — feed the detector
from a one-sample-delayed copy of the post-processing signal; or
(b) update REFERENCE.md §1.1 to make the feedforward choice an explicit
documented exception per §5. Option (b) is much smaller and defensible
because the existing ballistics already emulate the program-dependent
character. Team decision needed.
**Full finding:** `qa-findings/qa-dsp.md` → QA-DSP-004.

### QA-DSP-007: No plugin-layer processBlock tests (block-size, automation, bypass)

**Severity:** MEDIUM
**Owner:** `owner:dev-agent`
**Source:** qa-dsp agent
**Context:** All existing Catch2 tests exercise the bare DSP via
`Compressor::processSample`. There is no test that constructs an
`MW160Processor`, calls `prepareToPlay`, and runs `processBlock` at
varying block sizes. This is the exact gap where QA-DSP-001 (block-size
dependence) and QA-DSP-003 (no bypass) live.
**Expected:** Catch2 case that instantiates `MW160Processor`, calls
`prepareToPlay(48000, blockSize)` for each of 32/64/128/256/512/1024,
pushes the same buffer through `processBlock`, and asserts identical
output within float tolerance. A second case toggling the future bypass
parameter and asserting no-click behaviour covers QA-DSP-003.
**Location:** gap — `tests/test_plugin_processor_block.cpp` to be created.
**Fix direction:** New test file linked against the JUCE plugin target.
Requires extending the existing `MW160Tests` CMake config, which
currently links only `MW160_DSP`.
**Full finding:** `qa-findings/qa-dsp.md` → QA-DSP-007.

### QA-CONF-002: `apvts.replaceState` doesn't snap bool params — parent of QA-CONF-001

**Severity:** MEDIUM
**Owner:** `owner:dev-agent`
**Source:** qa-conformance agent
**Context:** Underlying mechanism behind QA-CONF-001.
**Expected:** Lossless state restore for every declared parameter type.
**Actual:** Floats survive `replaceState`; bools don't.
**Location:** `src/PluginProcessor.cpp:217-222`.
**Fix direction:** See QA-CONF-001 fix. This entry is the
"architectural" framing of the same bug; resolving QA-CONF-001 resolves
this.
**Full finding:** `qa-findings/qa-conformance.md` → QA-CONF-002.

### QA-CONF-003: User preset load silently ignores wrong-tag XML

**Severity:** MEDIUM
**Owner:** `owner:dev-agent`
**Source:** qa-conformance agent
**Context:** `PresetManager::loadUserPreset` (lines 61-71) returns
silently on `parseXML == nullptr` or root-tag mismatch. Combo-box
selection in `PluginEditor::comboBoxChanged` does not check for failure
either. User has no signal that a preset is corrupt.
**Expected:** Fail loudly (AlertWindow) or fall back to a documented
safe default; revert combo selection if load fails.
**Actual:** Silent no-op. UI combo still shows the bad preset as
selected.
**Location:** `src/PresetManager.cpp:61-71`; `src/PluginEditor.cpp`
comboBoxChanged.
**Fix direction:** Return a `bool` from `loadUserPreset`; on `false`
show an AlertWindow and revert the combo selection to the previous
valid item.
**Full finding:** `qa-findings/qa-conformance.md` → QA-CONF-003.

### QA-CONF-004: Preset XML has no version field → silent compat break risk

**Severity:** MEDIUM
**Owner:** `owner:dev-agent`
**Source:** qa-conformance agent
**Context:** `getStateInformation` writes the bare APVTS state via
`copyXmlToBinary` with no plugin version stamp. `setStateInformation`
checks only the root tag name. No schema version field. Future parameter
layout changes (including the legacy-ID→`softKnee` rename that
QA-TM-001 performed) will silently drop parameters from old presets with
no diagnostic.
**Expected:** `<plugin version="1"/>` attribute on the root state tree;
restore checks the version and either migrates forward or warns.
**Actual:** No version field.
**Location:** `src/PluginProcessor.cpp:210-222`; `src/PresetManager.cpp:73-98`.
**Full finding:** `qa-findings/qa-conformance.md` → QA-CONF-004.

### QA-CONF-005: Parameter ID rename migration must land with QA-TM-001

**Severity:** MEDIUM
**Owner:** `owner:dev-agent`
**Source:** qa-conformance agent
**Context:** The state-restoration angle of the rename tracked under
QA-TM-001. APVTS keys preset XML by parameter ID. When QA-TM-001
renames the legacy parameter ID → `softKnee`, every existing user
preset will lose its soft-knee setting without a migration shim. Bumping
the `ParameterID` version number to 2 will *not* solve this — that JUCE
versioning is for VST3 parameter ID stability across hosts, not state
migration.
**Expected:** `setStateInformation` looks for the legacy ID in the
loaded XML and copies its value into the new `softKnee` ID before
calling `replaceState`. One-time shim, not a permanent path.
**Actual:** No migration shim at time of filing.
**Location:** `src/PluginProcessor.cpp:47-50` and 217-222.
**Hard dependency:** QA-TM-001 (the rename) and QA-CONF-005 (the shim)
MUST land in the same change.
**Full finding:** `qa-findings/qa-conformance.md` → QA-CONF-005.

### QA-UX-005: No tooltips or hover help on any control

**Severity:** MEDIUM
**Owner:** `owner:dev-agent`
**Source:** qa-ux agent
**Context:** First-time user unfamiliar with the reference hardware
class must guess what "Compression" means or what the soft-knee toggle
does. No `setTooltip` call anywhere in the codebase; no `TooltipWindow`
instance.
**Expected:** Short tooltip per control describing what it does and its
unit. JUCE provides `setTooltip()` on `Component` plus `TooltipWindow`.
**Actual:** None.
**Location:** `src/PluginEditor.cpp` constructor — every slider, toggle,
and preset control.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-005.

### QA-UX-007: Knob label / APVTS display name / parameter ID vocabulary inconsistent

**Severity:** MEDIUM
**Owner:** `owner:dev-agent`
**Source:** qa-ux agent
**Context:** Parameter ID is `"ratio"`, APVTS display name is
`"Compression"` (`PluginProcessor.cpp:35`), GUI knob label is
`"COMPRESSION"` (`PluginEditor.cpp:58`). Automation data references
`"ratio"`; host parameter list shows `"Compression"`. Ambiguous to a
beginner — "compression" could mean amount, ratio, or threshold.
**Expected:** Consistent vocabulary across surfaces. "Ratio" is the
universal vocabulary in this product class.
**Actual:** Mixed.
**Location:** `src/PluginProcessor.cpp:33-38`; `src/PluginEditor.cpp:58`.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-007.

### QA-UX-009: Negative-ratio (dynamics inversion) not exposed

**Severity:** MEDIUM
**Owner:** `owner:dev-agent`
**Source:** qa-ux agent
**Context:** REFERENCE.md §2.1 calls out negative ratios as a
signature-class capability that is "optional but within scope." Current
parameter range caps at 60.0 and `1/ratio - 1` asymptotes at -1.0, so
the code cannot produce actual negative slopes anyway. The header
comment at `GainComputer.h:9-14` is aspirational and inaccurate.
**Expected:** Either exposed (with the parameter range extended past
the infinity-point into a negative-ratio zone) or the aspirational
docstring removed.
**Actual:** Docstring claims support; parameter range and formula do
not provide it.
**Location:** `src/dsp/GainComputer.h:9-14`; `src/PluginProcessor.cpp:36`.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-009.

### QA-UX-010: User preset save lacks name validation, silent overwrites, ignored write failures

**Severity:** MEDIUM
**Owner:** `owner:dev-agent`
**Source:** qa-ux agent
**Context:** `PresetManager::saveUserPreset` concatenates the name with
`.xml` and passes it directly to `dir.getChildFile`. JUCE refuses path
separators but accepts `..` and reserved Windows names (`con`, `prn`,
etc.). No duplicate-name check — silently overwrites. `xml->writeTo(file)`
return value is discarded, so read-only-filesystem / disk-full /
permission-denied failures produce no user feedback.
**Expected:** Reject or sanitize invalid characters, confirm overwrite
on duplicate, report write failure to the user.
**Actual:** None of the above.
**Location:** `src/PresetManager.cpp:73-98`; `src/PluginEditor.cpp:189-213`.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-010.

### DESIGN-IMPL-002: Inter / JetBrains Mono font binaries not embedded

**Severity:** MEDIUM
**Owner:** `owner:design-agent`
**Source:** design-impl agent
**Context:** VISUAL_SPEC.md §4.1 prescribes Inter (Regular/Medium/Bold)
and JetBrains Mono embedded via `juce_add_binary_data`. The `.ttf` files
were not available in this pass. `assets/fonts/` is an empty directory.
`src/gui/Fonts.h` falls back to JUCE default sans + monospace per the
§4.1 fallback clause. CMake wiring (`juce_add_binary_data`) is not in
place.
**Expected:** Fonts embedded; Fonts helper calls
`Typeface::createSystemTypefaceFor` on BinaryData blobs.
**Actual:** Helper in place; fallback active.
**Location:** `src/gui/Fonts.h`; `assets/fonts/`; `CMakeLists.txt`.
**Note:** DESIGN-REVIEW-002 (wordmark/sublabel overlap) is downstream of
this — the overlap is more visible with the fallback bold than it would
be with Inter. Fix order: embed fonts first, then re-check the overlap.
**Full finding:** `qa-findings/design-impl.md` → DESIGN-IMPL-002.

### DESIGN-REVIEW-002: Header wordmark and sublabel vertically overlap

**Severity:** MEDIUM
**Owner:** `owner:design-agent`
**Source:** design-review agent
**Context:** In `PluginEditor.cpp:493-507`, wordmark `drawText` target
is `(64*sx, 6*sy, 360*sx, 34*sy)` — 34 px height with 28 pt bold. The
sublabel area immediately follows at `(64, 38, 360, 16)`. No vertical
gap; fallback bold's antialiased ink exceeds the nominal 34 px box and
the sublabel draws on top of its lower third. Pixel-measured ink bboxes:
wordmark y=15..48, sublabel y=30..48 — an 18 px overlap.
**Expected:** Clean stacking with an explicit gap.
**Actual:** "VCA COMPRESSOR" tangled with "mw160" baseline.
**Location:** `src/PluginEditor.cpp:493-507`.
**Fix direction:** Move the sublabel outside the wordmark area
(e.g., `(64, 44, 360, 14)`), or render both via a single `TextLayout`
with explicit baseline gap. Will be re-evaluated after Inter is embedded
(DESIGN-IMPL-002) but should still carry an explicit gap.
**Evidence:** `qa-artifacts/editor-900x360.png`.
**Full finding:** `qa-findings/design-review.md` → DESIGN-REVIEW-002.

---

## LOW

### QA-DSP-005: THD spectrum 3rd > 2nd at low GR (crossover ~-15 dB)

**Severity:** LOW
**Owner:** `owner:dev-agent`
**Source:** qa-dsp agent
**Context:** REFERENCE.md §1.5 calls for 2nd-harmonic-dominant THD
across the GR range. Measured at 3 dB GR: h2 at -78 dB, h3 at -68 dB
(3rd wins by ~10 dB). At 15 dB GR: h2 at -62, h3 at -65 (2nd wins). The
polynomial in `VcaSaturation::processSample` has a too-strong cubic
term at low `amount`.
**Location:** `src/dsp/VcaSaturation.cpp:18-20`.
**Fix direction:** Reduce the cubic coefficient, or condition it on
`amount²` instead of `amount` (see QA-DSP-008).
**Full finding:** `qa-findings/qa-dsp.md` → QA-DSP-005.

### QA-DSP-006: Output gain ramp is multiplicative (linear-in-dB), REFERENCE.md §1.7 calls for linear-in-amp

**Severity:** LOW
**Owner:** `owner:dev-agent` / `owner:docs-agent`
**Source:** qa-dsp agent
**Context:** `Compressor::prepare` uses
`ParameterSmoother<SmoothingType::Multiplicative>` for output gain,
which is linear-in-dB (exponential in amplitude). REFERENCE.md §1.7
prescribes linear-in-amp. Both are zipper-free and both are defensible
perceptually, but they differ in long ramps.
**Location:** `src/dsp/Compressor.h:59`.
**Fix direction:** Switch to `Linear` to match the reference, OR update
REFERENCE.md §1.7 to call out that linear-in-dB is preferred for
perceptual smoothness (the common modern choice). Team decision.
**Full finding:** `qa-findings/qa-dsp.md` → QA-DSP-006.

### QA-DSP-008: VcaSaturation cubic term does not separately scale with `amount`

**Severity:** LOW
**Owner:** `owner:dev-agent`
**Source:** qa-dsp agent
**Context:** Both quadratic and cubic terms in the saturation polynomial
are scaled by the same `amount`, so the 3rd-to-2nd-harmonic ratio is
nearly fixed. Closely related to QA-DSP-005.
**Location:** `src/dsp/VcaSaturation.cpp:18-20`.
**Fix direction:** Scale the cubic as `amount²` so the 3rd harmonic is
negligible at low GR.
**Full finding:** `qa-findings/qa-dsp.md` → QA-DSP-008.

### QA-CONF-006: `test_no_alloc` does not cover `MW160Processor::processBlock`

**Severity:** LOW
**Owner:** `owner:dev-agent`
**Source:** qa-conformance agent
**Context:** Current alloc test covers `Compressor::processSample` and
`processSampleLinked` only. The JUCE-side `processBlock` — which adds
`getRawParameterValue` reads, `getMagnitude` calls, `getWritePointer`,
atomic stores — is not in the allocation fence. By inspection it is
alloc-free, so this is a coverage gap, not an active bug.
**Location:** `tests/test_no_alloc.cpp`; `src/PluginProcessor.cpp:125-201`.
**Fix direction:** Add a Catch2 case that constructs a `MW160Processor`,
calls `prepareToPlay(48000, 512)`, runs ~100 blocks through
`processBlock` inside the `AllocationTracker`, and asserts zero
allocations. This also helps close QA-DSP-007.
**Full finding:** `qa-findings/qa-conformance.md` → QA-CONF-006.

### QA-CONF-007: Per-sample `std::exp`/`std::pow`/`std::log10` in audio thread

**Severity:** LOW
**Owner:** `owner:dev-agent`
**Source:** qa-conformance agent
**Context:** `Ballistics::processSample` calls `std::pow` and `std::exp`
on every attack-branch sample. `Compressor::applyCompression` calls
`std::log10` and `std::pow` every sample for dB↔linear. None allocate
or block — thread-safe, not a correctness issue. Performance concern
only; existing `test_performance.cpp` does not flag it out of budget.
**Location:** `src/dsp/Ballistics.cpp:30-33`;
`src/dsp/Compressor.cpp:75, 87`.
**Fix direction:** None required immediately. If future profiling shows
the compressor hot, replace with fast log/exp approximations or a
lookup table.
**Full finding:** `qa-findings/qa-conformance.md` → QA-CONF-007.

### QA-UX-008: Slider textbox width (72 px) may truncate `-40.0 dBFS`-style values

**Severity:** LOW
**Owner:** `owner:design-agent`
**Source:** qa-ux agent
**Context:** `PluginEditor.cpp:9-11` uses
`Slider::TextBoxBelow, false, 72, 18`. 72 px is tight for "`-40.0 dBFS`"
at the JUCE default font. Needs visual verification on the Standalone
build at default size.
**Location:** `src/PluginEditor.cpp:9-11`.
**Note:** The design-impl agent's full rewrite replaces the slider
textbox with dedicated value/unit label pairs, so this may already be
resolved by DESIGN-IMPL's pass. Verify before re-filing.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-008.

### QA-UX-011: Preset delete is silent no-op on factory preset selection

**Severity:** LOW
**Owner:** `owner:design-agent`
**Source:** qa-ux agent
**Context:** `onDeletePreset` early-returns on factory preset without
feedback. Clicking Delete on a factory preset does nothing visible.
**Expected:** Disable the button when a factory is selected (better),
or show a brief "Factory presets cannot be deleted" message.
**Actual:** Silent no-op.
**Location:** `src/PluginEditor.cpp:215-220`.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-011.

### QA-UX-012: Fresh-instance defaults are passthrough — bad first impression

**Severity:** LOW
**Owner:** `owner:dev-agent`
**Source:** qa-ux agent
**Context:** APVTS defaults (threshold=0, ratio=1, gain=0, knee=hard,
mix=100%, link=on) mean a new instance is unity gain — a new user
dropping the plugin on a track hears nothing. No automatic "default to
a useful preset" on instantiation.
**Expected:** Either default to a useful factory preset on construction,
or set the APVTS defaults to a sensible starting point.
**Actual:** Passthrough.
**Location:** `src/PluginProcessor.cpp:26-65`, 73-84.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-012.

### QA-UX-013: Factory preset coverage is drum-heavy; no vocal preset

**Severity:** LOW
**Owner:** `owner:design-agent`
**Source:** qa-ux agent
**Context:** Of 7 factory presets, 5 are drum-oriented plus Bass Control
and Gentle Leveling. No preset labeled for vocal use. "Gentle Leveling"
could serve but is not labeled as such.
**Location:** `src/FactoryPresets.h:18-24`.
**Fix direction:** Add "Vocal" and (if appropriate) "Bass DI" presets,
or re-purpose "Gentle Leveling" with a vocal-appropriate threshold.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-013.

### QA-UX-014: GR meter has no scale legend

**Severity:** LOW
**Owner:** `owner:design-agent`
**Source:** qa-ux agent
**Context:** `LedMeter` for GR runs 0..-40 dB but the GUI does not draw
any tick marks or numbers next to the meter. User cannot tell whether
the bar shows −3 or −30 dB of GR.
**Location:** `src/PluginEditor.cpp:309-331`; `src/gui/LedMeter.h:25-81`.
**Note:** DESIGN-IMPL has since added shared IN/OUT dB labels for the
ladder. Verify if GR now has its own scale or still needs one.
**Full finding:** `qa-findings/qa-ux.md` → QA-UX-014.

### QA-CI-003: `docs/BACKLOG.md:110` historical reference to `build.yml` is now stale

**Severity:** LOW
**Owner:** `owner:docs-agent`
**Source:** orchestrator
**Context:** The original MW160-4 ticket history references
`.github/workflows/build.yml` as the file MW160-4 created. Phase 4 of
this QA pass renamed it to `build-and-validate.yml`. A newly arrived
engineer reading MW160-4 in isolation may look for `build.yml` and not
find it.
**Location:** `docs/BACKLOG.md:110`.
**Fix direction:** Either leave the historical reference as-is (and add
a note that the file has since been renamed) or update inline. Minor.
**Full finding:** `qa-findings/ci-status.md` → QA-CI-003.

### DESIGN-IMPL-003: `snap1px` helper is written but not called

**Severity:** LOW
**Owner:** `owner:design-agent`
**Source:** design-impl agent
**Context:** VISUAL_SPEC.md §10.3 asks for 1 px hairline snapping via
`Geometry::snap1px(scale)`. Helper exists in `src/gui/Geometry.h`. Stroke
call sites pass raw floats. Only matters at non-integer host scale
factors (e.g., 1.25x); with integer scaling the strokes land on pixels
anyway.
**Location:** `src/gui/Geometry.h`; all `drawRoundedRectangle` /
`drawLine` call sites in `src/PluginEditor.cpp`.
**Full finding:** `qa-findings/design-impl.md` → DESIGN-IMPL-003.

### DESIGN-IMPL-004: Toggle and preset-button hit targets below WCAG 44 px minimum

**Severity:** LOW
**Owner:** `owner:design-agent`
**Source:** design-impl agent
**Context:** VISUAL_SPEC.md §11.1 asks for invisible padding children
expanding toggle hit zones to 140×44 and preset buttons to 64×36.
Currently toggles are 140×36 and Save/Delete are 64×24 — below WCAG 2.1
minimum on the short axis.
**Location:** `src/PluginEditor.cpp` `resized()`.
**Full finding:** `qa-findings/design-impl.md` → DESIGN-IMPL-004.

### DESIGN-IMPL-005: VISUAL_SPEC.md §5.2 rotary-travel prose is self-contradictory

**Severity:** LOW
**Owner:** `owner:docs-agent`
**Source:** design-impl agent
**Context:** VISUAL_SPEC.md §5.2 states "Travel angle: 270 deg total.
Start angle = 7π/6, end angle = π/3 + 2π = 7π/3" — the literal formulae
give 210°, not 270°. The impl implements the stated *intent* (270° via
JUCE's canonical 1.25π / 2.75π) which is correct. The spec prose needs
reconciliation.
**Location:** `docs/VISUAL_SPEC.md` §5.2.
**Fix direction:** Update the prose to match the 270° intent — replace
the formula with `1.25π` (startAngle) and `2.75π` (endAngle).
**Full finding:** `qa-findings/design-impl.md` → DESIGN-IMPL-005.

### DESIGN-IMPL-006: Meter-mode selector highlights only top labels, not tick columns

**Severity:** LOW
**Owner:** `owner:design-agent`
**Source:** design-impl agent
**Context:** VISUAL_SPEC.md §6.6 says the 3-state selector adjusts the
brightness of both the label and the tick column of the active ladder.
Impl handles labels via `onModeChange` but tick columns are drawn
unconditionally in `textDim` from `paint()`.
**Location:** `src/PluginEditor.cpp` paint() meter tick section.
**Fix direction:** Store current meter mode on the editor and branch on
it in the tick paint.
**Full finding:** `qa-findings/design-impl.md` → DESIGN-IMPL-006.

### DESIGN-REVIEW-003: IN/OUT shared tick column is 8 px wide — too narrow for "-48"

**Severity:** LOW
**Owner:** `owner:design-agent`
**Source:** design-review agent
**Context:** Impl sets `shared_w = 8.0f * sx` at `shared_x = 54.0f * sx`
for the shared IN/OUT dB legend. 8 px is narrower than 3-glyph labels
like `"-48"` at 8 pt. Centered text clips or overflows into adjacent
ladders once Inter is embedded.
**Location:** `src/PluginEditor.cpp:601-607`.
**Fix direction:** Widen `shared_w` to 14–16 px (fits inside the
~18 px gap between IN right edge and OUT left edge at 900 px reference).
**Evidence:** `qa-artifacts/editor-900x360.png`.
**Full finding:** `qa-findings/design-review.md` → DESIGN-REVIEW-003.

### DESIGN-REVIEW-004: STEREO LINK / BYPASS labels drift from VISUAL_SPEC.md §7.4

**Severity:** LOW
**Owner:** `owner:design-agent` / `owner:docs-agent`
**Source:** design-review agent
**Context:** Spec table §7.4 prescribes `OFF`/`ON` for all three
toggles. Impl deviates on STEREO LINK (`LINK OFF`/`LINK ON`) and BYPASS
(`ACTIVE`/`BYPASS`) for readability when two pills sit side-by-side.
Impl flagged this as Deviation 1 in its own report.
**Expected:** Spec compliance OR a spec update that endorses the impl's
readability argument.
**Actual:** Spec and impl disagree.
**Location:** `src/PluginEditor.cpp:185-196`; `docs/VISUAL_SPEC.md` §7.4.
**Fix direction:** Team decides. Either revert impl to `OFF`/`ON` or
update spec to endorse the longer-form labels.
**Full finding:** `qa-findings/design-review.md` → DESIGN-REVIEW-004.

### DESIGN-REVIEW-005: Knob unit captions use `textDim` at 9 pt — violates §11.2 contrast rule

**Severity:** LOW
**Owner:** `owner:design-agent`
**Source:** design-review agent
**Context:** VISUAL_SPEC.md §11.2: "`textDim` is only allowed for text
11 pt and larger in this spec; the impl agent must use `textMid` for any
9 pt body text on dark backgrounds." Impl's `setupUnitLabel`
(`PluginEditor.cpp:72-80`) sets 9 pt in `textDim`, contrast ratio 3.8:1
vs required 4.5:1 for WCAG 2.1 AA body text. The `PRESET` label in the
footer is correctly in `textMid` at 9 pt — so the bug is specifically
in the unit captions.
**Location:** `src/PluginEditor.cpp:72-80`.
**Fix direction:** One-line change — `textDim` → `textMid` in
`setupUnitLabel`.
**Full finding:** `qa-findings/design-review.md` → DESIGN-REVIEW-005.

### DESIGN-REVIEW-007: Knob metallic gradient reads near-flat (observation, not a drift)

**Severity:** LOW (informational)
**Owner:** `owner:design-agent` (for a future spec tuning pass)
**Source:** design-review agent
**Context:** Knob body gradient `#6c6c6c → #2c2c2c` against
`surfaceLow #222222` reads as mostly uniform dark disc; brushed-metal
signal per VISUAL_SPEC.md §5.1 is weak. Impl is faithful to the exact
hex values — the issue is that the palette undershoots the "milled
aluminum" intent.
**Location:** `src/gui/Palette.h:35-36`; `docs/VISUAL_SPEC.md` §3 role
table.
**Fix direction:** Future spec revision — widen the gradient contrast
(e.g., `knobMetalHigh` → `#858585`). No editor change needed.
**Full finding:** `qa-findings/design-review.md` → DESIGN-REVIEW-007.

---

## INFO

### QA-DSP-009: Brand identifiers in DSP code (cross-reference only)

**Severity:** INFO
**Owner:** n/a (tracked under QA-TM-001)
**Source:** qa-dsp agent
**Context:** Listing so the dev-agent who takes QA-DSP-004 (topology) or
QA-DSP-002 (NaN guard) sees the overlapping files and can address naming
and topology together:
- `src/dsp/Compressor.{h,cpp}` — brand name in header comment;
  `setSoftKnee`, `kSoftKneeWidth_dB`.
- `src/dsp/RmsDetector.h:7-9` — brand name.
- `src/dsp/Ballistics.h:7,38` and `src/dsp/Ballistics.cpp:29` — brand
  name, published-spec references.
- `src/dsp/VcaSaturation.{h,cpp}` — brand of VCA, 3rd-party
  measurement source.
- `src/dsp/GainComputer.h:5-17` — brand + feature.
- `src/PluginProcessor.cpp:48-50` — `softKnee` parameter ID and display
  name.
**No action requested** from QA-DSP. Fully covered by QA-TM-001.

---


