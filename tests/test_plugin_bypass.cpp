// Regression tests for QA-DSP-003 + QA-UX-003 + DESIGN-IMPL-001:
// bypass parameter with sample-accurate crossfade.
//
// These tests exercise the bypass parameter at the plugin-processor level:
// getBypassParameter() API, state round-trip, dry-signal passthrough,
// click-free crossfade, and compressor-state survival during bypass.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{

/// Fill a mono or stereo AudioBuffer with a constant value on all channels.
void fillBuffer(juce::AudioBuffer<float>& buf, float value)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* data = buf.getWritePointer(ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            data[i] = value;
    }
}

/// Push a buffer through processBlock with the given constant sample value.
void pushBlock(MW160Processor& proc, juce::AudioBuffer<float>& buf,
               juce::MidiBuffer& midi, float value)
{
    fillBuffer(buf, value);
    proc.processBlock(buf, midi);
}

/// Return true if all samples in the buffer are approximately equal to the
/// expected value within the given tolerance.
bool allSamplesApprox(const juce::AudioBuffer<float>& buf, float expected,
                      float tolerance)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* data = buf.getReadPointer(ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            if (std::fabs(data[i] - expected) > tolerance)
                return false;
        }
    }
    return true;
}

} // namespace

// ============================================================================
// Test 1: getBypassParameter() returns the correct parameter
// ============================================================================

TEST_CASE("Bypass: getBypassParameter() returns the correct parameter",
          "[plugin][bypass][QA-DSP-003]")
{
    MW160Processor processor;

    auto* bypassParam = processor.getBypassParameter();
    REQUIRE(bypassParam != nullptr);

    auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*>(bypassParam);
    REQUIRE(withID != nullptr);
    REQUIRE(withID->paramID == "bypass");
}

// ============================================================================
// Test 2: Bypass parameter round-trips through state save/restore
// ============================================================================

TEST_CASE("Bypass: parameter round-trips through state save/restore",
          "[plugin][bypass][QA-DSP-003]")
{
    MW160Processor processor;

    // Set bypass to true (engaged).
    auto* bypassParam = processor.apvts.getParameter("bypass");
    REQUIRE(bypassParam != nullptr);
    bypassParam->setValueNotifyingHost(1.0f);

    // Verify it took effect.
    REQUIRE(bypassParam->getValue() >= 0.5f);

    // Save state.
    juce::MemoryBlock memoryBlock;
    processor.getStateInformation(memoryBlock);
    REQUIRE(memoryBlock.getSize() > 0);

    // Mutate bypass to false.
    bypassParam->setValueNotifyingHost(0.0f);
    REQUIRE(bypassParam->getValue() < 0.5f);

    // Restore state.
    processor.setStateInformation(memoryBlock.getData(),
                                  static_cast<int>(memoryBlock.getSize()));

    // Assert bypass is true again.
    REQUIRE(bypassParam->getValue() >= 0.5f);
}

// ============================================================================
// Test 3: Bypass passes dry signal
// ============================================================================

TEST_CASE("Bypass: passes dry signal when engaged",
          "[plugin][bypass][QA-DSP-003]")
{
    MW160Processor processor;
    processor.prepareToPlay(48000.0, 512);

    // Set bypass=true.
    auto* bypassParam = processor.apvts.getParameter("bypass");
    REQUIRE(bypassParam != nullptr);
    bypassParam->setValueNotifyingHost(1.0f);

    // Set compressor to heavy compression so we can tell if bypass works.
    auto* threshParam = processor.apvts.getParameter("threshold");
    auto* ratioParam  = processor.apvts.getParameter("ratio");
    REQUIRE(threshParam != nullptr);
    REQUIRE(ratioParam != nullptr);
    // Threshold -40 dBFS (minimum), ratio 8:1 — this would crush a 0.5 signal.
    threshParam->setValueNotifyingHost(threshParam->convertTo0to1(-40.0f));
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(8.0f));

    const float inputValue = 0.5f;
    const float tolerance = 0.02f; // generous tolerance for smoother convergence

    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;

    // Push enough blocks for the 20ms bypass crossfade to converge.
    // At 48kHz, 512 samples = 10.67ms. 3 blocks = 32ms > 20ms ramp.
    for (int block = 0; block < 5; ++block)
        pushBlock(processor, buf, midi, inputValue);

    // After convergence the output should be approximately equal to the input.
    INFO("Output sample [0][0] = " << buf.getReadPointer(0)[0]);
    INFO("Output sample [0][256] = " << buf.getReadPointer(0)[256]);
    REQUIRE(allSamplesApprox(buf, inputValue, tolerance));
}

// ============================================================================
// Test 4: Bypass crossfade is click-free
// ============================================================================

TEST_CASE("Bypass: crossfade is click-free on toggle",
          "[plugin][bypass][QA-DSP-003]")
{
    MW160Processor processor;
    processor.prepareToPlay(48000.0, 512);

    // Set up some compression so bypass/active sound different.
    auto* bypassParam  = processor.apvts.getParameter("bypass");
    auto* threshParam  = processor.apvts.getParameter("threshold");
    auto* ratioParam   = processor.apvts.getParameter("ratio");
    REQUIRE(bypassParam != nullptr);
    REQUIRE(threshParam != nullptr);
    REQUIRE(ratioParam != nullptr);

    threshParam->setValueNotifyingHost(threshParam->convertTo0to1(-20.0f));
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(4.0f));

    const float inputValue = 0.5f;
    const float maxStep = 0.1f; // max adjacent-sample difference allowed

    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;

    // Warm up in active mode.
    bypassParam->setValueNotifyingHost(0.0f);
    for (int i = 0; i < 5; ++i)
        pushBlock(processor, buf, midi, inputValue);

    // Toggle to bypass and check all transitions.
    bypassParam->setValueNotifyingHost(1.0f);

    float maxDiff = 0.0f;
    float prevSample = buf.getReadPointer(0)[buf.getNumSamples() - 1]; // last sample of prev block

    for (int block = 0; block < 3; ++block)
    {
        fillBuffer(buf, inputValue);
        processor.processBlock(buf, midi);

        const float* data = buf.getReadPointer(0);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float diff = std::fabs(data[i] - prevSample);
            if (diff > maxDiff)
                maxDiff = diff;
            prevSample = data[i];
        }
    }

    INFO("Max adjacent-sample step during bypass-engage crossfade: " << maxDiff);
    REQUIRE(maxDiff < maxStep);

    // Toggle back to active and check again.
    bypassParam->setValueNotifyingHost(0.0f);

    maxDiff = 0.0f;
    prevSample = buf.getReadPointer(0)[buf.getNumSamples() - 1];

    for (int block = 0; block < 3; ++block)
    {
        fillBuffer(buf, inputValue);
        processor.processBlock(buf, midi);

        const float* data = buf.getReadPointer(0);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float diff = std::fabs(data[i] - prevSample);
            if (diff > maxDiff)
                maxDiff = diff;
            prevSample = data[i];
        }
    }

    INFO("Max adjacent-sample step during bypass-disengage crossfade: " << maxDiff);
    REQUIRE(maxDiff < maxStep);
}

// ============================================================================
// Test 5: Compressor state survives bypass
// ============================================================================

TEST_CASE("Bypass: compressor state survives bypass engagement",
          "[plugin][bypass][QA-DSP-003]")
{
    MW160Processor processor;
    processor.prepareToPlay(48000.0, 512);

    auto* bypassParam = processor.apvts.getParameter("bypass");
    auto* threshParam = processor.apvts.getParameter("threshold");
    auto* ratioParam  = processor.apvts.getParameter("ratio");
    REQUIRE(bypassParam != nullptr);
    REQUIRE(threshParam != nullptr);
    REQUIRE(ratioParam != nullptr);

    // Heavy compression.
    threshParam->setValueNotifyingHost(threshParam->convertTo0to1(-30.0f));
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(8.0f));

    const float inputValue = 0.5f;
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;

    // Warm up in active mode for a few blocks to build up GR.
    bypassParam->setValueNotifyingHost(0.0f);
    for (int i = 0; i < 10; ++i)
        pushBlock(processor, buf, midi, inputValue);

    // Engage bypass and push several blocks — compressor DSP should
    // continue processing internally.
    bypassParam->setValueNotifyingHost(1.0f);
    for (int i = 0; i < 10; ++i)
        pushBlock(processor, buf, midi, inputValue);

    // Disengage bypass.
    bypassParam->setValueNotifyingHost(0.0f);

    // Push a few blocks and check that GR is non-zero, proving the
    // compressor state was maintained during bypass.
    for (int i = 0; i < 5; ++i)
        pushBlock(processor, buf, midi, inputValue);

    const float gr = processor.getMeterGainReduction();
    INFO("Gain reduction after disengaging bypass: " << gr << " dB");
    REQUIRE(gr < -1.0f);
}
