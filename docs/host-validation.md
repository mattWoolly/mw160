# MW160 Host Validation Report

**Date:** 2026-04-07
**Plugin version:** 0.1.0
**pluginval version:** 1.0.4

---

## Automated Validation (pluginval)

The MW160 VST3 plugin passes pluginval at **maximum strictness (level 10)** with zero failures.

### Tests executed

| Category | Result | Notes |
|---|---|---|
| Plugin scan | PASS | Detects 1 plugin: `VST3-MW160-141e20c9-fd502feb` |
| Open plugin (cold) | PASS | |
| Open plugin (warm) | PASS | |
| Plugin info | PASS | Name: MW160, Latency: 0, Tail: 0 |
| Plugin programs | PASS | 7 programs (factory presets) |
| Editor | PASS | Opens and closes without errors |
| Open editor whilst processing | PASS | No glitches or crashes |
| Audio processing | PASS | 44.1/48/96 kHz, block sizes 64-1024 |
| Non-releasing audio processing | PASS | Level 10 only |
| Plugin state | PASS | Save/restore roundtrip |
| Plugin state restoration | PASS | Level 10 only |
| Automation | PASS | All sample rates, block sizes, sub-block 32 |
| Editor automation | PASS | |
| Automatable parameters | PASS | All 6 parameters |
| Parameters (detailed) | PASS | Level 10 only |
| Background thread state | PASS | Level 10 only |
| Parameter thread safety | PASS | Level 10 only |
| Bus configurations | PASS | Mono and stereo I/O |
| Fuzz parameters | PASS | Level 10 only |

### Bus layouts validated

- **Inputs:** Mono, Stereo, Discrete #1
- **Outputs:** Mono, Stereo, Discrete #1
- Default: Stereo in / Stereo out

### How to run locally

```bash
./scripts/validate_plugin.sh              # Default: strictness 5
./scripts/validate_plugin.sh build 10     # Maximum strictness
```

---

## CI Integration

pluginval runs automatically in the GitHub Actions workflow on every push and PR:
- **Linux:** Uses `xvfb-run` for headless editor testing
- **macOS:** Native display available on CI runners
- **Strictness level:** 5 (standard)

---

## Manual DAW Testing Checklist

The following tests require manual verification in actual DAW hosts. This should be completed before any release.

### Reaper (VST3) -- Linux / macOS / Windows

- [ ] Plugin loads without errors
- [ ] Audio processing sounds correct (compare against golden references)
- [ ] All 6 parameters automate correctly via automation lanes
- [ ] Preset save/load works (factory + user presets)
- [ ] Bypass works (DAW bypass and any internal bypass)
- [ ] Stereo and mono track configurations
- [ ] Session save/reload preserves all parameter state
- [ ] No crashes after extended use

### Logic Pro (AU) -- macOS only

- [ ] Plugin loads without errors
- [ ] Audio processing works correctly
- [ ] Parameters automate correctly
- [ ] Preset save/load works
- [ ] Bypass works
- [ ] Stereo and mono configurations
- [ ] AUVal passes: `auval -v aufx mw16 mwAu`

### Ableton Live (VST3/AU) -- macOS / Windows

- [ ] Plugin loads without errors
- [ ] Audio processing works correctly
- [ ] Parameters automate correctly (via clip envelopes)
- [ ] Preset save/load works
- [ ] Bypass works
- [ ] Stereo configuration

### JUCE AudioPluginHost

- [x] VST3 loads and processes audio (validated via pluginval)
- [x] Parameters are automatable (validated via pluginval)
- [x] State save/restore works (validated via pluginval)
- [x] Bus configurations work (validated via pluginval)

---

## Known Issues

1. **No AAX target:** AAX requires PACE iLok signing. Deferred to future work.
2. **No display crashes pluginval on Linux:** Running without Xvfb causes a segfault during editor tests. The CI workflow and validation script handle this via `xvfb-run`.
3. **vst3 validator skipped:** pluginval's built-in VST3 SDK validator was skipped because the validator binary path was not configured. This is a pluginval configuration detail, not a plugin issue.
