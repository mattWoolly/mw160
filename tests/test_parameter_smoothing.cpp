#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/Compressor.h"

using Catch::Matchers::WithinAbs;

static constexpr double kSampleRate = 44100.0;

// Helper: generate a constant-amplitude sine wave
static std::vector<float> generateSine(double sampleRate, float amplitude,
                                       float frequency, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double twoPiOverSr = 2.0 * M_PI * frequency / sampleRate;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = amplitude * static_cast<float>(std::sin(twoPiOverSr * i));
    return buf;
}

// Measure the instantaneous gain in dB (output/input) for samples above a floor
static float gainDB(float input, float output)
{
    return 20.0f * std::log10(std::fabs(output) / std::fabs(input));
}

TEST_CASE("Parameter smoothing: abrupt threshold change produces no discontinuity > 0.1 dB",
          "[dsp][smoothing]")
{
    // Feed a loud sine well above threshold with active compression.
    // After settling, abruptly change the threshold and verify that the
    // applied gain (output/input ratio in dB) never jumps by more than
    // 0.1 dB between consecutive samples.
    const float amplitude = 0.8f;
    const float freq = 1000.0f;
    const int totalSamples = static_cast<int>(kSampleRate * 0.5);
    const float inputFloor = 0.05f; // skip samples near zero-crossing

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(-10.0f);
    comp.setRatio(4.0f);
    comp.setOutputGain(0.0f);

    auto input = generateSine(kSampleRate, amplitude, freq, totalSamples);

    // Let the compressor settle for the first 200 ms
    const int settleEnd = static_cast<int>(kSampleRate * 0.2);
    std::vector<float> output(static_cast<size_t>(totalSamples));
    for (int i = 0; i < settleEnd; ++i)
        output[static_cast<size_t>(i)] = comp.processSample(input[static_cast<size_t>(i)]);

    // Abruptly change threshold mid-stream
    comp.setThreshold(-30.0f);

    for (int i = settleEnd; i < totalSamples; ++i)
        output[static_cast<size_t>(i)] = comp.processSample(input[static_cast<size_t>(i)]);

    // Measure maximum gain-dB jump between consecutive above-floor samples
    float prevGain_dB = 0.0f;
    bool havePrev = false;
    float maxJump_dB = 0.0f;

    for (int i = settleEnd; i < totalSamples; ++i)
    {
        const float in  = std::fabs(input[static_cast<size_t>(i)]);
        const float out = std::fabs(output[static_cast<size_t>(i)]);

        if (in < inputFloor)
        {
            havePrev = false;
            continue;
        }

        const float curGain_dB = gainDB(input[static_cast<size_t>(i)],
                                        output[static_cast<size_t>(i)]);

        if (havePrev)
        {
            const float jump = std::fabs(curGain_dB - prevGain_dB);
            if (jump > maxJump_dB)
                maxJump_dB = jump;
        }

        prevGain_dB = curGain_dB;
        havePrev = true;
    }

    // Smoothed over 20 ms (~882 samples), per-sample gain change should be small.
    // Without smoothing the threshold jump would cause an instant gain shift of
    // several dB. With smoothing, consecutive-sample gain changes stay under 0.1 dB.
    REQUIRE(maxJump_dB < 0.1f);
}

TEST_CASE("Parameter smoothing: abrupt output gain change produces no click > 1 dB",
          "[dsp][smoothing]")
{
    // Use ratio 1:1 (no compression) so only output gain affects the signal.
    // Abruptly boost output gain and verify per-sample gain jump < 1 dB.
    const float amplitude = 0.3f;
    const float freq = 1000.0f;
    const int totalSamples = static_cast<int>(kSampleRate * 0.5);
    const float inputFloor = 0.02f;

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(0.0f);
    comp.setRatio(1.0f);
    comp.setOutputGain(0.0f);

    auto input = generateSine(kSampleRate, amplitude, freq, totalSamples);
    std::vector<float> output(static_cast<size_t>(totalSamples));

    // Let it settle for 100 ms
    const int settleEnd = static_cast<int>(kSampleRate * 0.1);
    for (int i = 0; i < settleEnd; ++i)
        output[static_cast<size_t>(i)] = comp.processSample(input[static_cast<size_t>(i)]);

    // Abruptly boost output gain by 12 dB
    comp.setOutputGain(12.0f);

    for (int i = settleEnd; i < totalSamples; ++i)
        output[static_cast<size_t>(i)] = comp.processSample(input[static_cast<size_t>(i)]);

    // Measure maximum gain-dB jump
    float prevGain_dB = 0.0f;
    bool havePrev = false;
    float maxJump_dB = 0.0f;

    for (int i = settleEnd; i < totalSamples; ++i)
    {
        if (std::fabs(input[static_cast<size_t>(i)]) < inputFloor)
        {
            havePrev = false;
            continue;
        }

        const float curGain_dB = gainDB(input[static_cast<size_t>(i)],
                                        output[static_cast<size_t>(i)]);

        if (havePrev)
        {
            const float jump = std::fabs(curGain_dB - prevGain_dB);
            if (jump > maxJump_dB)
                maxJump_dB = jump;
        }

        prevGain_dB = curGain_dB;
        havePrev = true;
    }

    // 12 dB gain change spread over 882 samples → ~0.014 dB/sample.
    // Should be well under 1 dB per sample.
    REQUIRE(maxJump_dB < 1.0f);
}

TEST_CASE("Parameter smoothing: after settling, parameter value matches target exactly",
          "[dsp][smoothing]")
{
    // Set up two compressors with different starting parameters.
    // Transition the test compressor to match the reference, then verify
    // their outputs converge after smoothing settles.
    const float amplitude = 0.5f;
    const float freq = 1000.0f;

    // Reference compressor: parameters set from the start
    mw160::Compressor ref;
    ref.prepare(kSampleRate, 512);
    ref.setThreshold(-20.0f);
    ref.setRatio(4.0f);
    ref.setOutputGain(6.0f);

    // Test compressor: starts at different values
    mw160::Compressor test;
    test.prepare(kSampleRate, 512);
    test.setThreshold(0.0f);
    test.setRatio(1.0f);
    test.setOutputGain(0.0f);

    const int totalSamples = static_cast<int>(kSampleRate * 1.5);
    auto input = generateSine(kSampleRate, amplitude, freq, totalSamples);

    // Warm up both for 100 ms
    const int warmup = static_cast<int>(kSampleRate * 0.1);
    for (int i = 0; i < warmup; ++i)
    {
        test.processSample(input[static_cast<size_t>(i)]);
        ref.processSample(input[static_cast<size_t>(i)]);
    }

    // Transition test to match reference targets
    test.setThreshold(-20.0f);
    test.setRatio(4.0f);
    test.setOutputGain(6.0f);

    // Process 1 second more — well past smoothing + detector/ballistics settling
    const int settleLen = static_cast<int>(kSampleRate * 1.0);
    for (int i = warmup; i < warmup + settleLen; ++i)
    {
        test.processSample(input[static_cast<size_t>(i)]);
        ref.processSample(input[static_cast<size_t>(i)]);
    }

    // Compare the last 200 ms — outputs should match
    const int compareStart = warmup + settleLen;
    const int compareLen = static_cast<int>(kSampleRate * 0.2);
    const float inputFloor = 0.02f;
    float maxDiff_dB = 0.0f;

    for (int i = compareStart; i < compareStart + compareLen; ++i)
    {
        const float outTest = test.processSample(input[static_cast<size_t>(i)]);
        const float outRef  = ref.processSample(input[static_cast<size_t>(i)]);

        if (std::fabs(outRef) > inputFloor)
        {
            const float diff_dB = std::fabs(
                20.0f * std::log10(std::fabs(outTest) / std::fabs(outRef)));
            if (diff_dB > maxDiff_dB)
                maxDiff_dB = diff_dB;
        }
    }

    // After smoothing settles, both compressors should produce identical output.
    REQUIRE(maxDiff_dB < 0.1f);
}
