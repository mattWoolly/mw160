# mw160

A faithful digital emulation of a classic 1U-rack VCA feedback compressor from the late 1970s, built as a cross-platform audio plugin with JUCE 7.

<!-- TODO: Add a screenshot of the plugin editor here -->
<!-- ![mw160 screenshot](docs/images/mw160-screenshot.png) -->

## Overview

mw160 models the behavior of a hardware VCA feedback compressor known for its punchy, musical dynamics control. The feedback topology — where the detector reads the compressor's own output rather than its input — produces an inherently program-dependent response that adapts to the character of the incoming audio. Louder transients compress faster, quieter signals compress more gently, and the release follows a constant dB-per-second rate that keeps the compressor transparent across a wide range of gain reduction depths.

The plugin pairs this vintage-inspired DSP core with a modern feature set: parallel dry/wet mix, sample-accurate bypass, stereo linking, user presets, and a resizable GUI styled after classic 1U rack hardware.

## Features

### DSP

- **Feedback topology** — detector reads post-VCA output, producing natural program-dependent compression without explicit attack/release controls
- **True-RMS level detection** with a ~20 ms time constant and block-size-independent state
- **Program-dependent ballistics** — attack scales inversely with transient magnitude (~3 ms for a 30 dB step, ~15 ms for 10 dB); release runs at a constant ~125 dB/sec
- **Soft-knee gain computer** — quadratic interpolation through a 10 dB knee region (switchable hard/soft)
- **VCA saturation modeling** — subtle even-order harmonic coloration that scales with gain reduction depth (~0.1-0.2% THD at moderate compression)
- **Infinity ratio** — ratio control reaches true limiter mode at maximum
- **Glitch-free automation** — all parameters smoothed per-sample with 20 ms linear ramps
- **Zero-allocation audio thread** — verified by dedicated no-alloc regression tests

### Controls

| Control | Range | Default | Description |
|---------|-------|---------|-------------|
| Threshold | -40 to +20 dBFS | -12.0 | Compression onset level |
| Ratio | 1:1 to infinity | 4.0:1 | Compression slope (60:1 and above = limiter) |
| Output Gain | -20 to +20 dB | +3.0 | Post-compression makeup gain |
| Mix | 0-100% | 100% | Dry/wet blend for parallel compression |
| Knee | Hard / Soft | Hard | Gain computer knee shape |
| Stereo Link | Off / On | On | Locks L+R to a single detector for mono-compatible imaging |
| Bypass | Off / On | Off | Sample-accurate bypass with click-free crossfade |

### GUI

- 1U-rack-inspired layout at 900 x 360 px (resizable from 720 x 288 to 1440 x 576, locked 2.5:1 aspect ratio)
- Three independent LED-ladder meters: Input, Output, and Gain Reduction
- Brushed-metal faceplate header with engraved wordmark
- Custom rotary knobs with arc-fill indicators and amber accent color
- Threshold indicator LEDs (BELOW / ABOVE)
- Preset browser with factory/user separation, save, and delete

### Factory Presets

| Preset | Use Case |
|--------|----------|
| Gentle Leveling | Transparent bus leveling (2:1, soft knee) |
| Kick Punch | Drum transient shaping (4:1, hard knee) |
| Snare Snap | Snare bite and body (5:1, hard knee) |
| Drum Bus Glue | Cohesive drum bus compression (4:1, soft knee) |
| Bass Control | Smooth bass dynamics (3:1, soft knee) |
| Vocal Presence | Vocal body and presence (3:1, soft knee) |
| Parallel Smash | Heavy parallel compression (8:1, 50% mix) |
| Brick Wall | Limiter mode (60:1, hard knee) |

## Plugin Formats

| Format | Platform |
|--------|----------|
| VST3 | Linux, macOS, Windows |
| AU | macOS |
| Standalone | Linux, macOS, Windows |

## Building

### Prerequisites

- **CMake** 3.22 or later
- **C++17** compiler (GCC 11+, Clang 14+, MSVC 2022+)
- **JUCE 7.0.12** (fetched automatically via CMake FetchContent)
- **Catch2 v3.5.4** (fetched automatically for tests)

**Linux additional dependencies:**

```bash
sudo apt install libasound2-dev libfreetype-dev libx11-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libfontconfig1-dev
```

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MW160_All -j$(nproc)
```

Build artifacts appear under `build/MW160_artefacts/Release/`:

```
VST3/MW160.vst3/
AU/MW160.component/          # macOS only
Standalone/MW160
```

### Install

**Linux (VST3):**
```bash
cp -r build/MW160_artefacts/Release/VST3/MW160.vst3 ~/.vst3/
```

**macOS (AU + VST3):**
```bash
cp -r build/MW160_artefacts/Release/AU/MW160.component ~/Library/Audio/Plug-Ins/Components/
cp -r build/MW160_artefacts/Release/VST3/MW160.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

**Windows (VST3):**
```powershell
Copy-Item -Recurse build\MW160_artefacts\Release\VST3\MW160.vst3 "$env:COMMONPROGRAMFILES\VST3\"
```

## Testing

The test suite covers DSP correctness, plugin state management, audio-thread safety, and golden-file regression across 26 test files.

### Run Tests

```bash
# Build and run all tests
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MW160Tests MW160NoAllocTests -j$(nproc)
cd build && ctest --output-on-failure
```

### Test Categories

| Category | What It Covers |
|----------|---------------|
| DSP correctness | RMS detector convergence, gain computer curves, ballistics timing, VCA saturation harmonics, compressor integration |
| Golden regression | 8 reference audio files capturing the full signal path — any DSP change that alters output is caught |
| State management | Plugin state save/restore, preset loading, parameter display names and labels |
| Audio-thread safety | Zero heap allocation during `processBlock` (custom `operator new` override) |
| Block independence | Identical output regardless of host buffer size (1 to 4096 samples) |
| Parameter behavior | Smoother convergence, automation glitch-freedom, mix blend accuracy |

### Plugin Validation

```bash
# Validate with pluginval (strictness 10)
pluginval --validate build/MW160_artefacts/Release/VST3/MW160.vst3 \
  --strictness-level 10 --validate-in-process
```

## Architecture

### Signal Flow

```
                          Feedback Path
                   .─────────────────────────────────.
                   |                                  |
Input ─────┬───────|──> VCA ──> Saturation ──> Makeup Gain ──┬──> Mix ──> Output
           |       |    (apply GR)              (post-VCA)   |    (dry/wet)
           |       |                                         |       ^
           |       '── RMS Detector ──> Gain Computer ───────'       |
      (dry copy)       (~20 ms)         (soft/hard knee)             |
           |                                                         |
           |              ^                                          |
           |              |                                          |
           |          Ballistics                                     |
           |          (program-dependent                             |
           |           attack/release)                               |
           |                                                         |
           '─────────────────────────────────────────────────────────'
                              (parallel mix)
```

The feedback loop is the defining characteristic: the detector reads the compressor's own output (delayed by one sample), creating a self-regulating servo that naturally adapts to program material.

### Project Structure

```
mw160/
├── CMakeLists.txt
├── src/
│   ├── PluginProcessor.cpp/h      # JUCE processor, APVTS, metering
│   ├── PluginEditor.cpp/h         # Custom GUI (900x360, resizable)
│   ├── PresetManager.cpp/h        # Factory + user preset system
│   ├── FactoryPresets.h            # 8 built-in presets
│   ├── dsp/                        # Pure C++ DSP (no JUCE dependency)
│   │   ├── Compressor.cpp/h       # Core engine (wires all DSP stages)
│   │   ├── RmsDetector.cpp/h      # True-RMS exponential detector
│   │   ├── GainComputer.cpp/h     # Threshold/ratio/knee curves
│   │   ├── Ballistics.cpp/h       # Program-dependent attack/release
│   │   ├── VcaSaturation.cpp/h    # Even-order harmonic waveshaper
│   │   └── ParameterSmoother.h    # Per-sample parameter smoothing
│   └── gui/
│       ├── CustomLookAndFeel.h     # Color palette and component styling
│       ├── LedMeter.h              # Vertical LED-ladder meter
│       ├── MeterModeButton.h       # IN/OUT/GR selector
│       └── Fonts.h                 # Embedded font management
├── tests/
│   ├── test_*.cpp                  # 26 Catch2 test files
│   └── golden/                     # 8 golden reference audio files
├── assets/fonts/                   # Inter + JetBrains Mono (SIL OFL)
├── docs/
│   ├── REFERENCE.md                # DSP behavioral specification
│   └── VISUAL_SPEC.md              # GUI design specification
└── .github/workflows/
    └── build-and-validate.yml      # CI: Linux, macOS, Windows
```

The DSP library (`src/dsp/`) is a standalone C++ static library with no JUCE dependency, making it independently testable and portable.

## CI

GitHub Actions runs on every push and pull request:

- **Linux** (Ubuntu) — build, unit tests, pluginval strictness 10
- **macOS** (Apple Silicon + Intel universal) — build, unit tests, pluginval strictness 10
- **Windows** (MSVC) — build, unit tests, pluginval strictness 10

All three platforms must pass before merge.

### Releases

Pushing a version tag triggers a release build across all platforms. If all tests and pluginval validation pass, packaged binaries are uploaded to a GitHub Release automatically.

```bash
git tag v0.1.0
git push origin v0.1.0
```

Download pre-built binaries from the [Releases](../../releases) page.

## Acknowledgments

- Built with [JUCE 7](https://juce.com/) (GPLv3 / commercial license)
- Tested with [Catch2](https://github.com/catchorg/Catch2) and [pluginval](https://github.com/Tracktion/pluginval)
- Fonts: [Inter](https://rsms.me/inter/) and [JetBrains Mono](https://www.jetbrains.com/lp/mono/) (SIL Open Font License)
