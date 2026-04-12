# DSP Implementation Notes -- MW160

## Recommended Signal Flow

```
Input Buffer
    |
    v
[Input Gain + Subtle Saturation (optional)]
    |
    +---> Audio Path                Sidechain Path <---+
    |         |                         |              |
    |    [Delay Line               [RMS Detector       |
    |     (optional look-ahead)]    (squared signal    |
    |         |                      + smoothing)]     |
    |         |                         |              |
    |         |                    [Log Conversion      |
    |         |                     (linear -> dB)]    |
    |         |                         |              |
    |         |                    [Gain Computer       |
    |         |                     (threshold, ratio,  |
    |         |                      knee)]            |
    |         |                         |              |
    |         |                    [Ballistics          |
    |         |                     (program-dependent  |
    |         |                      attack/release)]  |
    |         |                         |              |
    |         +--------+----------------+              |
    |                  |                               |
    |            [Apply Gain                           |
    |             (multiply in linear domain)]         |
    |                  |                               |
    |            [VCA Nonlinearity                     |
    |             (subtle even-order saturation)]      |
    |                  |                               |
    |            [Makeup Gain]                         |
    |                  |                               |
    |            [Dry/Wet Mix (optional)]              |
    |                  |                               |
    |               Output                             |
```

---

## 1. VCA Modeling

### Approach: Simplified Gain + Waveshaping

The Blackmer 200 VCA is very clean (< 0.2% THD overall). A full circuit simulation is not necessary for the initial implementation. Recommended approach:

1. **Primary gain stage**: Simple multiplication of audio by gain factor in linear domain
2. **Subtle saturation**: Apply a soft-saturation waveshaper to introduce the even-order harmonic character

The key nonlinearity is **predominantly even-order** (2nd harmonic dominant at -84 dBFS per Cytomic measurements). A simple asymmetric soft-clip or polynomial waveshaper can approximate this:

```
// Soft asymmetric saturation (predominantly 2nd harmonic)
float saturate(float x, float amount) {
    // amount controls how much coloration (0 = clean, 1 = full character)
    float x2 = x * x;  // even-order term
    return x + amount * (x2 * 0.05f - x * x2 * 0.02f);
}
```

The amount of saturation should be subtle and roughly gain-dependent -- more attenuation = slightly more coloration (the VCA works harder at deeper gain reduction).

### Future refinement
For higher fidelity, consider WDF-based modeling of the actual transistor current mirrors using chowdsp_wdf. This is a Phase 2+ concern.

---

## 2. RMS Detector

### Implementation

True RMS detection via exponential moving average of the squared signal:

```cpp
// Per-sample RMS detector
float inputSquared = input * input;
rmsSquared = rmsCoeff * rmsSquared + (1.0f - rmsCoeff) * inputSquared;
float rmsLevel = std::sqrt(rmsSquared);
float rmsDB = 20.0f * std::log10(rmsLevel + 1e-30f);  // avoid log(0)
```

The RMS averaging coefficient sets the "window" of the detector. For the DBX 160, the detector uses a 22 uF cap with 909K resistor, giving a time constant of:

```
tau = R * C = 909e3 * 22e-6 = ~20 ms
```

This is the baseline RMS averaging time. The coefficient at sample rate `fs`:
```
rmsCoeff = exp(-1.0 / (0.020 * fs))
```

### Stereo handling

For stereo operation with linking, sum the squared signals from both channels before RMS computation (True RMS Power Summing, as per the 160A's stereo link behavior).

---

## 3. Gain Computer

### Hard knee (original 160)

```cpp
float computeGainReduction_dB(float inputLevel_dB, float threshold_dB, float ratio) {
    if (inputLevel_dB <= threshold_dB)
        return 0.0f;  // no compression
    
    float excess = inputLevel_dB - threshold_dB;
    float targetOutput_dB = threshold_dB + excess / ratio;
    return targetOutput_dB - inputLevel_dB;  // negative value = attenuation
}
```

### Soft knee (OverEasy)

The OverEasy knee creates a gradual transition zone centered on the threshold. Using the Giannoulis et al. formulation with a knee width `W`:

```cpp
float computeGainReduction_dB(float x_dB, float T, float R, float W) {
    if (x_dB < (T - W / 2.0f))
        return 0.0f;  // below knee
    
    if (x_dB > (T + W / 2.0f)) {
        // above knee -- full ratio
        float excess = x_dB - T;
        return excess * (1.0f / R - 1.0f);
    }
    
    // in the knee region -- quadratic interpolation
    float diff = x_dB - T + W / 2.0f;
    return (1.0f / R - 1.0f) * diff * diff / (2.0f * W);
}
```

Recommended OverEasy knee width: 6-12 dB (needs A/B testing against hardware recordings).

### Negative ratios (Infinity+)

When ratio < 0 (or expressed as sidechain gain > 1), the same formula works -- the gain reduction exceeds the excess, causing output to decrease as input increases above threshold.

---

## 4. Program-Dependent Attack/Release

This is the most critical element to get right for the DBX 160's character.

### Key insight: constant-rate release

The published specs show release is approximately **125 dB/sec** constant rate (not constant time). This means:
- 1 dB GR releases in ~8 ms
- 10 dB GR releases in ~80 ms
- 50 dB GR releases in ~400 ms

### Recommended implementation: hybrid approach

```cpp
// Program-dependent ballistics
void updateBallistics(float targetGR_dB, float& currentGR_dB, float sampleRate) {
    float diff = targetGR_dB - currentGR_dB;
    
    if (diff < 0.0f) {
        // ATTACK: target is more negative (more compression)
        // Faster attack for larger transients
        float attackMagnitude = std::abs(diff);
        // Scale attack time inversely with magnitude
        // 15ms for 10dB, 5ms for 20dB, 3ms for 30dB
        float attackTime_s = 0.015f * 10.0f / std::max(attackMagnitude, 1.0f);
        attackTime_s = std::clamp(attackTime_s, 0.001f, 0.050f);
        float attackCoeff = std::exp(-1.0f / (attackTime_s * sampleRate));
        currentGR_dB = attackCoeff * currentGR_dB + (1.0f - attackCoeff) * targetGR_dB;
    } else {
        // RELEASE: constant rate in dB/sec
        float releaseRate_dBperSample = 125.0f / sampleRate;  // ~125 dB/sec
        currentGR_dB = std::min(currentGR_dB + releaseRate_dBperSample, targetGR_dB);
    }
}
```

### Alternative: junction-impedance model

For higher fidelity, model the actual capacitor-across-diode-junction behavior:
- The effective time constant is `tau = C * R_junction`
- `R_junction` varies exponentially with the voltage across it (signal level)
- This creates the program-dependent behavior organically

This is more complex but may capture subtleties the simplified model misses. Consider for Phase 2.

---

## 5. Parameter Smoothing

### Parameters that need smoothing
- Threshold (user control)
- Ratio (user control)
- Output/makeup gain (user control)

### Approach: JUCE SmoothedValue

```cpp
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGainSmoothed;

// In prepareToPlay:
outputGainSmoothed.reset(sampleRate, 0.020);  // 20ms smoothing time

// When parameter changes:
outputGainSmoothed.setTargetValue(newGainLinear);

// Per sample in processBlock:
float gain = outputGainSmoothed.getNextValue();
```

### Gain reduction smoothing
The ballistics filter (attack/release) already provides smooth gain reduction changes. No additional smoothing is needed on the GR signal itself -- double-smoothing would alter the timing character.

---

## 6. Oversampling

### Recommendation: Optional, start without

The DBX 160's VCA is very clean. For the initial implementation:
- **Phase 1**: No oversampling. The gain modulation is smooth (from ballistics) and the VCA saturation is subtle.
- **Phase 2**: Add optional 2x oversampling if saturation modeling introduces audible aliasing.

If adding oversampling, use JUCE's `dsp::Oversampling<float>`:

```cpp
juce::dsp::Oversampling<float> oversampling{2, 1, 
    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR};

// In prepareToPlay:
oversampling.initProcessing(samplesPerBlock);
setLatencySamples(oversampling.getLatencyInSamples());
```

Use IIR (minimum phase) filters rather than FIR to minimize latency.

---

## 7. Latency

### Target: zero latency for baseline

The original hardware has zero look-ahead. The emulation should match:
- No look-ahead delay (feedforward detector responds to current input)
- No oversampling (Phase 1)
- Report 0 latency to host

### Optional enhancements (Phase 2+)
- **Look-ahead** (1-5 ms): Delay audio path while sidechain processes ahead. Useful for brick-wall limiting at infinity:1. Implement as a circular buffer.
- **Oversampling latency**: If using FIR-based oversampling, report via `setLatencySamples()`.

---

## 8. Real-Time Safety Constraints

### Rules for processBlock()

**NEVER:**
- `new` / `delete` / `malloc` / `free`
- `std::mutex` lock/unlock
- File I/O, logging, printing
- System calls
- Anything with unbounded execution time

**ALWAYS:**
- Pre-allocate all working memory in `prepareToPlay()`
- Use `std::atomic<float>` for parameter reads (via APVTS `getRawParameterValue()`)
- Use `std::atomic<float>` to publish meter values to GUI thread
- Use `SmoothedValue` for per-sample parameter interpolation

### Architecture pattern

```
GUI Thread                          Audio Thread
    |                                    |
    +-- APVTS parameter change --------> std::atomic<float>* reads
    |                                    |
    +-- Timer-based meter poll <-------- std::atomic<float> GR value
    |                                    |
    +-- Repaint on timer                 processBlock() runs
```

---

## 9. JUCE Architecture

### Plugin class structure

```cpp
class MW160Processor : public juce::AudioProcessor {
public:
    // Parameters via APVTS
    juce::AudioProcessorValueTreeState apvts;
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void releaseResources() override;
    
    // Meter output (audio thread writes, GUI reads)
    std::atomic<float> currentGainReduction{0.0f};
    std::atomic<float> currentInputLevel{0.0f};
    std::atomic<float> currentOutputLevel{0.0f};
    
private:
    // DSP state (allocated in prepareToPlay)
    float rmsSquared[2] = {};           // per-channel RMS state
    float currentGR_dB[2] = {};         // per-channel gain reduction
    
    // Smoothed parameters
    juce::SmoothedValue<float> thresholdSmoothed;
    juce::SmoothedValue<float> ratioSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGainSmoothed;
    
    // Optional
    // juce::dsp::Oversampling<float> oversampling;
    // CircularBuffer lookAheadBuffer;
};
```

### Parameters

| ID | Name | Range | Default | Unit |
|----|------|-------|---------|------|
| `threshold` | Threshold | -40 to +20 | 0 | dBu |
| `ratio` | Compression | 1.0 to 60.0 (60 = infinity, >60 = negative) | 1.0 | :1 |
| `outputGain` | Output Gain | -20 to +20 | 0 | dB |
| `overEasy` | OverEasy | on/off | off | -- |
| `stereoLink` | Stereo Link | on/off | on | -- |
| `mix` | Mix | 0-100 | 100 | % |

---

## 10. Key References

### Essential papers
1. **Giannoulis, Massberg, Reiss** -- "Digital Dynamic Range Compressor Design -- A Tutorial and Analysis" (JAES 2012). Canonical reference for gain computer, detector topologies, soft knee.
2. **Giannoulis, Massberg, Reiss** -- "Parameter Automation in a Dynamic Range Compressor" (JAES 2013). Crest-factor-based automatic attack/release, auto makeup gain.
3. **Floru** -- "Attack and Release Time Constants in RMS-Based Feedback Compressors" (AES 1998, THAT Corp). Mathematical models for compressor timing.

### Open-source JUCE references
- **CTAGDRC** (github.com/p-hlp/CTAGDRC) -- Complete feedforward compressor with gain computer, ballistics, look-ahead, crest factor automation. Closest architectural reference.
- **chowdsp_utils** (github.com/Chowdhury-DSP/chowdsp_utils) -- LevelDetector, GainComputer, MonoCompressor modules, plus WDF library.
- **Stillwell Audio Major Tom** -- Program-dependent compressor inspired by the DBX 160 (commercial, but documented behavior).

### JUCE-specific
- `AudioProcessorValueTreeState` for thread-safe parameters
- `dsp::Oversampling<float>` for optional oversampling
- `SmoothedValue<float>` for parameter interpolation
- `dsp::BallisticsFilter<float>` as reference (but too simple for DBX 160 behavior)
