# CI Status findings (2026-04-11)

**Author:** orchestrator
**Scope:** Phase 4 of the coordinated QA + visual remediation pass —
cross-platform CI workflow.

## Summary

The existing `.github/workflows/build.yml` has been renamed to
`.github/workflows/build-and-validate.yml` (git mv, history preserved) and
extended in place per the task spec. The workflow now covers **three
platforms** (ubuntu-latest, macos-14, windows-latest), runs `pluginval` at
**strictness level 10** on each, and uploads the VST3, AU (macOS), and
Standalone as workflow artifacts with 14-day retention.

**Actual per-platform pass/fail status from a fresh CI run is NOT yet
available.** This file records the workflow changes, the local smoke-test
results, and the *expected* CI failures that pre-existing known defects
will surface on the next run.

## Changes to the CI workflow

| Dimension | Before | After |
|-----------|--------|-------|
| Filename | `.github/workflows/build.yml` | `.github/workflows/build-and-validate.yml` |
| Display name | `Build & Test` | `Build & Validate` |
| Matrix | `macos-14`, `ubuntu-latest` | `ubuntu-latest`, `macos-14`, `windows-latest` |
| pluginval strictness | 5 | **10** |
| pluginval arg hygiene | shell-split single step | per-OS steps with pwsh on Windows |
| macOS architectures | host default (arm64) | `arm64;x86_64` universal |
| VST3 artifact upload | none | per-OS upload, 14-day retention |
| AU bundle artifact upload | none | macOS-only, 14-day retention |
| Standalone artifact upload | none | per-OS, 14-day retention |
| Build-log upload on failure | none | on `failure()`, 7-day retention |
| VST3 path detection | bash only | per-OS (bash for Linux/macOS, pwsh for Windows) |

The FetchContent cache, Linux apt-dependencies, and Xvfb wrapping of the
pluginval run on Linux are preserved from the previous workflow. The
Xvfb install step has been merged into the main Linux-dependencies step.

## Local smoke-test results (Linux only)

The Linux side of the pipeline was re-exercised on the host machine
(`ubuntu-style` environment) before the impl agent began editing the
editor sources:

- `cmake -B build -DCMAKE_BUILD_TYPE=Release` → **OK**
- `cmake --build build --config Release --target MW160 --parallel` → **OK**
- Resulting `MW160.vst3` bundle is produced at the expected path.

**Warnings surfaced during the local build (not filed as new BACKLOG
entries because they overlap with existing agent findings):**

- `src/dsp/ParameterSmoother.h:37` — `-Wfloat-equal` comparing
  `newTarget == target_`. This is adjacent to the QA-DSP finding about
  `ParameterSmoother` restarting the ramp every block; the warning is a
  code-smell that would be eliminated by the fix QA-DSP proposes.

macOS and Windows builds could not be run locally (no macOS or Windows
host available to the orchestrator). The first run of the new workflow
on CI is where per-platform status will come from.

## Expected CI outcomes (predictive, not observed)

Based on already-filed agent findings, the next CI run of
`build-and-validate.yml` is expected to produce the following
statuses. Any platform that deviates from these expectations is itself a
backlog entry.

### Compilation and Catch2 test suite

| Platform | Expected status | Rationale |
|----------|-----------------|-----------|
| linux | PASS | Verified locally. |
| macos (universal) | PASS, with caveat | Catch2 and JUCE 7 are known to build clean on Apple Silicon; the universal `arm64;x86_64` flag may surface NEON/Intrinsics differences if any exist in the DSP — none are expected in the current code. |
| windows | PASS, with risk | MSVC is stricter about a few C++17 constructs than Clang/GCC, and this CI run is the first time MSVC has seen the codebase. If the build fails with MSVC-specific errors, that is itself a backlog entry (see `QA-CI-001`). |

### pluginval strictness 10

**All three platforms are expected to FAIL pluginval strictness 10** on
at least one test case, because the existing bool-parameter state
restoration bug (QA-CONF-001, HIGH) is reproducible under pluginval
strictness 5–10 and is platform-agnostic. This is the single bug that
gates the rest of CI health and must be fixed before the strictness
bump can be treated as a clean signal.

Other pre-existing QA-CONF findings are ≤ MEDIUM and do not block
pluginval.

## CI-specific findings

### QA-CI-001: Cross-platform CI workflow extended, actual run pending

**Severity:** HIGH
**Owner:** `owner:ci-agent`
**Context:** Task requires a full matrix (linux / macos / windows) with
pluginval strictness 10 and artifact upload. The workflow has been
extended to satisfy all of those requirements, but **the orchestrator
has no ability to run GitHub Actions from the local environment**, so
none of the three jobs have been verified end-to-end in a fresh CI
environment. First run of the extended workflow is required to surface
any MSVC-specific compile errors, macOS universal-binary issues, or
Windows path-handling bugs in the pwsh-shelled pluginval steps.

**Expected:** Next push to any branch triggers `build-and-validate.yml`,
all three jobs attempt to complete, and the orchestrator has a
per-platform pass/fail matrix for the QA report.
**Actual:** Workflow is committed but never run in this pass. Status is
unknown.
**Location:** `.github/workflows/build-and-validate.yml`
**Next step:** Allow the next push/PR to run the workflow and triage the
results into per-platform CRITICAL backlog entries if any platform
fails.

### QA-CI-002: pluginval strictness 10 will fail on all platforms due to QA-CONF-001

**Severity:** CRITICAL (because it gates every CI run after this
workflow lands)
**Owner:** `owner:dev-agent`
**Context:** QA-CONF-001 (bool parameter state restoration) is
reproducible under pluginval strictness 5 and 10. The bump from 5 → 10
in this pass does not create the bug — the bug was already masked at
strictness 5 because pluginval's state-round-trip checks run at every
strictness level. However, the bump to 10 does make this failure
unambiguously a CI red light on every push, which will be noticed
immediately on the first run.
**Expected:** All three CI platforms go green on pluginval strictness 10.
**Actual:** All three platforms will fail pluginval until the
bool-parameter state round-trip is fixed (see QA-CONF-001 for the fix
specification).
**Location:** `src/PluginProcessor.cpp` — `getStateInformation` /
`setStateInformation`
**Linked to:** QA-CONF-001 (parent defect), QA-TM-001 (the
`overEasy` parameter ID migration must land with the bool fix to avoid
double-breaking user state).

### QA-CI-003: Historical BACKLOG reference to `build.yml` is now stale

**Severity:** LOW
**Owner:** `owner:docs-agent`
**Context:** `docs/BACKLOG.md:110` in the ticket MW160-4 history
references `.github/workflows/build.yml` as the file path created by
that ticket. This is a historical statement of what MW160-4 did and is
still accurate *at the time of that ticket's closure*, but a
newly-arrived engineer reading that ticket in isolation may look for
`build.yml` on disk and not find it. Optional: add a forward-reference
comment or let the git log speak for itself.
**Expected:** Either historical reference remains (documented as such)
or is updated with a pointer to the renamed file.
**Actual:** Historical reference is unchanged.
**Location:** `docs/BACKLOG.md:110`

## Trademark scrub status (this subsystem)

- The extended workflow contains **zero** brand references. The only
  product-name tokens in the file are the neutral working name `mw160`
  and the uppercase target name `MW160` emitted by CMake.
- The rename from `build.yml` to `build-and-validate.yml` was done as a
  `git mv` so that file history is preserved; the old filename is no
  longer on disk.
