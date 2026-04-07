#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/Ballistics.h"
#include "dsp/Compressor.h"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

static constexpr double kSampleRate = 44100.0;

// Helper: measure time (ms) for Ballistics to reach a given fraction of target GR
static double measureAttackTime(double sampleRate, float targetGR_dB, float fraction = 0.9f)
{
    mw160::Ballistics bal;
    bal.prepare(sampleRate);

    const float threshold = targetGR_dB * fraction;
    int samples = 0;
    float gr = 0.0f;
    const int maxSamples = static_cast<int>(sampleRate * 0.5); // 500 ms safety limit

    while (gr > threshold && samples < maxSamples)
    {
        gr = bal.processSample(targetGR_dB);
        samples++;
    }

    return samples / sampleRate * 1000.0; // ms
}

// Helper: measure instantaneous release rate (dB/sec) at a given GR depth
static double measureReleaseRate(double sampleRate, float settleGR_dB)
{
    mw160::Ballistics bal;
    bal.prepare(sampleRate);

    // Drive to target GR (generous settle time)
    for (int i = 0; i < static_cast<int>(sampleRate * 0.2); ++i)
        bal.processSample(settleGR_dB);

    // Begin releasing. Measure rate over a 10 ms window starting after a
    // brief delay to avoid the attack-to-release transition artifact.
    float gr = bal.processSample(0.0f);
    const int windowSamples = static_cast<int>(sampleRate * 0.010); // 10 ms

    float grBefore = gr;
    for (int i = 0; i < windowSamples; ++i)
        gr = bal.processSample(0.0f);
    float grAfter = gr;

    // Rate is positive (moving toward 0)
    return (grAfter - grBefore) / (windowSamples / sampleRate);
}

// ============================================================
// Attack timing calibration — published DBX 160 specs:
//   10 dB GR target → 15 ms attack
//   20 dB GR target →  5 ms attack
//   30 dB GR target →  3 ms attack
// Acceptance: within 20% of each spec
// ============================================================

TEST_CASE("Timing calibration: 10 dB attack within 20% of 15 ms",
          "[dsp][calibration][attack]")
{
    const double measured = measureAttackTime(kSampleRate, -10.0f);
    INFO("Measured 10 dB attack time: " << measured << " ms (spec: 15 ms, ±20% = 12–18 ms)");
    REQUIRE(measured >= 15.0 * 0.8); // ≥ 12.0 ms
    REQUIRE(measured <= 15.0 * 1.2); // ≤ 18.0 ms
}

TEST_CASE("Timing calibration: 20 dB attack within 20% of 5 ms",
          "[dsp][calibration][attack]")
{
    const double measured = measureAttackTime(kSampleRate, -20.0f);
    INFO("Measured 20 dB attack time: " << measured << " ms (spec: 5 ms, ±20% = 4–6 ms)");
    REQUIRE(measured >= 5.0 * 0.8); // ≥ 4.0 ms
    REQUIRE(measured <= 5.0 * 1.2); // ≤ 6.0 ms
}

TEST_CASE("Timing calibration: 30 dB attack within 20% of 3 ms",
          "[dsp][calibration][attack]")
{
    const double measured = measureAttackTime(kSampleRate, -30.0f);
    INFO("Measured 30 dB attack time: " << measured << " ms (spec: 3 ms, ±20% = 2.4–3.6 ms)");
    REQUIRE(measured >= 3.0 * 0.8); // ≥ 2.4 ms
    REQUIRE(measured <= 3.0 * 1.2); // ≤ 3.6 ms
}

TEST_CASE("Timing calibration: program-dependent ordering preserved",
          "[dsp][calibration][attack]")
{
    const double t10 = measureAttackTime(kSampleRate, -10.0f);
    const double t20 = measureAttackTime(kSampleRate, -20.0f);
    const double t30 = measureAttackTime(kSampleRate, -30.0f);

    INFO("Attack times: 10 dB=" << t10 << " ms, 20 dB=" << t20 << " ms, 30 dB=" << t30 << " ms");
    REQUIRE(t30 < t20);
    REQUIRE(t20 < t10);
}

// ============================================================
// Release rate calibration — published spec: ~125 dB/sec
// Acceptance: within 15% across 5–50 dB GR range
// ============================================================

TEST_CASE("Timing calibration: release rate ~125 dB/sec at -5 dB GR",
          "[dsp][calibration][release]")
{
    const double rate = measureReleaseRate(kSampleRate, -5.0f);
    INFO("Release rate at -5 dB GR: " << rate << " dB/sec (spec: 125, ±15% = 106.25–143.75)");
    REQUIRE_THAT(rate, WithinRel(125.0, 0.15));
}

TEST_CASE("Timing calibration: release rate ~125 dB/sec at -10 dB GR",
          "[dsp][calibration][release]")
{
    const double rate = measureReleaseRate(kSampleRate, -10.0f);
    INFO("Release rate at -10 dB GR: " << rate << " dB/sec");
    REQUIRE_THAT(rate, WithinRel(125.0, 0.15));
}

TEST_CASE("Timing calibration: release rate ~125 dB/sec at -20 dB GR",
          "[dsp][calibration][release]")
{
    const double rate = measureReleaseRate(kSampleRate, -20.0f);
    INFO("Release rate at -20 dB GR: " << rate << " dB/sec");
    REQUIRE_THAT(rate, WithinRel(125.0, 0.15));
}

TEST_CASE("Timing calibration: release rate ~125 dB/sec at -30 dB GR",
          "[dsp][calibration][release]")
{
    const double rate = measureReleaseRate(kSampleRate, -30.0f);
    INFO("Release rate at -30 dB GR: " << rate << " dB/sec");
    REQUIRE_THAT(rate, WithinRel(125.0, 0.15));
}

TEST_CASE("Timing calibration: release rate ~125 dB/sec at -50 dB GR",
          "[dsp][calibration][release]")
{
    const double rate = measureReleaseRate(kSampleRate, -50.0f);
    INFO("Release rate at -50 dB GR: " << rate << " dB/sec");
    REQUIRE_THAT(rate, WithinRel(125.0, 0.15));
}

// ============================================================
// Transient "crack" preservation
// The DBX 160's fast-then-slow attack lets the initial transient peak
// pass through before gain reduction clamps down. Verify that a sharp
// drum-like transient retains its initial peak in the output.
// ============================================================

TEST_CASE("Timing calibration: transient crack is preserved through compressor",
          "[dsp][calibration][transient]")
{
    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(-20.0f);
    comp.setRatio(4.0f);
    comp.setOutputGain(0.0f);
    comp.setMix(100.0f);

    // Warm up the detector with silence so it's in a quiet state
    for (int i = 0; i < static_cast<int>(kSampleRate * 0.1); ++i)
        comp.processSample(0.0f);

    // Generate a synthetic drum transient: sharp 0.5 ms attack, 20 ms decay
    // Peak amplitude = 0.9 (well above -20 dB threshold for sustained signal)
    const int attackSamples = static_cast<int>(kSampleRate * 0.0005);  // 0.5 ms
    const int decaySamples  = static_cast<int>(kSampleRate * 0.020);   // 20 ms
    const int totalSamples  = attackSamples + decaySamples;
    const float peakAmplitude = 0.9f;

    std::vector<float> input(static_cast<size_t>(totalSamples));
    std::vector<float> output(static_cast<size_t>(totalSamples));

    // Build envelope: linear attack then exponential decay
    for (int i = 0; i < attackSamples; ++i)
    {
        float env = peakAmplitude * static_cast<float>(i) / static_cast<float>(attackSamples);
        input[static_cast<size_t>(i)] = env;
    }
    for (int i = 0; i < decaySamples; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(decaySamples);
        float env = peakAmplitude * std::exp(-5.0f * t); // ~60 dB decay over 20 ms
        input[static_cast<size_t>(attackSamples + i)] = env;
    }

    // Process
    for (int i = 0; i < totalSamples; ++i)
        output[static_cast<size_t>(i)] = comp.processSample(input[static_cast<size_t>(i)]);

    // Find peak in first 2 ms of output (the initial transient window)
    const int peakWindow = static_cast<int>(kSampleRate * 0.002);
    float outputPeak = 0.0f;
    float inputPeak = 0.0f;
    for (int i = 0; i < peakWindow && i < totalSamples; ++i)
    {
        outputPeak = std::max(outputPeak, std::abs(output[static_cast<size_t>(i)]));
        inputPeak  = std::max(inputPeak, std::abs(input[static_cast<size_t>(i)]));
    }

    // The transient peak should pass through with minimal attenuation.
    // The RMS detector has a 20 ms time constant, so the sidechain hasn't
    // responded yet. We expect the output peak to be at least 80% of input peak.
    INFO("Input peak (first 2 ms): " << inputPeak);
    INFO("Output peak (first 2 ms): " << outputPeak);
    INFO("Ratio: " << (inputPeak > 0 ? outputPeak / inputPeak : 0));

    REQUIRE(outputPeak >= inputPeak * 0.80f);
}

TEST_CASE("Timing calibration: sustained signal after transient is compressed",
          "[dsp][calibration][transient]")
{
    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(-20.0f);
    comp.setRatio(4.0f);
    comp.setOutputGain(0.0f);
    comp.setMix(100.0f);

    // Warm up with silence
    for (int i = 0; i < static_cast<int>(kSampleRate * 0.1); ++i)
        comp.processSample(0.0f);

    // Feed a sustained loud sine (RMS ≈ -6 dB, 14 dB above threshold)
    const float amplitude = 0.5f; // RMS ≈ -9 dB
    const int totalSamples = static_cast<int>(kSampleRate * 0.5); // 500 ms
    std::vector<float> output(static_cast<size_t>(totalSamples));

    for (int i = 0; i < totalSamples; ++i)
    {
        float sample = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f *
                       static_cast<float>(i) / static_cast<float>(kSampleRate));
        output[static_cast<size_t>(i)] = comp.processSample(sample);
    }

    // Measure RMS of last 100 ms — should show significant compression
    const int measureLen = static_cast<int>(kSampleRate * 0.1);
    const int measureStart = totalSamples - measureLen;
    double sumSq = 0.0;
    for (int i = measureStart; i < totalSamples; ++i)
    {
        double s = output[static_cast<size_t>(i)];
        sumSq += s * s;
    }
    float outputRms = static_cast<float>(std::sqrt(sumSq / measureLen));
    float inputRms = amplitude / std::sqrt(2.0f);
    float grApprox_dB = 20.0f * std::log10(outputRms / inputRms);

    INFO("Input RMS: " << inputRms << " (" << 20.0f * std::log10(inputRms) << " dB)");
    INFO("Output RMS (steady state): " << outputRms << " (" << 20.0f * std::log10(outputRms) << " dB)");
    INFO("Approximate GR: " << grApprox_dB << " dB");

    // There should be measurable compression (at least 3 dB of GR)
    REQUIRE(grApprox_dB < -3.0f);
}

// ============================================================
// Sample-rate independence: timing should scale correctly at 96 kHz
// ============================================================

TEST_CASE("Timing calibration: attack times consistent at 96 kHz",
          "[dsp][calibration][samplerate]")
{
    constexpr double sr96 = 96000.0;

    const double t10_44 = measureAttackTime(kSampleRate, -10.0f);
    const double t10_96 = measureAttackTime(sr96, -10.0f);

    const double t30_44 = measureAttackTime(kSampleRate, -30.0f);
    const double t30_96 = measureAttackTime(sr96, -30.0f);

    // Attack times should be within 10% between sample rates
    INFO("10 dB attack: 44.1 kHz=" << t10_44 << " ms, 96 kHz=" << t10_96 << " ms");
    INFO("30 dB attack: 44.1 kHz=" << t30_44 << " ms, 96 kHz=" << t30_96 << " ms");

    REQUIRE_THAT(t10_96, WithinRel(t10_44, 0.10));
    REQUIRE_THAT(t30_96, WithinRel(t30_44, 0.10));
}

TEST_CASE("Timing calibration: release rate consistent at 96 kHz",
          "[dsp][calibration][samplerate]")
{
    constexpr double sr96 = 96000.0;

    const double rate_44 = measureReleaseRate(kSampleRate, -20.0f);
    const double rate_96 = measureReleaseRate(sr96, -20.0f);

    INFO("Release rate: 44.1 kHz=" << rate_44 << " dB/sec, 96 kHz=" << rate_96 << " dB/sec");
    REQUIRE_THAT(rate_96, WithinRel(rate_44, 0.05));
}
