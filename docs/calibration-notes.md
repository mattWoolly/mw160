# MW160 Timing Calibration Notes

## Date: 2026-04-06
## Ticket: MW160-17

## Reference Specs (Published)

**Attack (program-dependent):**
| GR Depth | Published Time |
|----------|---------------|
| 10 dB    | 15 ms         |
| 20 dB    | 5 ms          |
| 30 dB    | 3 ms          |

**Release:** ~125 dB/sec constant rate

## Model

The ballistics model uses a one-pole IIR lowpass filter with a **program-dependent time constant** for attack and a **constant-rate linear ramp** for release.

### Attack

```
magnitude = |target_GR - current_GR|
attackTime = kBaseAttackTime / max(1.0, magnitude^kAttackExponent)
coeff = exp(-1.0 / (attackTime * sampleRate))
current_GR = coeff * current_GR + (1 - coeff) * target_GR
```

The key behavior: larger transients (higher magnitude) produce a shorter time constant,
giving faster initial attack. As the filter converges and magnitude shrinks, the time
constant lengthens and convergence slows. This produces the characteristic classic VCA compressor
attack shape: fast onset that decelerates near final GR.

### Release

```
releaseStep = 125.0 / sampleRate   (dB per sample)
current_GR = min(current_GR + releaseStep, target_GR)
```

Constant rate at 125 dB/sec, independent of GR depth.

## Calibration Results

### Constants (before)

| Parameter           | Value  |
|---------------------|--------|
| `kBaseAttackTime_s` | 0.001  |
| `kAttackExponent`   | 1.0 (implicit linear) |

**Pre-calibration attack times (to 90% of target):**
| GR Depth | Measured | Spec  | Error    |
|----------|----------|-------|----------|
| 10 dB    | 0.88 ms  | 15 ms | 17x fast |
| 20 dB    | 0.43 ms  | 5 ms  | 12x fast |
| 30 dB    | 0.27 ms  | 3 ms  | 11x fast |

**Problems identified:**
1. Base time constant far too small (all times ~10-17x too fast)
2. Linear 1/magnitude scaling gave insufficient program-dependent differentiation
   (t10/t30 ratio: 3.3x measured vs 5.0x published)

### Constants (after calibration)

| Parameter           | Value  |
|---------------------|--------|
| `kBaseAttackTime_s` | 0.025  |
| `kAttackExponent`   | 1.55   |

### Attack timing (post-calibration, 44.1 kHz)

| GR Depth | Measured  | Spec  | Deviation | Within 20%? |
|----------|-----------|-------|-----------|-------------|
| 10 dB    | 15.65 ms  | 15 ms | +4.3%     | Yes         |
| 20 dB    | 5.33 ms   | 5 ms  | +6.6%     | Yes         |
| 30 dB    | 2.83 ms   | 3 ms  | -5.5%     | Yes         |

Program-dependent ratios:
- t10/t20 = 2.94x (published: 3.0x)
- t10/t30 = 5.53x (published: 5.0x)

### Release rate (post-calibration)

| GR Depth | Rate (dB/sec) | Deviation |
|----------|---------------|-----------|
| -5 dB    | 125.0         | 0%        |
| -10 dB   | 125.0         | 0%        |
| -20 dB   | 125.0         | 0%        |
| -30 dB   | 125.0         | 0%        |
| -50 dB   | 125.0         | 0%        |

Release is implemented as a constant dB/sample step, so it is exactly 125 dB/sec
at all depths and sample rates.

### Transient preservation ("crack" test)

A synthetic drum transient (0.5 ms linear attack, 20 ms exponential decay, peak 0.9)
was fed through the compressor with threshold=-20 dB, ratio=4:1.

- Input peak (first 2 ms): 0.90
- Output peak (first 2 ms): 0.90
- Peak preservation ratio: 100%

The RMS detector's 20 ms time constant means the sidechain hasn't responded during
the initial transient, allowing the peak to pass through. This is the mechanism
behind the reference hardware's characteristic "crack" on drums.

### Sample rate independence

| Metric          | 44.1 kHz | 96 kHz  | Difference |
|-----------------|----------|---------|------------|
| 10 dB attack    | 15.65 ms | 15.66 ms | < 0.1%    |
| 30 dB attack    | 2.83 ms  | 2.84 ms  | < 0.4%    |
| Release rate    | 125 dB/s | 125 dB/s | 0%         |

## Rationale for Exponent Choice

The published reference hardware timing shows attack time scaling approximately as:

```
t_attack ~ C / (GR_depth ^ 1.5)
```

A linear scaling (exponent = 1.0) produced a t10/t30 ratio of only 3.3x vs the
published 5.0x. The exponent of 1.55 was chosen to match the observed super-linear
relationship between transient magnitude and attack speed. This reflects the physical
behavior of the VCA control circuit where larger signal excursions drive the
attack amplifier harder, producing disproportionately faster response.

## Deviations from Hardware

This model is a simplified approximation. Known differences:

1. **Attack shape:** The reference hardware uses an analog integrator with junction-impedance
   nonlinearities. Our one-pole digital filter with time-varying coefficient captures
   the envelope well but may differ in fine detail (overshoot, sub-ms transient shape).

2. **No junction-impedance modeling:** The hardware's VCA control path has impedance
   characteristics that affect the attack/release transition region. Our model uses
   a clean threshold between attack and release modes.

3. **Release is exactly linear:** The hardware release is approximately constant-rate
   but may show minor deviations at extreme GR depths due to component tolerances.
   Our model is mathematically exact.

These deviations are well within the acceptance criteria and produce the correct
subjective character: fast transient onset, decelerating attack, and smooth
constant-rate release.

## Test Coverage

13 tests in `tests/test_timing_calibration.cpp`:
- 3 attack timing tests (10/20/30 dB, 20% tolerance)
- 1 program-dependent ordering test
- 5 release rate tests (-5 to -50 dB, 15% tolerance)
- 2 transient preservation tests (crack + sustained compression)
- 2 sample rate independence tests (44.1 kHz vs 96 kHz)
