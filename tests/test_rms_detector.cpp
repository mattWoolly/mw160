#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "dsp/RmsDetector.h"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("RmsDetector: silence produces RMS of 0", "[dsp][rms]")
{
    mw160::RmsDetector det;
    det.prepare(kSampleRate);

    for (int i = 0; i < 4410; ++i)
        det.processSample(0.0f);

    REQUIRE_THAT(det.processSample(0.0f), WithinAbs(0.0, 1e-6));
}

TEST_CASE("RmsDetector: DC signal converges to amplitude", "[dsp][rms]")
{
    mw160::RmsDetector det;
    det.prepare(kSampleRate);

    // Feed ~200 ms of DC at amplitude 1.0 (10x the time constant)
    const int settleFrames = static_cast<int>(kSampleRate * 0.2);
    float rms = 0.0f;
    for (int i = 0; i < settleFrames; ++i)
        rms = det.processSample(1.0f);

    REQUIRE_THAT(rms, WithinAbs(1.0, 0.01));
}

TEST_CASE("RmsDetector: sine wave converges to 1/sqrt(2)", "[dsp][rms]")
{
    mw160::RmsDetector det;
    det.prepare(kSampleRate);

    const double freq = 1000.0; // 1 kHz
    const double expected = 1.0 / std::sqrt(2.0); // ~0.7071
    const int settleFrames = static_cast<int>(kSampleRate * 0.5); // 500 ms

    float rms = 0.0f;
    for (int i = 0; i < settleFrames; ++i)
    {
        const float sample = static_cast<float>(
            std::sin(2.0 * M_PI * freq * i / kSampleRate));
        rms = det.processSample(sample);
    }

    REQUIRE_THAT(static_cast<double>(rms), WithinAbs(expected, 0.01));
}

TEST_CASE("RmsDetector: time constant is approximately 20 ms", "[dsp][rms]")
{
    mw160::RmsDetector det;
    det.prepare(kSampleRate);

    // Step from silence to DC=1.0.
    // After 20 ms the squared envelope should reach ~63.2% of its final value (1.0).
    // RMS = sqrt(envelope), so RMS at 20 ms ≈ sqrt(0.632) ≈ 0.795.
    const int samplesAt20ms = static_cast<int>(kSampleRate * 0.020);

    float rms = 0.0f;
    for (int i = 0; i < samplesAt20ms; ++i)
        rms = det.processSample(1.0f);

    const double expectedRms = std::sqrt(1.0 - std::exp(-1.0)); // sqrt(0.6321) ≈ 0.7952
    REQUIRE_THAT(static_cast<double>(rms), WithinAbs(expectedRms, 0.02));
}

TEST_CASE("RmsDetector: reset clears state", "[dsp][rms]")
{
    mw160::RmsDetector det;
    det.prepare(kSampleRate);

    // Feed some signal
    for (int i = 0; i < 4410; ++i)
        det.processSample(1.0f);

    det.reset();
    REQUIRE_THAT(det.processSample(0.0f), WithinAbs(0.0, 1e-6));
}

TEST_CASE("RmsDetector: works at 96 kHz", "[dsp][rms]")
{
    constexpr double sr = 96000.0;
    mw160::RmsDetector det;
    det.prepare(sr);

    // DC at 1.0, settle for 200 ms
    const int settleFrames = static_cast<int>(sr * 0.2);
    float rms = 0.0f;
    for (int i = 0; i < settleFrames; ++i)
        rms = det.processSample(1.0f);

    REQUIRE_THAT(rms, WithinAbs(1.0, 0.01));
}
