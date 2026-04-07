#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/Compressor.h"

using Catch::Matchers::WithinAbs;

static constexpr double kSampleRate = 44100.0;
static constexpr int kSettleSeconds = 1;
static constexpr int kSettleSamples = static_cast<int>(kSampleRate * kSettleSeconds);
static constexpr int kMeasureLen = static_cast<int>(kSampleRate * 0.2);
static constexpr int kMeasureStart = kSettleSamples - kMeasureLen;

// ---------- helpers ----------

static std::vector<float> generateSine(float amplitude, float frequency,
                                       int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double w = 2.0 * M_PI * frequency / kSampleRate;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = amplitude * static_cast<float>(std::sin(w * i));
    return buf;
}

static std::vector<float> generateSquare(float amplitude, float frequency,
                                         int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double period = kSampleRate / frequency;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = (std::fmod(i, period) < period / 2.0) ? amplitude : -amplitude;
    return buf;
}

/// Amplitude for a sine whose RMS equals the given dBFS level.
static float sineAmplitudeForRms_dBFS(float rms_dBFS)
{
    // RMS of sine = amplitude / sqrt(2)
    // => amplitude = 10^(rms_dBFS/20) * sqrt(2)
    return std::pow(10.0f, rms_dBFS / 20.0f) * std::sqrt(2.0f);
}

static float measureRms(const std::vector<float>& buf, int start, int len)
{
    double sum = 0.0;
    for (int i = start; i < start + len; ++i)
    {
        const double s = static_cast<double>(buf[static_cast<size_t>(i)]);
        sum += s * s;
    }
    return static_cast<float>(std::sqrt(sum / len));
}

static float toDb(float linear)
{
    return 20.0f * std::log10(linear);
}

/// Process a full buffer through a compressor instance and return the output.
static std::vector<float> processBuffer(mw160::Compressor& comp,
                                        const std::vector<float>& input)
{
    std::vector<float> output(input.size());
    for (size_t i = 0; i < input.size(); ++i)
        output[i] = comp.processSample(input[i]);
    return output;
}

/// Compute the theoretical hard-knee gain reduction for a given excess.
static float theoreticalGR(float excess_dB, float ratio)
{
    if (excess_dB <= 0.0f) return 0.0f;
    return excess_dB * (1.0f / ratio - 1.0f);
}

// ==========================================================================
// Steady-state gain reduction at multiple signal levels
// ==========================================================================

TEST_CASE("Gain staging: steady-state GR at 0 dBFS RMS, threshold -20, ratio 4:1",
          "[dsp][calibration]")
{
    // 0 dBFS RMS input, threshold -20 dB => 20 dB excess
    // GR = 20 * (1/4 - 1) = -15 dB
    const float rms_dBFS = 0.0f;
    const float threshold = -20.0f;
    const float ratio = 4.0f;
    const float expectedGR = theoreticalGR(rms_dBFS - threshold, ratio);  // -15.0

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(ratio);
    comp.setOutputGain(0.0f);

    const auto input = generateSine(sineAmplitudeForRms_dBFS(rms_dBFS), 1000.0f, kSettleSamples);
    const auto output = processBuffer(comp, input);

    const float inputRms_dB = toDb(measureRms(input, kMeasureStart, kMeasureLen));
    const float outputRms_dB = toDb(measureRms(output, kMeasureStart, kMeasureLen));
    const float measuredGR = outputRms_dB - inputRms_dB;

    REQUIRE_THAT(static_cast<double>(measuredGR),
                 WithinAbs(static_cast<double>(expectedGR), 0.5));
}

TEST_CASE("Gain staging: steady-state GR at -6 dBFS RMS, threshold -20, ratio 4:1",
          "[dsp][calibration]")
{
    // -6 dBFS RMS, threshold -20 => 14 dB excess
    // GR = 14 * (1/4 - 1) = -10.5 dB
    const float rms_dBFS = -6.0f;
    const float threshold = -20.0f;
    const float ratio = 4.0f;
    const float expectedGR = theoreticalGR(rms_dBFS - threshold, ratio);  // -10.5

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(ratio);
    comp.setOutputGain(0.0f);

    const auto input = generateSine(sineAmplitudeForRms_dBFS(rms_dBFS), 1000.0f, kSettleSamples);
    const auto output = processBuffer(comp, input);

    const float inputRms_dB = toDb(measureRms(input, kMeasureStart, kMeasureLen));
    const float outputRms_dB = toDb(measureRms(output, kMeasureStart, kMeasureLen));
    const float measuredGR = outputRms_dB - inputRms_dB;

    REQUIRE_THAT(static_cast<double>(measuredGR),
                 WithinAbs(static_cast<double>(expectedGR), 0.5));
}

TEST_CASE("Gain staging: steady-state GR at -12 dBFS RMS, threshold -20, ratio 4:1",
          "[dsp][calibration]")
{
    // -12 dBFS RMS, threshold -20 => 8 dB excess
    // GR = 8 * (1/4 - 1) = -6.0 dB
    const float rms_dBFS = -12.0f;
    const float threshold = -20.0f;
    const float ratio = 4.0f;
    const float expectedGR = theoreticalGR(rms_dBFS - threshold, ratio);  // -6.0

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(ratio);
    comp.setOutputGain(0.0f);

    const auto input = generateSine(sineAmplitudeForRms_dBFS(rms_dBFS), 1000.0f, kSettleSamples);
    const auto output = processBuffer(comp, input);

    const float inputRms_dB = toDb(measureRms(input, kMeasureStart, kMeasureLen));
    const float outputRms_dB = toDb(measureRms(output, kMeasureStart, kMeasureLen));
    const float measuredGR = outputRms_dB - inputRms_dB;

    REQUIRE_THAT(static_cast<double>(measuredGR),
                 WithinAbs(static_cast<double>(expectedGR), 0.5));
}

TEST_CASE("Gain staging: no GR at -20 dBFS RMS when threshold is -10",
          "[dsp][calibration]")
{
    // -20 dBFS RMS, threshold -10 => signal below threshold, GR = 0
    const float rms_dBFS = -20.0f;
    const float threshold = -10.0f;

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(4.0f);
    comp.setOutputGain(0.0f);

    const auto input = generateSine(sineAmplitudeForRms_dBFS(rms_dBFS), 1000.0f, kSettleSamples);
    const auto output = processBuffer(comp, input);

    const float inputRms_dB = toDb(measureRms(input, kMeasureStart, kMeasureLen));
    const float outputRms_dB = toDb(measureRms(output, kMeasureStart, kMeasureLen));
    const float measuredGR = outputRms_dB - inputRms_dB;

    REQUIRE_THAT(static_cast<double>(measuredGR), WithinAbs(0.0, 0.1));
}

// ==========================================================================
// Specific acceptance criterion from the ticket
// ==========================================================================

TEST_CASE("Gain staging: threshold 0 dB + ratio 4:1 + input +10 dBFS RMS => 7.5 dB GR",
          "[dsp][calibration]")
{
    // Input at +10 dBFS RMS, threshold 0 dB => 10 dB excess
    // GR = 10 * (1/4 - 1) = -7.5 dB
    const float rms_dBFS = 10.0f;
    const float threshold = 0.0f;
    const float ratio = 4.0f;

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(ratio);
    comp.setOutputGain(0.0f);

    const auto input = generateSine(sineAmplitudeForRms_dBFS(rms_dBFS), 1000.0f, kSettleSamples);
    const auto output = processBuffer(comp, input);

    const float inputRms_dB = toDb(measureRms(input, kMeasureStart, kMeasureLen));
    const float outputRms_dB = toDb(measureRms(output, kMeasureStart, kMeasureLen));
    const float measuredGR = outputRms_dB - inputRms_dB;

    REQUIRE_THAT(static_cast<double>(measuredGR), WithinAbs(-7.5, 0.5));
}

// ==========================================================================
// Output gain is purely additive (independent of threshold and ratio)
// ==========================================================================

TEST_CASE("Gain staging: output gain is independent of compression settings",
          "[dsp][calibration]")
{
    const float rms_dBFS = -6.0f;
    const float amplitude = sineAmplitudeForRms_dBFS(rms_dBFS);

    struct Config
    {
        float threshold;
        float ratio;
    };
    const Config configs[] = {
        { -20.0f, 4.0f },   // moderate compression
        { -10.0f, 8.0f },   // heavy compression, different ratio
        { -30.0f, 2.0f },   // light compression
    };

    const float outputGains_dB[] = { 0.0f, 6.0f, 12.0f, -6.0f };

    for (const auto& cfg : configs)
    {
        INFO("threshold=" << cfg.threshold << " ratio=" << cfg.ratio);

        // First, measure baseline (0 dB output gain)
        mw160::Compressor compBaseline;
        compBaseline.prepare(kSampleRate, 512);
        compBaseline.setThreshold(cfg.threshold);
        compBaseline.setRatio(cfg.ratio);
        compBaseline.setOutputGain(0.0f);

        const auto input = generateSine(amplitude, 1000.0f, kSettleSamples);
        const auto outBaseline = processBuffer(compBaseline, input);
        const float baselineRms_dB = toDb(measureRms(outBaseline, kMeasureStart, kMeasureLen));

        for (float gainBoost_dB : outputGains_dB)
        {
            if (gainBoost_dB == 0.0f) continue;

            INFO("output gain=" << gainBoost_dB);

            mw160::Compressor comp;
            comp.prepare(kSampleRate, 512);
            comp.setThreshold(cfg.threshold);
            comp.setRatio(cfg.ratio);
            comp.setOutputGain(gainBoost_dB);

            const auto output = processBuffer(comp, input);
            const float outputRms_dB = toDb(measureRms(output, kMeasureStart, kMeasureLen));

            // The difference from baseline should equal the output gain setting
            const float actualDelta = outputRms_dB - baselineRms_dB;
            REQUIRE_THAT(static_cast<double>(actualDelta),
                         WithinAbs(static_cast<double>(gainBoost_dB), 0.5));
        }
    }
}

TEST_CASE("Gain staging: output gain does not change gain reduction amount",
          "[dsp][calibration]")
{
    const float rms_dBFS = -6.0f;
    const float amplitude = sineAmplitudeForRms_dBFS(rms_dBFS);
    const float threshold = -20.0f;
    const float ratio = 4.0f;

    // Measure GR at 0 dB output gain
    mw160::Compressor compRef;
    compRef.prepare(kSampleRate, 512);
    compRef.setThreshold(threshold);
    compRef.setRatio(ratio);
    compRef.setOutputGain(0.0f);

    const auto input = generateSine(amplitude, 1000.0f, kSettleSamples);
    processBuffer(compRef, input);
    const float grRef = compRef.getLastGainReduction_dB();

    // Now test with various output gains — GR meter should read the same
    for (float gainBoost : { 6.0f, 12.0f, -6.0f })
    {
        INFO("output gain=" << gainBoost);

        mw160::Compressor comp;
        comp.prepare(kSampleRate, 512);
        comp.setThreshold(threshold);
        comp.setRatio(ratio);
        comp.setOutputGain(gainBoost);

        processBuffer(comp, input);
        const float gr = comp.getLastGainReduction_dB();

        REQUIRE_THAT(static_cast<double>(gr),
                     WithinAbs(static_cast<double>(grRef), 0.1));
    }
}

// ==========================================================================
// RMS detector tracks energy, not peaks
// ==========================================================================

TEST_CASE("Gain staging: RMS detector tracks energy (sine vs square at same RMS)",
          "[dsp][calibration]")
{
    // A sine with amplitude A has RMS = A / sqrt(2).
    // A square wave with amplitude B has RMS = B.
    // For same RMS: B = A / sqrt(2).
    // Both should produce the same steady-state GR.
    const float sineAmplitude = 0.8f;
    const float sineRms = sineAmplitude / std::sqrt(2.0f);
    const float squareAmplitude = sineRms;  // Same RMS

    const float threshold = -10.0f;
    const float ratio = 4.0f;

    // Process sine
    mw160::Compressor compSine;
    compSine.prepare(kSampleRate, 512);
    compSine.setThreshold(threshold);
    compSine.setRatio(ratio);
    compSine.setOutputGain(0.0f);

    const auto sineInput = generateSine(sineAmplitude, 1000.0f, kSettleSamples);
    const auto sineOutput = processBuffer(compSine, sineInput);
    const float sineGR = compSine.getLastGainReduction_dB();

    // Process square
    mw160::Compressor compSquare;
    compSquare.prepare(kSampleRate, 512);
    compSquare.setThreshold(threshold);
    compSquare.setRatio(ratio);
    compSquare.setOutputGain(0.0f);

    const auto squareInput = generateSquare(squareAmplitude, 1000.0f, kSettleSamples);
    const auto squareOutput = processBuffer(compSquare, squareInput);
    const float squareGR = compSquare.getLastGainReduction_dB();

    // Both should have the same RMS-based GR (within 0.5 dB)
    REQUIRE_THAT(static_cast<double>(sineGR),
                 WithinAbs(static_cast<double>(squareGR), 0.5));

    // Both should actually be compressing (not zero)
    REQUIRE(sineGR < -1.0f);
    REQUIRE(squareGR < -1.0f);
}

// ==========================================================================
// Metering accuracy: reported GR matches actual level difference
// ==========================================================================

TEST_CASE("Gain staging: metered GR matches actual output level difference",
          "[dsp][calibration]")
{
    struct TestPoint
    {
        float rms_dBFS;
        float threshold;
        float ratio;
    };
    const TestPoint points[] = {
        { 0.0f,  -20.0f, 4.0f },   // 20 dB excess, GR = -15.0
        { -6.0f, -20.0f, 4.0f },   // 14 dB excess, GR = -10.5
        { -6.0f, -20.0f, 8.0f },   // 14 dB excess, GR = -12.25
        { 10.0f,   0.0f, 4.0f },   // 10 dB excess, GR = -7.5
    };

    for (const auto& pt : points)
    {
        INFO("rms=" << pt.rms_dBFS << " threshold=" << pt.threshold
                    << " ratio=" << pt.ratio);

        mw160::Compressor comp;
        comp.prepare(kSampleRate, 512);
        comp.setThreshold(pt.threshold);
        comp.setRatio(pt.ratio);
        comp.setOutputGain(0.0f);

        const auto input = generateSine(sineAmplitudeForRms_dBFS(pt.rms_dBFS),
                                        1000.0f, kSettleSamples);
        const auto output = processBuffer(comp, input);

        // Metered GR
        const float meteredGR = comp.getLastGainReduction_dB();

        // Actual GR from measured levels
        const float inputRms_dB = toDb(measureRms(input, kMeasureStart, kMeasureLen));
        const float outputRms_dB = toDb(measureRms(output, kMeasureStart, kMeasureLen));
        const float actualGR = outputRms_dB - inputRms_dB;

        // Metered value should match actual level difference within 1 dB
        // (metered GR is instantaneous last-sample; actual GR is RMS over 200 ms)
        REQUIRE_THAT(static_cast<double>(meteredGR),
                     WithinAbs(static_cast<double>(actualGR), 1.0));
    }
}

// ==========================================================================
// Multiple ratios at fixed input/threshold
// ==========================================================================

TEST_CASE("Gain staging: correct GR across ratio range",
          "[dsp][calibration]")
{
    const float rms_dBFS = -6.0f;
    const float threshold = -20.0f;
    const float excess = rms_dBFS - threshold;  // 14 dB

    struct RatioTest
    {
        float ratio;
        float expectedGR;  // excess * (1/ratio - 1)
    };
    const RatioTest tests[] = {
        { 2.0f,  excess * (1.0f / 2.0f  - 1.0f) },   // -7.0
        { 4.0f,  excess * (1.0f / 4.0f  - 1.0f) },   // -10.5
        { 8.0f,  excess * (1.0f / 8.0f  - 1.0f) },   // -12.25
        { 20.0f, excess * (1.0f / 20.0f - 1.0f) },   // -13.3
        { 60.0f, excess * (1.0f / 60.0f - 1.0f) },   // -13.77 (infinity:1)
    };

    for (const auto& t : tests)
    {
        INFO("ratio=" << t.ratio << " expectedGR=" << t.expectedGR);

        mw160::Compressor comp;
        comp.prepare(kSampleRate, 512);
        comp.setThreshold(threshold);
        comp.setRatio(t.ratio);
        comp.setOutputGain(0.0f);

        const auto input = generateSine(sineAmplitudeForRms_dBFS(rms_dBFS),
                                        1000.0f, kSettleSamples);
        const auto output = processBuffer(comp, input);

        const float inputRms_dB = toDb(measureRms(input, kMeasureStart, kMeasureLen));
        const float outputRms_dB = toDb(measureRms(output, kMeasureStart, kMeasureLen));
        const float measuredGR = outputRms_dB - inputRms_dB;

        REQUIRE_THAT(static_cast<double>(measuredGR),
                     WithinAbs(static_cast<double>(t.expectedGR), 0.5));
    }
}

// ==========================================================================
// 96 kHz sample rate produces same steady-state GR
// ==========================================================================

TEST_CASE("Gain staging: consistent GR at 96 kHz",
          "[dsp][calibration]")
{
    const double sr96 = 96000.0;
    const int settleSamples96 = static_cast<int>(sr96 * kSettleSeconds);
    const int measureLen96 = static_cast<int>(sr96 * 0.2);
    const int measureStart96 = settleSamples96 - measureLen96;

    const float rms_dBFS = -6.0f;
    const float threshold = -20.0f;
    const float ratio = 4.0f;
    const float expectedGR = theoreticalGR(rms_dBFS - threshold, ratio);  // -10.5

    // Generate sine at 96 kHz
    const float amplitude = sineAmplitudeForRms_dBFS(rms_dBFS);
    std::vector<float> input(static_cast<size_t>(settleSamples96));
    const double w = 2.0 * M_PI * 1000.0 / sr96;
    for (int i = 0; i < settleSamples96; ++i)
        input[static_cast<size_t>(i)] = amplitude * static_cast<float>(std::sin(w * i));

    mw160::Compressor comp;
    comp.prepare(sr96, 512);
    comp.setThreshold(threshold);
    comp.setRatio(ratio);
    comp.setOutputGain(0.0f);

    std::vector<float> output(input.size());
    for (size_t i = 0; i < input.size(); ++i)
        output[i] = comp.processSample(input[i]);

    const float inputRms_dB = toDb(measureRms(input, measureStart96, measureLen96));
    const float outputRms_dB = toDb(measureRms(output, measureStart96, measureLen96));
    const float measuredGR = outputRms_dB - inputRms_dB;

    REQUIRE_THAT(static_cast<double>(measuredGR),
                 WithinAbs(static_cast<double>(expectedGR), 0.5));
}
