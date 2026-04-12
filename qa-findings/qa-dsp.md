# QA-DSP findings (2026-04-11)

## Summary

Core sample-rate-by-sample DSP (RmsDetector, Ballistics, GainComputer, soft
knee, sustained-tone GR curves) is in good shape and matches REFERENCE.md
within tolerance. However, validation surfaced four real defects: (1) the
plugin has no bypass parameter at all, (2) `ParameterSmoother::setTarget`
restarts the smoothing ramp every block, producing block-size-dependent
output during the first ~20 ms of any setting and during automation, (3) a
single NaN/Inf input permanently dead-keys the RMS detector (audio path
keeps running but compression is silently disabled), and (4) the
implementation is feedforward whereas REFERENCE.md §1.1 specifies
feedback. Existing 110-test Catch2 suite passes; pluginval at strictness
10 succeeds.

## Pass/fail matrix

| Check | Status | Notes |
|-------|--------|-------|
| Static GR curve (sine, 2/4/8/∞:1) | PASS | All ratios within ±0.02 dB of theoretical, full chain. See `qa-artifacts/probe_output.txt`. |
| Knee shape — hard knee | PASS (with caveat) | Discontinuous first derivative at threshold (0 → -0.75) is intrinsic to a hard knee; correctly behaved. |
| Knee shape — soft knee | PASS | Quadratic interpolation continuous in value and first derivative at both knee boundaries (within finite-difference precision). Width 10 dB, inside the 6–12 dB envelope per §1.4. |
| Attack timing (10/20/30 dB to 90%) | PASS | Measured 15.6/5.3/2.8 ms vs spec 15/5/3 ms at 44.1 kHz; within ±10%. Identical at 48/96/192 kHz (sample-rate invariant). |
| Release rate (5–50 dB GR range) | PASS | Constant 125.0 dB/s at every depth and at 44.1/48/96/192 kHz. Linear-in-dB. |
| Program-dependent attack | PASS | Strictly monotonic in transient magnitude; verified at multiple sample rates. |
| **Block-size independence** | **FAIL** | `ParameterSmoother::setTarget` re-initialises the ramp on every block, even when the target is unchanged. Output deviates by up to ~1.7 dB at small block sizes for the first ~20 ms; large blocks (≥ 1024 samples) hide the bug. See QA-DSP-001. |
| Sample-rate independence | PASS | Detector tau and release rate are exactly invariant at 44.1/48/96/192 kHz. |
| **NaN / Inf safety** | **FAIL** | Detector is permanently poisoned by a single NaN/Inf sample; audio path continues but compression is silently disabled. See QA-DSP-002. |
| Denormals / DC / subsonic | PASS | No bad values; DC and 10 Hz tones produce sane output. |
| Zipper noise on automation | PASS | Max sample-to-sample step under sweep ≈ 0.07 (driven by 1 kHz sine itself, not zipper); no NaN/Inf. |
| Stereo link semantics | PASS | Linked-mode L-only signal yields identical GR on both channels (`processSampleLinked` API). The plugin processBlock wires this correctly. |
| **Bypass sample-accurate** | **FAIL** | No bypass parameter exists in `MW160Processor`. `getBypassParameter()` is not overridden. See QA-DSP-003. |
| No heap allocation in audio path | PASS | Three existing `test_no_alloc` Catch2 cases cover `processSample`, `processSampleLinked`, and parameter changes; all green. |
| THD ≈ 0.1–0.2% at moderate GR | PASS | 0.098% at -15 dB GR (4:1, 100 Hz fund); 0.098% at -17 dB GR (8:1). |
| **THD spectrum 2nd > 3rd** | **FAIL at low GR** | At -3 dB GR, 3rd harmonic is **9.8 dB louder than 2nd** (h2 = -78 dB, h3 = -68 dB). At higher GR (≥-15 dB) the 2nd does dominate. See QA-DSP-005. |
| **Topology: feedback** | **FAIL** | Code is unambiguously feedforward. Measured GR matches feedforward theory to within 0.02 dB and is 6.4 dB off the closed-form feedback expectation. See QA-DSP-004. |
| Output gain ramp shape | DIVERGE | Smoothed multiplicatively (linear in dB) rather than the linear-in-amp ramp called out in §1.7. Inaudible as zipper, but a documented divergence. See QA-DSP-006. |

## Findings

### QA-DSP-001: ParameterSmoother re-triggers ramp every block, causing block-size-dependent output

**Severity:** HIGH
**Owner:** owner:dev-agent
**Context / repro:** `MW160Processor::processBlock` calls
`compressor_[ch].setThreshold(...)`, `setRatio(...)`, `setOutputGain(...)`,
`setSoftKnee(...)`, `setMix(...)` once at the top of every block, regardless
of whether the value has changed. `ParameterSmoother::setTarget` only
early-outs when `newTarget == target_` **and** `countdown_ == 0`. While the
smoother is mid-ramp (`countdown_ > 0`), an identical-target call still
re-enters the body and recomputes `step_` based on a fresh `stepsToRamp_`
countdown. The effect: every block restarts the ramp from `current_` with
the full 20 ms duration. Small blocks therefore never let the ramp reach
the target.

I reproduced this with a probe (`/home/mwoolly/projects/mw160/qa-artifacts/mw160_probe.cpp`,
function `probe_block_size_independence_chunked`) running 1 second of 1 kHz
sine through `Compressor::processSample`, chunked into varying block sizes
with the per-block setter pattern from `processBlock`. Reference is a
single huge block:

```
chunked block=32     max |out-ref|=1.919e-01 at sample 1116
chunked block=64     max |out-ref|=1.895e-01 at sample 1092
chunked block=128    max |out-ref|=1.842e-01 at sample 1092
chunked block=256    max |out-ref|=1.711e-01 at sample 1068
chunked block=512    max |out-ref|=1.356e-01 at sample 1020
chunked block=1024   max |out-ref|=0.000e+00
chunked block=2048   max |out-ref|=0.000e+00
```

A 0.19 absolute output delta on a 0.5-amplitude sine is roughly 1.7 dB of
extra attenuation that appears at small block sizes only.

**Expected:** REFERENCE.md §1.2: "Detector state must persist across
processBlock boundaries. Any detector whose state is implicitly tied to
block layout (e.g., reset or smoothed per-block instead of per-sample) will
produce block-size dependent output and is incorrect." Same principle
applies to parameter smoothing — identical input at any block size must
produce identical output to within float tolerance.

**Actual:** Output diverges by up to 0.19 (≈1.7 dB) for block sizes
< 1024 samples at 48 kHz (i.e. < 21 ms, the ramp duration). Larger blocks
mask the bug.

**Location:**
- `src/dsp/ParameterSmoother.h:36-55` — `setTarget()` early-out only when
  countdown is 0 *and* target unchanged.
- `src/PluginProcessor.cpp:141-148` — block setter pattern that triggers
  the bug.

**Fix sketch:** Either (a) early-out unconditionally when
`newTarget == target_` (the smoother is already heading there), or (b)
track the *previous* requested target separately from the current ramp
target so that an unchanged request is a no-op. Option (a) is the smaller
change; verify ParameterSmoother test still passes after.

**Evidence:** `qa-artifacts/probe_output.txt` (block-size sections),
`qa-artifacts/mw160_probe.cpp`.

---

### QA-DSP-002: RmsDetector permanently poisoned by a single NaN or Inf input sample

**Severity:** HIGH
**Owner:** owner:dev-agent
**Context / repro:** `RmsDetector::processSampleFromSquared` does
`runningSquared_ = coeff_ * runningSquared_ + (1 - coeff_) * inputSquared`
with no NaN guard. A single NaN (or Inf, which squares to Inf and then to
NaN under subsequent arithmetic) latches `runningSquared_` to NaN forever.
`Compressor::applyCompression` then computes `rms_dB = (rmsLinear > 0) ?
20*log10(rmsLinear) : -100`. NaN is not `> 0`, so the branch picks the
silence floor and the gain computer reports 0 dB GR. The audio path keeps
running, but **all compression is silently disabled** for the rest of the
session.

Probe (`probe_denormals_nan` in `mw160_probe.cpp`):

```
NaN injected at idx 100: output NaN/Inf count = 1 / 9600
NaN-recovery test: input RMS = 1.000 (-0.0 dB), output RMS = 1.000 (-0.0 dB)
metered GR after recovery = 0.000 dB
```

After feeding a single NaN sample into a freshly settled detector, then
feeding 1 second of 0 dBFS RMS sine (which should produce ~15 dB GR with
threshold -20, ratio 4:1), the metered GR is exactly 0.000 dB and the
output RMS equals the input RMS. The compressor is dead.

The defect is silent — no asserts, no DAW indication. A daw with a
glitched track or a momentary plugin chain misconfiguration upstream would
disable this plugin's compression for the entire session.

**Expected:** REFERENCE.md §1.5: "must not introduce denormals, NaN, or Inf
anywhere in the signal path." A reasonable interpretation extends to:
isolated NaN/Inf input must not poison persistent state.

**Actual:** Single NaN/Inf input → detector dead, compression bypassed,
indefinitely.

**Location:**
- `src/dsp/RmsDetector.cpp:21-25` — no NaN/Inf guard on
  `runningSquared_`.
- `src/dsp/Compressor.cpp:74-77` — branch on `rmsLinear > 0` silently
  swallows NaN as "silence" instead of clamping/recovering.

**Fix sketch:** In `RmsDetector::processSampleFromSquared`, clamp non-finite
inputs to 0 before the EMA update, or use `if (!std::isfinite(...)) reset()`
on the running state. Add a Catch2 case under `test_rms_detector.cpp` that
injects NaN and verifies the detector recovers within ~3τ. Add similar
coverage to `test_compressor_integration.cpp`.

**Evidence:** `qa-artifacts/probe_output.txt` (Denormals/NaN section).

---

### QA-DSP-003: Plugin has no bypass parameter at all

**Severity:** HIGH
**Owner:** owner:dev-agent
**Context / repro:** REFERENCE.md §2.2 lists Bypass as a required control:
"sample-accurate, with no latency discrepancy between bypassed and engaged
states. Bypass must not produce a click." `MW160Processor::createParameterLayout`
declares only six parameters: `threshold`, `ratio`, `outputGain`, `softKnee`,
`stereoLink`, `mix`. There is no bypass parameter and no
`getBypassParameter()` override on `MW160Processor`. Most DAWs use a
plugin's bypass parameter to coordinate sample-accurate bypass with their
own automation; with none declared, host-level bypass simply stops calling
processBlock (no crossfade, possible click).

**Expected:** A `juce::AudioParameterBool` "bypass" surfaced through
`getBypassParameter()` so the host can drive sample-accurate bypass,
plus internal handling that crossfades at parameter-change time (e.g. via
the existing parameter smoother).

**Actual:** No parameter exists. Host bypass falls back to host-level
behaviour, not the plugin-controlled sample-accurate path described in
the reference. There is also no UI bypass control on the faceplate per
the GUI source.

**Location:**
- `src/PluginProcessor.cpp:22-65` — `createParameterLayout()` (no bypass).
- `src/PluginProcessor.h` — no `getBypassParameter()` override.

**Fix sketch:** Add a bool parameter `bypass`, override
`AudioProcessor::getBypassParameter()` to return it, and in `processBlock`
either skip the DSP entirely (the trivial path) or — for sample-accurate
no-click behaviour — crossfade the wet output toward the dry input over
20 ms whenever the parameter changes. Also wire it into the GUI per the
reference §2.2.

---

### QA-DSP-004: Topology is feedforward, REFERENCE.md §1.1 specifies feedback

**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context / repro:** `Compressor::applyCompression` reads `rmsLinear` from
the detector that was fed the pre-VCA `input` (`detector_.processSample(input)`),
computes a gain reduction, and applies it. There is no path from the
post-VCA, post-saturation, post-output-gain signal back to the detector.
This is a textbook feedforward topology. The header comment on
`Compressor.h:14-15` even labels it "feedforward compression pipeline."

I quantified the topology directly. With threshold -20, ratio 4:1, 20 dB
excess sine input, full chain at 48 kHz:

```
measured GR (sr=48000): -14.988 dB
expected if feedforward: -15.000 dB  (delta +0.012 dB)
expected if feedback:    -8.571 dB   (delta -6.417 dB)
```

The closed-form feedback solution (solving for the loop equilibrium) is
`GR_FB = excess · (1/R - 1) / (2 - 1/R) = -8.571 dB` for these settings.
The measurement matches feedforward to better than 0.02 dB and is 6.4 dB
off the feedback expectation.

**Expected:** REFERENCE.md §1.1: "The target behavior is a **feedback**
compressor, not feedforward. In a feedback topology, the level detector
reads the compressor's own output (post-VCA) and steers the gain-control
element from there." Listed consequences include the inherent soft curve
near threshold, naturally program-dependent behaviour, and the gentler
measured static curve. The current implementation produces a textbook-sharp
feedforward static curve.

**Actual:** Pure feedforward; the program-dependent attack character is
hand-modelled in `Ballistics::processSample` rather than emerging from
loop equilibrium dynamics.

**Location:**
- `src/dsp/Compressor.h:14-15` — header comment honestly labels the
  pipeline as feedforward.
- `src/dsp/Compressor.cpp:53-64` — the detector is fed `input`, not the
  post-processing signal.

**Fix sketch:** Either (a) change the topology — feed the detector from a
one-sample-delayed copy of the *output* of `applyCompression` and move
the gain-application to the next sample so the loop can be evaluated
in-order; or (b) update REFERENCE.md to make the feedforward choice an
explicit, documented exception per §5 ("If any decision in the plugin
diverges from this document, the divergence belongs here as a documented
exception with justification, not silently in the code"). Option (b) is
much smaller and is defensible because the existing ballistics already
emulate the program-dependent character; option (a) is the only way to
get a true feedback static curve.

**Evidence:** `qa-artifacts/probe_output.txt` (Topology section).

---

### QA-DSP-005: THD spectrum at low GR is 3rd-harmonic dominant, not 2nd

**Severity:** LOW
**Owner:** owner:dev-agent
**Context / repro:** `VcaSaturation::processSample` computes
`return input + amount * (x2 * 0.05f - input * x2 * 0.02f)` where
`x2 = input*input`. The second term `-amount * 0.02 * input^3` is a
cubic — it produces a 3rd harmonic as well as a small fundamental
contribution. At low GR, `amount` is small enough that the constant
`0.05*x^2` term is dominated by signal-dependent cubic terms after the
RMS smoothing, and the *measured* 3rd harmonic exceeds the 2nd by ~10 dB.

Probe at threshold -20 dB, ratio 2:1, 6 dB excess (~3 dB GR), 100 Hz sine
through full chain:

```
2:1 6dB excess (~3 dB GR)
  metered GR = -2.93 dB
  h2 = 2.470696e-05  (-78.2 dB rel fund)
  h3 = 7.638335e-05  (-68.4 dB rel fund)
  THD ~ 0.0418%   (h2 > h3? NO)
```

At higher GR (4:1 / 8:1, ~15-17 dB GR) the 2nd does become dominant, so
the saturation polynomial does eventually behave as designed:

```
4:1 20dB excess (~15 dB GR)   h2=-62.0 dB  h3=-65.3 dB  THD=0.098%  h2>h3? yes
8:1 20dB excess (~17.5 dB GR) h2=-63.2 dB  h3=-63.8 dB  THD=0.098%  h2>h3? yes
```

The crossover is somewhere between -3 and -15 dB GR. The reference
language ("even-order dominant", "2nd > 3rd") implies this should hold
across the GR range, not only at heavy compression.

**Expected:** REFERENCE.md §1.5: "even-order dominant (2nd harmonic
stronger than 3rd), reflecting the asymmetric transfer of a typical
analog gain-control element of this lineage."

**Actual:** At light GR, 3rd > 2nd by ~10 dB. The 8:1 / -17 dB GR case is
also marginal: h2 only 0.6 dB above h3.

**Location:** `src/dsp/VcaSaturation.cpp:18-20`. The cubic term is too
strong relative to the quadratic; reduce the cubic coefficient or
condition both on `amount` separately so the 2nd-harmonic dominance holds
across the range.

**Evidence:** `qa-artifacts/probe_output.txt` (THD section).

---

### QA-DSP-006: Output gain smoothing is multiplicative (linear-in-dB), reference calls for linear-in-amp

**Severity:** LOW
**Owner:** owner:dev-agent
**Context / repro:** `Compressor::prepare` initialises
`outputGainSmoother_` as `ParameterSmoother<SmoothingType::Multiplicative>`
which advances `current_ *= factor_` per sample (constant ratio per step,
i.e. linear-in-dB). REFERENCE.md §1.7 explicitly says "smoothed per-sample
(20 ms linear ramp)". Captured the ramp shape from the silence path:

```
12 dB output-gain ramp: 960 samples (20 ms @ 48 kHz)
first 8 samples: 1.0014 1.0029 1.0043 1.0058 1.0072 1.0087 1.0101 1.0116
last 4 samples:  3.9640 3.9697 3.9754 3.9811
ideal final:     3.9811
```

A linear-in-amplitude ramp from 1.0 to 3.981 over 960 samples would step
0.00310 per sample throughout. The actual first step is 0.00145 and the
last step is 0.0057 — the curve is exponential, as expected for a
multiplicative smoother. Both shapes are zipper-free, but they differ
audibly in the perceived loudness curve over a long ramp (the
multiplicative ramp sounds like a uniform fade in dB).

**Expected:** REFERENCE.md §1.7: "Output gain should be smoothed
per-sample (20 ms linear ramp)."

**Actual:** Per-sample, but a logarithmic (multiplicative) ramp.

**Location:** `src/dsp/Compressor.h:59` declares
`outputGainSmoother_` as `Multiplicative`. Switch to `Linear` to match the
reference, OR update §1.7 to call out that linear-in-dB is preferred for
perceptual smoothness (which is the more common modern choice).

**Evidence:** `qa-artifacts/probe_output.txt` (Output gain ramp section).

---

### QA-DSP-007: No tests cover the JUCE plugin processBlock layer (block-size, automation, bypass)

**Severity:** MEDIUM
**Owner:** owner:dev-agent
**Context / repro:** Every existing Catch2 test exercises the bare DSP
through `mw160::Compressor::processSample` / `processSampleLinked`.
There is no test that constructs an `MW160Processor`, calls
`prepareToPlay`, and then `processBlock` at varying block sizes with
varying automation — which is precisely the layer where QA-DSP-001
(block-size dependence) and QA-DSP-003 (no bypass) live.

**Expected:** At least one Catch2 case that:
1. Instantiates `MW160Processor`
2. Calls `prepareToPlay(48000.0, blockSize)` for each of 32/64/128/256/512/1024
3. Pushes the same float buffer through `processBlock` for each
4. Asserts identical output to within float tolerance

This would have caught QA-DSP-001 immediately. A second case that toggles
the future bypass parameter and asserts no-click behaviour would cover
QA-DSP-003.

**Location:** Gap; no file exists yet.

**Fix sketch:** Add `tests/test_plugin_processor_block.cpp` that links
against the JUCE plugin target. The CMakeLists already builds
`MW160Tests` separately from the plugin so this requires additional
linkage configuration.

---

### QA-DSP-008: VcaSaturation 3rd-harmonic component does not scale with `amount`

**Severity:** LOW
**Owner:** owner:dev-agent
**Context / repro:** Looking again at the saturator:

```cpp
const float x2 = input * input;
return input + amount * (x2 * 0.05f - input * x2 * 0.02f);
```

Both the quadratic and cubic terms are scaled by the same `amount`, which
itself scales with `|GR|`. At low GR `amount` is small, so the input
itself dominates. But the *ratio* of cubic to quadratic in the polynomial
is fixed at 2/5 = 0.4 of the input magnitude. For the unit-amplitude sine
my probe used, the cubic term outputs roughly 0.4× the quadratic in peak
amplitude — but harmonic *spectrum* contributions go like `(0.02·|x|³)/4`
for the 3rd harmonic versus `0.05·|x|²/2` for the 2nd, with the relative
amplitude `(0.02/0.05)·(|x|/2) = 0.4·(|x|/2) = 0.2·|x|`. So 3rd grows with
input level. At small input levels (small `x`) the 3rd should be weaker
than the 2nd, and at large levels the 2nd should win. The measurements
contradict this at low GR — likely because the smoothed RMS-level path
runs the saturator on a very small instantaneous signal during the recovery
phase, where rounding-noise / numerical floor dominates.

This is closely related to QA-DSP-005 but is an independent observation:
the `amount` scaler factors out the GR dependence but does not separately
condition the 2nd vs 3rd harmonic balance.

**Expected:** A clean 2nd-harmonic-dominant signature across the GR
range, per §1.5.

**Actual:** Crossover behaviour as documented in QA-DSP-005.

**Location:** `src/dsp/VcaSaturation.cpp:18-20`.

**Fix sketch:** Decouple the two coefficients — for example,
`return input + amount * 0.05 * x2 - amount * amount * 0.02 * input * x2`
so the 3rd-harmonic term grows as `amount²`, ensuring it's negligible at
low GR.

---

### QA-DSP-009: Brand-related identifiers in DSP code (informational, covered by QA-TM-001)

**Severity:** INFO
**Owner:** owner:tm-agent
**Context / repro:** Per the briefing, QA-TM-001 already covers a global
trademark scrub. Listing here only because the topology gap (QA-DSP-004)
references the same files and a fix to one will touch the other:

- `src/dsp/Compressor.h:14-15` — comment names a brand and model.
- `src/dsp/Compressor.cpp` — no direct brand strings, but `setSoftKnee`
  and `kSoftKneeWidth_dB` use a trademarked feature name.
- `src/dsp/RmsDetector.h:7-9` — comment names a brand and model.
- `src/dsp/Ballistics.h:7,38` and `src/dsp/Ballistics.cpp:29` — comments
  reference a brand and model, and named published specs.
- `src/dsp/VcaSaturation.h` and `.cpp` — comments name a brand of VCA and
  a third-party measurement source by name.
- `src/dsp/GainComputer.h:5-17` — comment names a brand and a feature.
- `src/PluginProcessor.cpp:48-50` — `softKnee` parameter ID and display
  name.

No action requested from QA-DSP; cross-referenced for awareness so the fix
agent can address topology and naming together.

## Notes on what's clean

Worth saying explicitly: the things I checked carefully and that look
good are:

- **RMS detector convergence behaviour** (sine → 1/√2, DC → amplitude,
  ~20 ms tau across 44.1/48/96/192 kHz) is exact within float precision.
- **Ballistics attack and release timing** match the §1.3 targets at
  every sample rate I tested (44.1 / 48 / 96 / 192 kHz), including the
  program-dependent monotonicity.
- **Static GR curve** matches feedforward theory to within ±0.02 dB
  across 5 excess values × 4 ratios — the small bias is the VCA
  saturation imprint, well within tolerance.
- **Soft-knee curve** is C¹ at both knee boundaries (verified by
  finite-difference probe) and the implementation is the textbook
  Giannoulis et al. quadratic.
- **Stereo link semantics** work correctly when the host wires
  `processSampleLinked` per `MW160Processor::processBlock`.
- **Heap allocation in audio path**: zero, across three existing
  catch2 cases including parameter automation.
- **pluginval at strictness level 10** validates cleanly against the
  built VST3 (44.1 / 48 / 96 kHz × block sizes 64/128/256/512/1024,
  including parameter fuzzing and thread-safety checks).
- **Aliasing**: existing tests already verify aliased harmonic content
  is below -80 dBFS in the full chain at 44.1 kHz with worst-case
  fundamentals.

## Artifacts

- `qa-artifacts/probe_output.txt` — full text output of the standalone
  probe, all 12 sections.
- `qa-artifacts/mw160_probe.cpp` — source of the standalone probe; can be
  re-run by compiling against `build/libMW160_DSP.a`. Compile command:
  `g++ -std=c++17 -O2 -I/home/mwoolly/projects/mw160/src qa-artifacts/mw160_probe.cpp build/libMW160_DSP.a -o /tmp/mw160_probe`.
