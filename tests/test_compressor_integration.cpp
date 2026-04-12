#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/Compressor.h"

using Catch::Matchers::WithinAbs;

static constexpr double kSampleRate = 44100.0;

// Helper: generate a sine wave at a given frequency and amplitude
static std::vector<float> generateSine(double sampleRate, float amplitude,
                                       float frequency, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double twoPiOverSr = 2.0 * M_PI * frequency / sampleRate;
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

// --- Acceptance criteria ---

TEST_CASE("Compressor integration: ratio 1:1 passes audio through unchanged",
          "[dsp][integration]")
{
    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(-20.0f);
    comp.setRatio(1.0f);
    comp.setOutputGain(0.0f);

    const auto input = generateSine(kSampleRate, 0.5f, 1000.0f, 44100);
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i)
        output[i] = comp.processSample(input[i]);

    // After settling (skip first 100 ms for detector warmup), output should match input
    const int settleOffset = static_cast<int>(kSampleRate * 0.1);
    for (int i = settleOffset; i < static_cast<int>(input.size()); ++i)
        REQUIRE_THAT(static_cast<double>(output[static_cast<size_t>(i)]),
                     WithinAbs(static_cast<double>(input[static_cast<size_t>(i)]), 1e-5));
}

TEST_CASE("Compressor integration: sine above threshold at 4:1 compresses correctly",
          "[dsp][integration]")
{
    // Set up a sine whose RMS is 10 dB above threshold.
    // Threshold = -20 dB, target RMS = -10 dB.
    // RMS of sine = amplitude / sqrt(2).
    // amplitude / sqrt(2) = 10^(-10/20) = 0.3162  =>  amplitude ≈ 0.4472
    const float amplitude = 0.4472f;
    const float threshold = -20.0f;
    const float ratio = 4.0f;

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(ratio);
    comp.setOutputGain(0.0f);

    // Process 1 second of audio for detector and ballistics to fully settle
    const int totalSamples = static_cast<int>(kSampleRate);
    const auto input = generateSine(kSampleRate, amplitude, 1000.0f, totalSamples);
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i)
        output[i] = comp.processSample(input[i]);

    // Measure RMS of the last 200 ms (steady state)
    const int measureLen = static_cast<int>(kSampleRate * 0.2);
    const int measureStart = totalSamples - measureLen;

    const float inputRms = measureRms(input, measureStart, measureLen);
    const float outputRms = measureRms(output, measureStart, measureLen);

    const float inputRms_dB = 20.0f * std::log10(inputRms);
    const float outputRms_dB = 20.0f * std::log10(outputRms);

    // Feedback topology: GR = excess * slope / (1 - slope)
    // 10 dB excess at 4:1: slope = 1/4 - 1 = -0.75
    // GR = 10 * (-0.75) / (1 - (-0.75)) = 10 * (-0.75) / 1.75 ≈ -4.286 dB
    const float expectedGR = 10.0f * (1.0f / 4.0f - 1.0f) / (1.0f - (1.0f / 4.0f - 1.0f));
    const float expectedOutputRms_dB = inputRms_dB + expectedGR;

    REQUIRE_THAT(static_cast<double>(outputRms_dB),
                 WithinAbs(static_cast<double>(expectedOutputRms_dB), 1.0));
}

TEST_CASE("Compressor integration: silence in produces silence out",
          "[dsp][integration]")
{
    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(-20.0f);
    comp.setRatio(4.0f);
    comp.setOutputGain(0.0f);

    for (int i = 0; i < 4410; ++i)
        REQUIRE(comp.processSample(0.0f) == 0.0f);
}

TEST_CASE("Compressor integration: output gain boosts signal by expected amount",
          "[dsp][integration]")
{
    // Use ratio 1:1 so no compression, just test output gain.
    const float amplitude = 0.25f;
    const float gainBoost_dB = 6.0f;

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(0.0f);
    comp.setRatio(1.0f);
    comp.setOutputGain(gainBoost_dB);

    const int totalSamples = static_cast<int>(kSampleRate * 0.5);
    const auto input = generateSine(kSampleRate, amplitude, 1000.0f, totalSamples);
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i)
        output[i] = comp.processSample(input[i]);

    // Measure RMS of the last 200 ms
    const int measureLen = static_cast<int>(kSampleRate * 0.2);
    const int measureStart = totalSamples - measureLen;

    const float inputRms = measureRms(input, measureStart, measureLen);
    const float outputRms = measureRms(output, measureStart, measureLen);

    const float inputRms_dB = 20.0f * std::log10(inputRms);
    const float outputRms_dB = 20.0f * std::log10(outputRms);

    // Output should be ~6 dB louder than input
    REQUIRE_THAT(static_cast<double>(outputRms_dB - inputRms_dB),
                 WithinAbs(static_cast<double>(gainBoost_dB), 0.5));
}
