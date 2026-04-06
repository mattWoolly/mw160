#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "dsp/Ballistics.h"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

static constexpr double kSampleRate = 44100.0;

// --- Acceptance criteria ---

TEST_CASE("Ballistics: large step attacks fast", "[dsp][ballistics]")
{
    mw160::Ballistics bal;
    bal.prepare(kSampleRate);

    // Step from 0 to -20 dB GR target
    const float target = -20.0f;
    const int samplesAt1ms = static_cast<int>(kSampleRate * 0.001);

    float gr = 0.0f;
    for (int i = 0; i < samplesAt1ms; ++i)
        gr = bal.processSample(target);

    // After 1 ms, a 20 dB step should have reached significant GR
    REQUIRE(gr < -10.0f);
}

TEST_CASE("Ballistics: small step attacks slower than large step", "[dsp][ballistics]")
{
    // 20 dB step
    mw160::Ballistics bal20;
    bal20.prepare(kSampleRate);

    const int samplesAt1ms = static_cast<int>(kSampleRate * 0.001);

    float gr20 = 0.0f;
    for (int i = 0; i < samplesAt1ms; ++i)
        gr20 = bal20.processSample(-20.0f);

    // 5 dB step
    mw160::Ballistics bal5;
    bal5.prepare(kSampleRate);

    float gr5 = 0.0f;
    for (int i = 0; i < samplesAt1ms; ++i)
        gr5 = bal5.processSample(-5.0f);

    // Both should be negative (compressing)
    REQUIRE(gr20 < 0.0f);
    REQUIRE(gr5 < 0.0f);

    // The 20 dB step should reach a larger fraction of its target than the 5 dB step
    // Normalize: fraction = current / target (both negative, result is positive 0-1)
    const double fraction20 = gr20 / -20.0f;
    const double fraction5 = gr5 / -5.0f;

    REQUIRE(fraction20 > fraction5);
}

TEST_CASE("Ballistics: release at ~125 dB/sec constant rate", "[dsp][ballistics]")
{
    mw160::Ballistics bal;
    bal.prepare(kSampleRate);

    // Drive to -20 dB GR (attack quickly with a large step)
    for (int i = 0; i < static_cast<int>(kSampleRate * 0.05); ++i)
        bal.processSample(-20.0f);

    // Now release: target = 0 dB
    // At 125 dB/sec, 20 dB should release in ~160 ms = 7056 samples at 44.1 kHz
    const int expectedReleaseSamples = static_cast<int>(20.0 / 125.0 * kSampleRate);

    float gr = 0.0f;
    for (int i = 0; i < expectedReleaseSamples; ++i)
        gr = bal.processSample(0.0f);

    // Should be at or very near 0 dB after the expected release time
    REQUIRE_THAT(static_cast<double>(gr), WithinAbs(0.0, 0.5));
}

TEST_CASE("Ballistics: release from -20 dB takes approximately 160 ms", "[dsp][ballistics]")
{
    mw160::Ballistics bal;
    bal.prepare(kSampleRate);

    // Drive to -20 dB GR
    for (int i = 0; i < static_cast<int>(kSampleRate * 0.1); ++i)
        bal.processSample(-20.0f);

    // Count samples until GR reaches -0.1 dB (essentially released)
    int releaseSamples = 0;
    float gr = bal.processSample(0.0f);
    releaseSamples++;
    while (gr < -0.1f && releaseSamples < static_cast<int>(kSampleRate))
    {
        gr = bal.processSample(0.0f);
        releaseSamples++;
    }

    // Expected: 20/125 = 0.16s = 160 ms = 7056 samples at 44.1 kHz
    const double releaseTime_ms = releaseSamples / kSampleRate * 1000.0;
    REQUIRE_THAT(releaseTime_ms, WithinAbs(160.0, 10.0));
}

TEST_CASE("Ballistics: release rate is constant (linear in dB)", "[dsp][ballistics]")
{
    mw160::Ballistics bal;
    bal.prepare(kSampleRate);

    // Drive to -30 dB GR
    for (int i = 0; i < static_cast<int>(kSampleRate * 0.1); ++i)
        bal.processSample(-30.0f);

    // Release and measure rate at two different points
    // First: measure rate around -25 dB
    float gr = 0.0f;
    for (int i = 0; i < 100; ++i)
        gr = bal.processSample(0.0f);

    float grBefore = gr;
    for (int i = 0; i < 441; ++i) // ~10 ms
        gr = bal.processSample(0.0f);
    float grAfter = gr;
    double rate1 = (grAfter - grBefore) / (441.0 / kSampleRate); // dB/sec

    // Continue releasing and measure rate around -15 dB
    for (int i = 0; i < 2000; ++i)
        gr = bal.processSample(0.0f);

    grBefore = gr;
    for (int i = 0; i < 441; ++i)
        gr = bal.processSample(0.0f);
    grAfter = gr;
    double rate2 = (grAfter - grBefore) / (441.0 / kSampleRate);

    // Both rates should be approximately 125 dB/sec
    REQUIRE_THAT(rate1, WithinAbs(125.0, 2.0));
    REQUIRE_THAT(rate2, WithinAbs(125.0, 2.0));
}

// --- Basic functionality ---

TEST_CASE("Ballistics: zero target produces zero output", "[dsp][ballistics]")
{
    mw160::Ballistics bal;
    bal.prepare(kSampleRate);

    for (int i = 0; i < 1000; ++i)
        REQUIRE(bal.processSample(0.0f) == 0.0f);
}

TEST_CASE("Ballistics: reset clears state", "[dsp][ballistics]")
{
    mw160::Ballistics bal;
    bal.prepare(kSampleRate);

    // Drive into compression
    for (int i = 0; i < 4410; ++i)
        bal.processSample(-20.0f);

    bal.reset();

    // After reset, output should be 0 (no GR)
    REQUIRE(bal.processSample(0.0f) == 0.0f);
}

TEST_CASE("Ballistics: works at 96 kHz", "[dsp][ballistics]")
{
    constexpr double sr = 96000.0;
    mw160::Ballistics bal;
    bal.prepare(sr);

    // Drive to -20 dB and release
    for (int i = 0; i < static_cast<int>(sr * 0.1); ++i)
        bal.processSample(-20.0f);

    // Release should still take ~160 ms
    int releaseSamples = 0;
    float gr = bal.processSample(0.0f);
    releaseSamples++;
    while (gr < -0.1f && releaseSamples < static_cast<int>(sr))
    {
        gr = bal.processSample(0.0f);
        releaseSamples++;
    }

    const double releaseTime_ms = releaseSamples / sr * 1000.0;
    REQUIRE_THAT(releaseTime_ms, WithinAbs(160.0, 10.0));
}

TEST_CASE("Ballistics: output converges to sustained target", "[dsp][ballistics]")
{
    mw160::Ballistics bal;
    bal.prepare(kSampleRate);

    const float target = -15.0f;

    // Run for 500 ms — should converge to target
    float gr = 0.0f;
    for (int i = 0; i < static_cast<int>(kSampleRate * 0.5); ++i)
        gr = bal.processSample(target);

    REQUIRE_THAT(static_cast<double>(gr), WithinAbs(static_cast<double>(target), 0.01));
}

TEST_CASE("Ballistics: GR never overshoots target during attack", "[dsp][ballistics]")
{
    mw160::Ballistics bal;
    bal.prepare(kSampleRate);

    const float target = -20.0f;

    for (int i = 0; i < static_cast<int>(kSampleRate * 0.1); ++i)
    {
        float gr = bal.processSample(target);
        REQUIRE(gr >= target); // GR should never go below target
        REQUIRE(gr <= 0.0f);  // GR should never go positive
    }
}

TEST_CASE("Ballistics: matches published 160A attack specs approximately", "[dsp][ballistics]")
{
    // Published specs:
    // 10 dB GR in 15 ms
    // 20 dB GR in 5 ms
    // 30 dB GR in 3 ms
    // These are approximate targets — the implementation uses a simplified model
    // that should show the same trend: bigger steps = faster attack.

    auto measureAttackTime = [](double sampleRate, float targetGR) -> double
    {
        mw160::Ballistics bal;
        bal.prepare(sampleRate);

        const float threshold = targetGR * 0.9f; // 90% of target as "arrived"
        int samples = 0;
        float gr = 0.0f;

        while (gr > threshold && samples < static_cast<int>(sampleRate * 0.1))
        {
            gr = bal.processSample(targetGR);
            samples++;
        }

        return samples / sampleRate * 1000.0; // ms
    };

    double time10 = measureAttackTime(kSampleRate, -10.0f);
    double time20 = measureAttackTime(kSampleRate, -20.0f);
    double time30 = measureAttackTime(kSampleRate, -30.0f);

    // Program-dependent: larger steps should be faster
    REQUIRE(time30 < time20);
    REQUIRE(time20 < time10);

    // All should be in single-digit milliseconds for these magnitudes
    REQUIRE(time10 < 20.0);
    REQUIRE(time20 < 10.0);
    REQUIRE(time30 < 5.0);
}
