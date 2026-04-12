// Regression test for QA-CONF-001: bool parameter state restoration.
//
// Verifies that MW160Processor::getStateInformation /
// setStateInformation round-trip every parameter (4 floats and 2 bools)
// without leaving stale or unsnapped values in the parameter objects.
//
// The bug observed by pluginval at strictness 10 is that
// AudioProcessorValueTreeState::replaceState only re-drives a parameter
// through setValueNotifyingHost when the value read from the restored
// ValueTree differs from the adapter's currently stored unnormalised
// value. AudioParameterBool's range snaps the adapter's atomic to 0/1,
// but its own internal `value` atomic stores whatever raw float was
// passed to setValue. As a result, after a save/restore round-trip a
// bool parameter's internal value can remain stuck at the raw unsnapped
// float that was last written, while the APVTS-adapter atomic looks
// snapped. pluginval reads parameter->getValue() and observes the
// stale raw float (e.g. 0.163635, 0.435694, 0.572227), failing the
// round-trip check.
//
// The fix is to iterate every parameter after replaceState and
// explicitly re-apply each via setValueNotifyingHost, forcing bool
// parameters to re-snap through the public API.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

using Catch::Matchers::WithinAbs;

namespace
{
constexpr float kFloatTolerance = 1.0e-4f;

// Snapshot of every parameter's normalised getValue(). This is the same
// surface pluginval inspects: it queries the AudioProcessorParameter
// objects directly, not the APVTS adapter atomics.
struct ParameterSnapshot
{
    float threshold;
    float ratio;
    float outputGain;
    float overEasy;
    float stereoLink;
    float mix;
};

ParameterSnapshot snapshotParameterValues(const MW160Processor& processor)
{
    auto get = [&processor](const juce::String& id) {
        const auto* param = processor.apvts.getParameter(id);
        REQUIRE(param != nullptr);
        return param->getValue();
    };

    ParameterSnapshot s;
    s.threshold  = get("threshold");
    s.ratio      = get("ratio");
    s.outputGain = get("outputGain");
    s.overEasy   = get("overEasy");
    s.stereoLink = get("stereoLink");
    s.mix        = get("mix");
    return s;
}

void writeParameterValue(MW160Processor& processor,
                         const juce::String& id,
                         float normalised)
{
    auto* param = processor.apvts.getParameter(id);
    REQUIRE(param != nullptr);
    param->setValueNotifyingHost(normalised);
}
} // namespace

TEST_CASE("Plugin processor restores all parameters via state round-trip",
          "[plugin][state][regression][QA-CONF-001]")
{
    MW160Processor processor;

    // Pick distinctive non-default normalised values for every
    // parameter. The bools start at the opposite of their defaults so
    // that any failure to re-apply is visible.
    writeParameterValue(processor, "threshold",  0.25f);
    writeParameterValue(processor, "ratio",      0.50f);
    writeParameterValue(processor, "outputGain", 0.75f);
    writeParameterValue(processor, "overEasy",   1.0f);  // default false
    writeParameterValue(processor, "stereoLink", 0.0f);  // default true
    writeParameterValue(processor, "mix",        0.40f);

    const auto captured = snapshotParameterValues(processor);

    // Persist state.
    juce::MemoryBlock memoryBlock;
    processor.getStateInformation(memoryBlock);
    REQUIRE(memoryBlock.getSize() > 0);

    // Mutate every parameter to a different value.
    writeParameterValue(processor, "threshold",  0.90f);
    writeParameterValue(processor, "ratio",      0.10f);
    writeParameterValue(processor, "outputGain", 0.20f);
    writeParameterValue(processor, "overEasy",   0.0f);
    writeParameterValue(processor, "stereoLink", 1.0f);
    writeParameterValue(processor, "mix",        0.95f);

    // Restore.
    processor.setStateInformation(memoryBlock.getData(),
                                  static_cast<int>(memoryBlock.getSize()));

    const auto restored = snapshotParameterValues(processor);

    REQUIRE_THAT(restored.threshold,  WithinAbs(captured.threshold,  kFloatTolerance));
    REQUIRE_THAT(restored.ratio,      WithinAbs(captured.ratio,      kFloatTolerance));
    REQUIRE_THAT(restored.outputGain, WithinAbs(captured.outputGain, kFloatTolerance));
    REQUIRE_THAT(restored.mix,        WithinAbs(captured.mix,        kFloatTolerance));
    REQUIRE_THAT(restored.overEasy,   WithinAbs(captured.overEasy,   kFloatTolerance));
    REQUIRE_THAT(restored.stereoLink, WithinAbs(captured.stereoLink, kFloatTolerance));
}

TEST_CASE("Plugin processor snaps bool parameters on restore even when host wrote raw float",
          "[plugin][state][regression][QA-CONF-001]")
{
    // This case reproduces the pluginval failure mode directly. pluginval
    // at strictness 10 fuzzes parameters with raw normalised floats such
    // as 0.163635 / 0.435694 / 0.572227 via setValueNotifyingHost on bool
    // parameters. AudioParameterBool::setValue stores those raw floats
    // verbatim in its internal value atomic without snapping. The APVTS
    // adapter's atomic does snap (because the bool parameter's
    // NormalisableRange has step 1.0), so the tree saved by
    // getStateInformation contains the snapped 0.0 / 1.0 value. On
    // restore, the adapter sees value_from_tree == unnormalisedValue and
    // early-returns without calling setValueNotifyingHost, leaving the
    // parameter object's internal raw float in place. pluginval then
    // reads parameter->getValue() and observes the stale unsnapped float.

    MW160Processor processor;

    auto* overEasyParam   = processor.apvts.getParameter("overEasy");
    auto* stereoLinkParam = processor.apvts.getParameter("stereoLink");
    REQUIRE(overEasyParam   != nullptr);
    REQUIRE(stereoLinkParam != nullptr);

    // Step 1: write raw unsnapped values via the public host API.
    overEasyParam  ->setValueNotifyingHost(0.163635f);
    stereoLinkParam->setValueNotifyingHost(0.572227f);

    // Sanity check: the parameter object stores the raw values, but the
    // APVTS adapter's atomic snaps them through the bool's range.
    REQUIRE_THAT(overEasyParam  ->getValue(), WithinAbs(0.163635f, kFloatTolerance));
    REQUIRE_THAT(stereoLinkParam->getValue(), WithinAbs(0.572227f, kFloatTolerance));

    // Step 2: capture state. The tree records the snapped values, so
    // captured snapshot reads the parameter's stale raw floats while
    // the saved bytes contain 0.0f / 1.0f.
    juce::MemoryBlock memoryBlock;
    processor.getStateInformation(memoryBlock);
    REQUIRE(memoryBlock.getSize() > 0);

    // Step 3: mutate the bool parameters with raw unsnapped values
    // chosen so that the adapter's snapped atomic still matches what is
    // in the saved tree. This is what makes the early-return path in
    // ParameterAdapter::setDenormalisedValue fire on restore.
    //
    // overEasy: tree saved 0.0f. Choose a value that snaps to 0.0f.
    // stereoLink: tree saved 1.0f. Choose a value that snaps to 1.0f.
    overEasyParam  ->setValueNotifyingHost(0.435694f); // snaps to 0
    stereoLinkParam->setValueNotifyingHost(0.617421f); // snaps to 1

    processor.setStateInformation(memoryBlock.getData(),
                                  static_cast<int>(memoryBlock.getSize()));

    // Step 4: assert the parameter object's internal value is properly
    // snapped after restore. Without the fix, overEasy will still be
    // 0.435694 because the adapter early-returned and never called
    // setValueNotifyingHost on the parameter object.
    const float overEasyAfter   = overEasyParam  ->getValue();
    const float stereoLinkAfter = stereoLinkParam->getValue();

    INFO("overEasy after restore: " << overEasyAfter);
    INFO("stereoLink after restore: " << stereoLinkAfter);

    REQUIRE((overEasyAfter   == 0.0f || overEasyAfter   == 1.0f));
    REQUIRE((stereoLinkAfter == 0.0f || stereoLinkAfter == 1.0f));
}
