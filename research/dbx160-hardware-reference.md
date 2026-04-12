# DBX 160 Hardware Reference

## Model Lineage

| Model | Year | VCA | Detector | Knee | Notes |
|-------|------|-----|----------|------|-------|
| 160 VU | 1976 | 200 "silver can" (7-transistor discrete) | 207/208 discrete | Hard only | Original. VU meter. |
| 161 | 1976 | 200 "silver can" | 207/208 | Hard only | Budget 160, simpler output stage. |
| 162 | 1978 | 210 (4-transistor) | Dual RMS | Hard only | Stereo linked pair. |
| 165/165A | 1978 | 202 "black can" (8-transistor) | IC | OverEasy introduced | First soft knee in the dbx line. |
| 160X | 1982 | IC (2152/THAT 2159) | IC | Hard + OverEasy | First IC VCA in 160 series. LED meters. |
| 160XT | ~1984 | IC | IC | Hard + OverEasy | Added balanced XLR output. |
| 160A | ~1990 | IC | IC | Hard + OverEasy | SMT PCB. Negative ratios (Infinity+). Current production. |
| 160S | ~1993 | V8 (32-transistor discrete, potted) | -- | Hard + OverEasy + Auto | Premium. Jensen transformers. Manual A/R. |
| 160SL | ~2000 | V8 | -- | Hard + OverEasy + AutoVelocity | Flagship. |

**Primary emulation target:** The original 160 VU character (discrete Blackmer 200 VCA), with the 160A's control set (OverEasy toggle, Infinity+ ratios) as the UI reference, since the 160A is the most commonly referenced unit in modern production.

---

## Signal Path (Original 160 VU)

```
Balanced Input
    |
    v
Input Stage (TL082 differential amp, -6 dB gain)
    |
    +---> Sidechain tap (to RMS detector)
    |
    v
Blackmer 200 VCA (current-in / current-out)
    |
    v
Transimpedance Amplifier (current-to-voltage, JFET op-amp)
    |
    v
Output Driver (non-inverting, +10 dB gain, push-pull)
    |
    v
XLR Output (ground-canceling, NOT true balanced)
```

### Key details

- The VCA operates in the **current domain** -- signal enters as current and exits as current. External op-amp stages handle V-to-I and I-to-V conversion.
- The output is single-ended with a ground-canceling driver (pin 3 cancels ground hum on pin 1). Not a true differential balanced output.
- The 161 omits the push-pull output transistors; the transimpedance amp doubles as the output driver.

---

## Blackmer Gain Cell (VCA)

### Topology
Four-transistor push-pull core: two complementary bipolar current mirrors (NPN lower, PNP upper). Performs **log-antilog** gain control via the Shockley equation:

```
I_out = I_in * exp(V_control / V_thermal)
```

where V_thermal ~ 25.85 mV at 300K.

The seven-transistor Model 200 adds three transistors for biasing and error correction over the four-transistor core.

### Control law
**-6 mV/dB** ("decilinear") -- every 6 mV change in control voltage produces 1 dB change in signal level. Linear control voltage changes produce logarithmic (dB) audio changes.

### Performance (Model 200)
- Dynamic range: 110-116 dB
- THD: ~0.03-0.2% (higher than later generations; this contributes to the "character")
- Control port sensitivity: 1 mV of noise on control port produces ~4% modulation (requires very clean control voltage)

### Nonlinearities
Three sources of distortion in the Blackmer VCA:
1. **Logarithming error** from finite parasitic resistances in the transistors
2. **Asymmetry** between the NPN and PNP transistor pairs (even-order harmonics)
3. **Nonlinearity** of the input voltage-to-current converter (A1 op-amp stage)

The original Model 200 has measurably higher distortion than later IC versions (2150 series at 0.01%, 2001 at <0.001%). This is widely credited as the source of the "warm, grainy" character. Cytomic measured the discrete Blackmer at -84 dBFS 2nd harmonic, -97 dBFS 3rd harmonic -- predominantly even-order.

### Manufacturing note
Discrete transistors were individually matched in an oven, then thermally coupled with a conductive ceramic block inside a sealed metal can. Temperature drift causes gain drift (inverse relationship).

---

## Sidechain / Detector

### Architecture: Feedforward (CONFIRMED)

The sidechain derives its signal from the **input** (before the VCA), not the output. This is feedforward topology.

**Consequences:**
- Stable at infinite and even negative compression ratios (no feedback loop to oscillate)
- Faster, more predictable transient response
- The detector "sees" the full uncompressed dynamics

### Detection: True RMS (CONFIRMED)

The Blackmer RMS detector (207/208) is the **first successful commercialized log-domain filter** (1971 invention).

**How it works:**
1. Log conversion is performed **before** rectification, using diode-connected transistors
2. In the log domain, squaring = multiplication by 2, square root = division by 2 (trivial scaling)
3. Time-averaging occurs on a capacitor in the log domain
4. Output voltage is proportional to log(RMS level) at ~6 mV/dB

This gives true RMS response -- insensitive to waveform shape and phase, responding to actual signal energy content.

### Sidechain signal flow (160 VU)

1. Input signal tapped to RMS detector (207/208 module)
2. Detector outputs positive DC voltage proportional to log(RMS level)
3. First op-amp (OA3) with "level cal" pot for calibration
4. Second op-amp (OA5) subtracts the **threshold** DC offset -- this sets the compression onset point
5. **Ratio control** sets the gain of the sidechain path (how much detected level change reaches the VCA)
6. Final op-amp provides makeup gain offset before the VCA control port

### Timing component values (160 VU)
- **22 uF capacitor** with **909K timing resistor** in the RMS detector smoothing circuit

---

## Attack and Release (Program-Dependent)

The DBX 160 has **NO user-adjustable attack or release controls** (original through 160A). Timing adapts automatically to program material.

### Published specifications (from 160A datasheet)

**Attack:**
| Gain reduction | Time |
|----------------|------|
| 10 dB | 15 ms |
| 20 dB | 5 ms |
| 30 dB | 3 ms |

Larger transients trigger faster attack -- the harder the hit, the faster the response.

**Release:**
| Gain reduction | Time |
|----------------|------|
| 1 dB | 8 ms |
| 10 dB | 80 ms |
| 50 dB | 400 ms |

Release rate: ~125 dB/sec (roughly constant in dB/sec, not constant time)

### Mechanism

The program-dependent behavior arises from a **single smoothing capacitor placed across the current-driven diode junction** in the log-domain RMS detector. Since junction impedance varies exponentially with signal level:

- **Louder signals = lower junction impedance = shorter effective time constant = faster attack**
- **Quieter signals = higher junction impedance = longer effective time constant = slower release**

This is NOT a simple one-pole RC filter -- the time constant is signal-dependent because the resistance is the junction impedance itself.

---

## Compression Characteristics

### Knee: Hard (original 160 VU)
Below threshold: signal passes uncompressed. Above threshold: full ratio applied immediately. No transition zone.

OverEasy (soft knee) was introduced in the 165 (1978) and added to 160X/160A as a switchable option.

### Ratio range
- 160 VU: 1:1 to infinity:1
- 160A: 1:1 through infinity:1 to negative ratios (-1:1) -- the "Infinity+" mode where increasing input above threshold causes decreasing output (dynamics inversion)

### How ratio is implemented
The compression ratio is set by the **gain of the sidechain path**:
- 1:1 = zero sidechain gain (no control voltage change)
- infinity:1 = unity sidechain gain (VCA attenuates by exactly the input increase)
- Negative ratios = sidechain gain > 1 (VCA attenuates more than the input increased)

### Maximum gain reduction
Over 60 dB.

---

## Front Panel Controls

All 160/160X/160XT/160A models share the same **three-knob** philosophy:

| Control | Range | Function |
|---------|-------|----------|
| **Threshold** | -40 to +20 dBu (160A) | Level above which compression engages |
| **Compression (Ratio)** | 1:1 to Infinity:1 (to -1:1 on 160A) | Compression ratio |
| **Output Gain** | -20 to +20 dB | Post-compression makeup gain |

Additional controls:
- **OverEasy button** (160X/160A): Toggles soft-knee mode
- **Meter switch**: Input / Output / Gain Reduction display
- **Above/Below threshold LEDs**: Green (below), amber (OverEasy zone), red (above)
- **Stereo link** (160A): Front panel button, requires strapping cable

**Output gain is independent of threshold** -- adjusting one does not affect the other.

---

## Operating Levels and Calibration

| Parameter | 160 VU | 160A |
|-----------|--------|------|
| Nominal level (0 VU) | +4 dBu | +4 dBu |
| Max balanced output | +26 dBu | +24 dBm |
| Headroom above nominal | ~20-22 dB | ~20 dB |
| Input impedance | -- | 40k ohms |
| Output impedance | -- | 200 ohms bal / 100 ohms unbal |
| Frequency response | 20 Hz - 20 kHz +0/-0.5 dB | +0/-3 dB at 0.5 Hz and 90 kHz |
| THD | < 0.2% (any compression up to 40 dB @ 1 kHz) | < 0.2% typical |
| Noise floor | -89 dBu EIN | < -90 dBu unweighted |
| Dynamic range | -- | > 113 dB |

---

## Metering

### 160 VU
- Analog VU meter, illuminated, full 60 dB range
- Switchable: Input level / Output level / Gain reduction
- Factory calibration: 0 VU = +4 dBu
- Rear panel trimmer to recalibrate 0 VU reference (+10 to -10 dBu range)
- Standard VU ballistics: 300 ms integration time (shows quasi-average, not peaks)

### 160X/160A
- 19-segment LED array: switchable input or output level (-40 to +20 dB)
- 12-segment LED array: gain reduction display (0 to -40 dB)

---

## Sonic Character

### What makes the 160 sound "punchy"

1. **Hard knee**: Abrupt compression onset emphasizes the leading edge of transients -- the very front of a drum hit passes through before the compressor clamps, creating an enhanced attack followed by controlled sustain.

2. **Program-dependent fast attack**: The harder the transient hits, the faster the compressor reacts. But it is NOT instant -- the first few milliseconds of a transient always pass through, preserving "crack" and "snap."

3. **Nonlinear attack trajectory**: Attack is faster at onset and slows as it approaches final gain reduction. This preserves transient impact even under heavy compression.

4. **Constant-rate release (dB/sec)**: Deep compression releases over a longer time period but at a consistent rate, producing a natural "breathing" rather than an abrupt snap-back.

5. **RMS detection**: Responds to signal energy rather than peaks, producing compression that tracks perceived loudness. Does not react to every peak, which preserves dynamic texture.

6. **Discrete VCA coloration**: The Model 200's slightly higher distortion (compared to later IC VCAs) adds warmth and "graininess" -- predominantly even-order harmonics.

### Widely cited descriptions
- "Rough, warm and grainy punch"
- "Distinct nonlinearities, making it a character piece"
- "Unusual for a VCA model" in its coloration
- Adds "subtle low-mid weight" and "solid, present" quality
- When pushed: characteristic "pumping and breathing" that enhances rhythmic feel

### Hip-hop relevance
- Among the most-used compressors in hip-hop production (late 1980s-1990s)
- The "knock" on kick drums and "crack" on snares that defines classic hip-hop drum sounds
- Tight, focused bass response
- Used on DMX "It's Dark And Hell Is Hot" (1998) and many others

### Typical production settings

**Kick drum (punchy):** Hard Knee, 4:1-6:1, 6-10 dB GR on hits
**Snare (snap):** Hard Knee, 4:1-5:1, 4-6 dB GR
**Drum bus (glue):** Hard Knee or OverEasy, 4:1, moderate threshold
**Bass:** Hard Knee or OverEasy, 3:1, 3-5 dB constant GR
**Aggressive hip-hop drums:** Hard Knee, 6:1+, deep GR, often used in parallel

---

## Sources

- Mix Online: "Birth of a Classic: The dbx 160 Compressor" (most detailed technical source)
- Wikipedia: Blackmer Gain Cell, Blackmer RMS Detector
- THAT Corporation: "A Brief History of VCAs"
- GroupDIY: dbx 160 thread (sidechain analysis, component values)
- GroupDIY: dbx 160VU clone build thread (timing R/C values)
- dbx Professional Audio: 160A product page and manual
- Sound on Sound: Waves dbx 160 review, dbx 160S review
- Attack Magazine: Top 20 Hardware Compressors
- Gearspace: DBX 160 circuit operation thread
- Gearspace: "What's the difference between 160/160XT/160A"
- Universal Audio: dbx 160 Compressor Manual
- Ovnilab: 160X/160XT review
- Sine-Post Audio: 160A review (production settings)
- Gearspace Hip-Hop forum: "dbx 160A = dope"
