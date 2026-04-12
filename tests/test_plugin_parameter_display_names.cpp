// Regression test for DESIGN-REVIEW-006 / QA-UX-004:
// neutral parameter display name for the knee-mode bool.
//
// The AudioParameterBool that controls compression knee character is
// exposed to every VST3 / AU host's parameter automation surface via its
// APVTS display-name string. The display name must be a neutral,
// trademark-free label that matches the project's canonical vocabulary
// ("Knee") as defined in docs/VISUAL_SPEC.md §0 and §7.4.
//
// The APVTS parameter ID "softKnee" is intentionally NOT tested here;
// it is preserved for state-restore compatibility and will be renamed
// under a separate migration ticket (QA-TM-001 / QA-CONF-005).

#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

TEST_CASE("Knee parameter display name is neutral and trademark-free",
          "[plugin][parameters][trademark][DESIGN-REVIEW-006]")
{
    MW160Processor processor;

    auto* parameter = processor.apvts.getParameter("softKnee");
    REQUIRE(parameter != nullptr);

    // The display name must be the project-canonical neutral label.
    const juce::String displayName = parameter->getName(256);
    REQUIRE(displayName == "Knee");

    // Belt-and-suspenders guard: the name must NOT contain the trademark
    // string regardless of capitalisation.
    REQUIRE_FALSE(displayName.containsIgnoreCase("OverEasy"));
}
