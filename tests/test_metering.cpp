#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/Compressor.h"

using Catch::Matchers::WithinAbs;

static constexpr double kSampleRate = 44100.0;

static std::vector<float> generateSine(double sampleRate, float amplitude,
                                       float frequency, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double twoPiOverSr = 2.0 * M_PI * frequency / sampleRate;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = amplitude * static_cast<float>(std::sin(twoPiOverSr * i));
    return buf;
}

TEST_CASE("Metering: GR reports correct gain reduction for known compression",
          "[dsp][metering]")
{
    // Sine at -10 dBFS RMS, threshold -20, ratio 4:1
    // Feedback GR = 10 * (-0.75) / 1.75 ≈ -4.286 dB
    const float amplitude = 0.4472f;  // RMS ≈ -10 dBFS
    const float threshold = -20.0f;
    const float ratio = 4.0f;
    const float slope = 1.0f / ratio - 1.0f;
    const float expectedGR = 10.0f * slope / (1.0f - slope);  // ≈ -4.286

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(ratio);
    comp.setOutputGain(0.0f);

    // Process 1 second to fully settle
    const int totalSamples = static_cast<int>(kSampleRate);
    const auto input = generateSine(kSampleRate, amplitude, 1000.0f, totalSamples);

    for (size_t i = 0; i < input.size(); ++i)
        comp.processSample(input[i]);

    const float gr = comp.getLastGainReduction_dB();
    REQUIRE(gr < 0.0f);
    REQUIRE_THAT(static_cast<double>(gr),
                 WithinAbs(static_cast<double>(expectedGR), 1.5));
}

TEST_CASE("Metering: GR is zero when signal is below threshold",
          "[dsp][metering]")
{
    // Sine at -30 dBFS RMS, threshold -10 → no compression
    const float amplitude = 0.0447f;  // RMS ≈ -30 dBFS
    const float threshold = -10.0f;

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(4.0f);
    comp.setOutputGain(0.0f);

    const int totalSamples = static_cast<int>(kSampleRate * 0.5);
    const auto input = generateSine(kSampleRate, amplitude, 1000.0f, totalSamples);

    for (size_t i = 0; i < input.size(); ++i)
        comp.processSample(input[i]);

    const float gr = comp.getLastGainReduction_dB();
    REQUIRE_THAT(static_cast<double>(gr), WithinAbs(0.0, 0.1));
}

TEST_CASE("Metering: GR is zero at ratio 1:1",
          "[dsp][metering]")
{
    const float amplitude = 0.5f;

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(-20.0f);
    comp.setRatio(1.0f);
    comp.setOutputGain(0.0f);

    const int totalSamples = static_cast<int>(kSampleRate * 0.5);
    const auto input = generateSine(kSampleRate, amplitude, 1000.0f, totalSamples);

    for (size_t i = 0; i < input.size(); ++i)
        comp.processSample(input[i]);

    const float gr = comp.getLastGainReduction_dB();
    REQUIRE_THAT(static_cast<double>(gr), WithinAbs(0.0, 0.01));
}

TEST_CASE("Metering: GR returns towards zero after signal stops",
          "[dsp][metering]")
{
    const float amplitude = 0.4472f;

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(-20.0f);
    comp.setRatio(4.0f);
    comp.setOutputGain(0.0f);

    // Feed signal to build up GR
    const int signalSamples = static_cast<int>(kSampleRate * 0.5);
    const auto input = generateSine(kSampleRate, amplitude, 1000.0f, signalSamples);
    for (size_t i = 0; i < input.size(); ++i)
        comp.processSample(input[i]);

    REQUIRE(comp.getLastGainReduction_dB() < -1.0f);

    // Feed silence — GR should release towards zero
    const int silenceSamples = static_cast<int>(kSampleRate * 2.0);
    for (int i = 0; i < silenceSamples; ++i)
        comp.processSample(0.0f);

    const float grAfterSilence = comp.getLastGainReduction_dB();
    REQUIRE_THAT(static_cast<double>(grAfterSilence), WithinAbs(0.0, 0.5));
}

TEST_CASE("Metering: GR initial state is zero",
          "[dsp][metering]")
{
    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);

    REQUIRE(comp.getLastGainReduction_dB() == 0.0f);
}
