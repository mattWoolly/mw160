#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/Compressor.h"

using Catch::Matchers::WithinAbs;

static constexpr double kSampleRate = 44100.0;
static constexpr int kSettleSamples = static_cast<int>(kSampleRate * 0.5);
static constexpr int kMeasureSamples = static_cast<int>(kSampleRate * 0.2);

// Helper: generate a sine wave
static std::vector<float> generateSine(float amplitude, float frequency, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double twoPiOverSr = 2.0 * M_PI * frequency / kSampleRate;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = amplitude * static_cast<float>(std::sin(twoPiOverSr * i));
    return buf;
}

// Helper: compute RMS of a buffer region
static float measureRms(const std::vector<float>& buf, int startSample, int numSamples)
{
    double sum = 0.0;
    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        const double s = static_cast<double>(buf[static_cast<size_t>(i)]);
        sum += s * s;
    }
    return static_cast<float>(std::sqrt(sum / numSamples));
}

// Helper: process a sine through a compressor with given mix setting
static std::vector<float> processWithMix(float mixPercent, float threshold, float ratio,
                                         float outputGain_dB, float amplitude,
                                         const std::vector<float>& input)
{
    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(ratio);
    comp.setOutputGain(outputGain_dB);
    comp.setOverEasy(false);
    comp.setMix(mixPercent);

    std::vector<float> output(input.size());
    for (size_t i = 0; i < input.size(); ++i)
        output[i] = comp.processSample(input[i]);
    return output;
}

// ---------------------------------------------------------------------------

TEST_CASE("Mix: 100% -> output equals fully compressed signal",
          "[dsp][mix]")
{
    const float amplitude = 0.4472f; // RMS ~ -10 dBFS, 10 dB above threshold at -20
    const float threshold = -20.0f;
    const float ratio = 4.0f;
    const int totalSamples = kSettleSamples + kMeasureSamples;
    const auto input = generateSine(amplitude, 1000.0f, totalSamples);

    // Process with mix = 100% (default fully wet)
    auto output100 = processWithMix(100.0f, threshold, ratio, 0.0f, amplitude, input);

    // Process with a separate compressor at default mix (should also be 100%)
    mw160::Compressor compRef;
    compRef.prepare(kSampleRate, 512);
    compRef.setThreshold(threshold);
    compRef.setRatio(ratio);
    compRef.setOutputGain(0.0f);
    compRef.setOverEasy(false);
    // Do NOT call setMix — default should be 100%

    std::vector<float> outputDefault(input.size());
    for (size_t i = 0; i < input.size(); ++i)
        outputDefault[i] = compRef.processSample(input[i]);

    // After settling, both should match exactly
    for (int i = kSettleSamples; i < totalSamples; ++i)
        REQUIRE(output100[static_cast<size_t>(i)] == outputDefault[static_cast<size_t>(i)]);

    // Verify compression actually occurred
    const float inputRms = measureRms(input, kSettleSamples, kMeasureSamples);
    const float outputRms = measureRms(output100, kSettleSamples, kMeasureSamples);
    REQUIRE(outputRms < inputRms);
}

TEST_CASE("Mix: 0% -> output equals dry input (bypasses compression)",
          "[dsp][mix]")
{
    const float amplitude = 0.4472f;
    const float threshold = -20.0f;
    const float ratio = 4.0f;
    const int totalSamples = kSettleSamples + kMeasureSamples;
    const auto input = generateSine(amplitude, 1000.0f, totalSamples);

    auto output = processWithMix(0.0f, threshold, ratio, 0.0f, amplitude, input);

    // After settling (smoother ramp), output should match dry input
    for (int i = kSettleSamples; i < totalSamples; ++i)
        REQUIRE_THAT(static_cast<double>(output[static_cast<size_t>(i)]),
                     WithinAbs(static_cast<double>(input[static_cast<size_t>(i)]), 1e-5));
}

TEST_CASE("Mix: 50% -> output is average of dry and compressed",
          "[dsp][mix]")
{
    const float amplitude = 0.4472f;
    const float threshold = -20.0f;
    const float ratio = 4.0f;
    const int totalSamples = kSettleSamples + kMeasureSamples;
    const auto input = generateSine(amplitude, 1000.0f, totalSamples);

    // Get fully compressed output (100% mix)
    auto outputWet = processWithMix(100.0f, threshold, ratio, 0.0f, amplitude, input);
    // Get 50% mix output
    auto output50 = processWithMix(50.0f, threshold, ratio, 0.0f, amplitude, input);

    // After settling, 50% output should be midpoint of dry and compressed
    for (int i = kSettleSamples; i < totalSamples; ++i)
    {
        const float dry = input[static_cast<size_t>(i)];
        const float wet = outputWet[static_cast<size_t>(i)];
        const float expected = (dry + wet) * 0.5f;

        REQUIRE_THAT(static_cast<double>(output50[static_cast<size_t>(i)]),
                     WithinAbs(static_cast<double>(expected), 1e-4));
    }
}

TEST_CASE("Mix: output gain applies equally at any mix setting",
          "[dsp][mix]")
{
    const float amplitude = 0.4472f;
    const float threshold = -20.0f;
    const float ratio = 4.0f;
    const float gainBoost_dB = 6.0f;
    const int totalSamples = kSettleSamples + kMeasureSamples;
    const auto input = generateSine(amplitude, 1000.0f, totalSamples);

    // Process at 50% mix with 0 dB output gain
    auto outputNoGain = processWithMix(50.0f, threshold, ratio, 0.0f, amplitude, input);
    // Process at 50% mix with +6 dB output gain
    auto outputWithGain = processWithMix(50.0f, threshold, ratio, gainBoost_dB, amplitude, input);

    // Measure RMS in the settled region
    const float rmsNoGain = measureRms(outputNoGain, kSettleSamples, kMeasureSamples);
    const float rmsWithGain = measureRms(outputWithGain, kSettleSamples, kMeasureSamples);

    const float rmsNoGain_dB = 20.0f * std::log10(rmsNoGain);
    const float rmsWithGain_dB = 20.0f * std::log10(rmsWithGain);

    // The difference should be ~6 dB
    REQUIRE_THAT(static_cast<double>(rmsWithGain_dB - rmsNoGain_dB),
                 WithinAbs(static_cast<double>(gainBoost_dB), 0.5));
}
