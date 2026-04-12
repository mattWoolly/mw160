// Regression test for QA-UX-001:
// threshold parameter must be labeled in dBFS, not dBu.
//
// The DSP path (RmsDetector + GainComputer) operates on raw sample
// magnitudes converted directly to dB — i.e. dBFS (0 dBFS = full-scale
// digital sample). The APVTS threshold parameter was historically
// declared with label "dBu", a hardware-era convention implying an
// analog 0-dBu reference around -18 dBFS. That mismatch is a ~18 dB
// semantic lie: a user dialling threshold = 0 expects compression
// onset at -18 dBFS but the code starts compressing at 0 dBFS.
//
// This regression test locks in the corrected label ("dBFS") and also
// audits the factory preset threshold values so that a silent future
// rebalance cannot reintroduce the mismatch.
//
// Audit conclusion (recorded here as a regression lock):
//   The seven factory threshold values are already musically plausible
//   in the dBFS frame (Kick Punch -10 dBFS, Drum Bus Glue -12 dBFS,
//   Parallel Smash -20 dBFS, etc. — all sit at sensible levels for
//   modern tracked program material). No preset values are shifted by
//   this ticket; the fix is the label string and the docs.
//
// See docs/REFERENCE.md §2.1 and qa-findings/ for the full rationale.

#include <catch2/catch_test_macros.hpp>

#include "FactoryPresets.h"
#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

TEST_CASE("Threshold parameter is labeled in dBFS, not dBu",
          "[plugin][parameters][QA-UX-001]")
{
    MW160Processor processor;

    auto* parameter = processor.apvts.getParameter("threshold");
    REQUIRE(parameter != nullptr);

    // The DSP works in dBFS — the label must match.
    const juce::String label = parameter->getLabel();
    REQUIRE(label == "dBFS");

    // Belt-and-suspenders: the historical analog-reference label must
    // be gone regardless of capitalisation.
    REQUIRE_FALSE(label.containsIgnoreCase("dBu"));
}

TEST_CASE("Factory preset thresholds locked to audited dBFS values",
          "[plugin][parameters][presets][QA-UX-001]")
{
    // These are the exact values audited during QA-UX-001 and judged
    // already-correct in the dBFS frame (no rebalance). Locking them
    // here guards against a silent future shift.
    struct Expected { const char* name; float threshold; };
    constexpr Expected expected[] = {
        { "Kick Punch",      -10.0f },
        { "Snare Snap",       -8.0f },
        { "Drum Bus Glue",   -12.0f },
        { "Bass Control",    -14.0f },
        { "Parallel Smash",  -20.0f },
        { "Gentle Leveling",  -8.0f },
        { "Brick Wall",       -6.0f },
    };

    REQUIRE(mw160::kNumFactoryPresets
            == static_cast<int>(sizeof(expected) / sizeof(expected[0])));

    for (int i = 0; i < mw160::kNumFactoryPresets; ++i)
    {
        const auto& preset = mw160::kFactoryPresets[i];
        INFO("Preset index " << i << ": " << preset.name);
        REQUIRE(std::string(preset.name) == expected[i].name);
        CHECK(preset.threshold == expected[i].threshold);
    }
}
