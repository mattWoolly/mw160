#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/VcaSaturation.h"

using Catch::Matchers::WithinAbs;

static constexpr double kSampleRate = 44100.0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Generate a sine wave.  Use an integer multiple of (sampleRate / frequency)
// samples for spectral-leakage-free DFT analysis.
static std::vector<float> generateSine(double sampleRate, float amplitude,
                                       double frequency, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double omega = 2.0 * M_PI * frequency / sampleRate;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = amplitude
            * static_cast<float>(std::sin(omega * i));
    return buf;
}

// Measure the amplitude of a specific frequency component using correlation
// (single-bin DFT / Goertzel-equivalent).
static double measureHarmonicAmplitude(const std::vector<float>& buf,
                                       double sampleRate, double frequency,
                                       int startSample, int numSamples)
{
    double sinSum = 0.0, cosSum = 0.0;
    const double omega = 2.0 * M_PI * frequency / sampleRate;
    for (int i = 0; i < numSamples; ++i)
    {
        const double s = static_cast<double>(
            buf[static_cast<size_t>(startSample + i)]);
        const double phase = omega * (startSample + i);
        sinSum += s * std::sin(phase);
        cosSum += s * std::cos(phase);
    }
    sinSum *= 2.0 / numSamples;
    cosSum *= 2.0 / numSamples;
    return std::sqrt(sinSum * sinSum + cosSum * cosSum);
}

// Compute THD as sqrt(sum of harmonic powers) / fundamental amplitude.
static double computeTHD(const std::vector<float>& buf, double sampleRate,
                         double fundamental, int startSample, int numSamples,
                         int numHarmonics = 5)
{
    const double h1 = measureHarmonicAmplitude(buf, sampleRate, fundamental,
                                               startSample, numSamples);
    if (h1 < 1e-10)
        return 0.0;

    double harmonicPowerSum = 0.0;
    for (int n = 2; n <= numHarmonics + 1; ++n)
    {
        const double hn = measureHarmonicAmplitude(buf, sampleRate,
                                                   fundamental * n,
                                                   startSample, numSamples);
        harmonicPowerSum += hn * hn;
    }
    return std::sqrt(harmonicPowerSum) / h1;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static const mw160::VcaSaturation sat;

TEST_CASE("VcaSaturation: zero input produces zero output", "[dsp][saturation]")
{
    REQUIRE(sat.processSample(0.0f, 0.0f) == 0.0f);
    REQUIRE(sat.processSample(0.0f, -10.0f) == 0.0f);
    REQUIRE(sat.processSample(0.0f, -30.0f) == 0.0f);
    REQUIRE(sat.processSample(0.0f, -60.0f) == 0.0f);
}

TEST_CASE("VcaSaturation: low-level signal has negligible distortion",
          "[dsp][saturation]")
{
    // A very low-level signal should pass through with < 0.01% THD
    // even at moderate gain reduction.
    const float amplitude = 0.01f;
    const float grAmount = -15.0f;
    const double fundamental = 1000.0;

    // 100 complete cycles for clean DFT (44100/1000 * 100 = 4410 samples)
    const int numCycles = 100;
    const int numSamples = static_cast<int>(kSampleRate / fundamental
                                                      * numCycles);

    auto input = generateSine(kSampleRate, amplitude, fundamental, numSamples);
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i)
        output[i] = sat.processSample(input[i], grAmount);

    const double thd = computeTHD(output, kSampleRate, fundamental,
                                  0, numSamples);
    REQUIRE(thd < 0.0001);  // < 0.01%
}

TEST_CASE("VcaSaturation: moderate GR produces measurable distortion",
          "[dsp][saturation]")
{
    // At moderate gain reduction with a typical signal level,
    // THD should be in the 0.05-0.2% range.
    const float amplitude = 0.5f;
    const float grAmount = -15.0f;
    const double fundamental = 1000.0;

    const int numCycles = 100;
    const int numSamples = static_cast<int>(kSampleRate / fundamental
                                                      * numCycles);

    auto input = generateSine(kSampleRate, amplitude, fundamental, numSamples);
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i)
        output[i] = sat.processSample(input[i], grAmount);

    const double thd = computeTHD(output, kSampleRate, fundamental,
                                  0, numSamples);
    REQUIRE(thd > 0.0005);  // > 0.05%
    REQUIRE(thd < 0.002);   // < 0.2%
}

TEST_CASE("VcaSaturation: 2nd harmonic dominates over 3rd",
          "[dsp][saturation]")
{
    // The discrete VCA's NPN/PNP asymmetry produces predominantly
    // even-order harmonics.  2nd harmonic should be significantly
    // larger than 3rd.
    const float amplitude = 0.5f;
    const float grAmount = -20.0f;
    const double fundamental = 1000.0;

    const int numCycles = 100;
    const int numSamples = static_cast<int>(kSampleRate / fundamental
                                                      * numCycles);

    auto input = generateSine(kSampleRate, amplitude, fundamental, numSamples);
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i)
        output[i] = sat.processSample(input[i], grAmount);

    const double h2 = measureHarmonicAmplitude(output, kSampleRate, 2000.0,
                                               0, numSamples);
    const double h3 = measureHarmonicAmplitude(output, kSampleRate, 3000.0,
                                               0, numSamples);

    REQUIRE(h2 > h3 * 3.0);
}

TEST_CASE("VcaSaturation: output level approximately matches input level",
          "[dsp][saturation]")
{
    // Saturation should add coloration, not significant gain change.
    // Output RMS should be within 0.5 dB of input RMS.
    const float amplitude = 0.5f;
    const float grAmount = -20.0f;
    const double fundamental = 1000.0;

    const int numCycles = 100;
    const int numSamples = static_cast<int>(kSampleRate / fundamental
                                                      * numCycles);

    auto input = generateSine(kSampleRate, amplitude, fundamental, numSamples);
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i)
        output[i] = sat.processSample(input[i], grAmount);

    double inputRmsSum = 0.0, outputRmsSum = 0.0;
    for (size_t i = 0; i < input.size(); ++i)
    {
        inputRmsSum += static_cast<double>(input[i]) * input[i];
        outputRmsSum += static_cast<double>(output[i]) * output[i];
    }
    const double inputRms_dB = 10.0 * std::log10(inputRmsSum / numSamples);
    const double outputRms_dB = 10.0 * std::log10(outputRmsSum / numSamples);

    REQUIRE_THAT(outputRms_dB, WithinAbs(inputRms_dB, 0.5));
}
