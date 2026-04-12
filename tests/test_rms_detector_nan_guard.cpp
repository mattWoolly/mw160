#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include "dsp/RmsDetector.h"

using Catch::Matchers::WithinRel;

static constexpr double kSampleRate = 48000.0;
static constexpr int kSettleSamples = 4800;  // 100 ms at 48 kHz
static constexpr int kTimeConstantSamples = 960;  // 20 ms at 48 kHz
static constexpr float kSignalAmplitude = 0.5f;

// Helper: settle a detector with a constant signal and return steady-state output.
static float settleDetector(mw160::RmsDetector& det, float amplitude, int samples)
{
    float out = 0.0f;
    for (int i = 0; i < samples; ++i)
        out = det.processSample(amplitude);
    return out;
}

TEST_CASE("RmsDetector: NaN input does not poison detector",
          "[dsp][rms-detector][QA-DSP-002]")
{
    mw160::RmsDetector det;
    det.prepare(kSampleRate);

    // Settle the detector with 100 ms of 0.5f amplitude signal.
    const float steadyState = settleDetector(det, kSignalAmplitude, kSettleSamples);

    // Sanity: steady state for 0.5f input should be ~0.5 (RMS of DC = amplitude).
    REQUIRE_THAT(static_cast<double>(steadyState),
                 WithinRel(0.5, 0.01));

    // Inject ONE NaN sample.
    const float nanOutput = det.processSample(std::numeric_limits<float>::quiet_NaN());

    // The output on the NaN sample itself must NOT be NaN.
    REQUIRE_FALSE(std::isnan(nanOutput));

    // The output should be finite.
    REQUIRE(std::isfinite(nanOutput));

    // Continue feeding normal signal for one time constant (20 ms = 960 samples).
    float recovered = 0.0f;
    for (int i = 0; i < kTimeConstantSamples; ++i)
        recovered = det.processSample(kSignalAmplitude);

    // After one time constant, detector should be within 0.1% of steady state.
    const double relError = std::abs(static_cast<double>(recovered) - static_cast<double>(steadyState))
                          / static_cast<double>(steadyState);
    REQUIRE(relError < 0.005);
}

TEST_CASE("RmsDetector: Inf input does not poison detector",
          "[dsp][rms-detector][QA-DSP-002]")
{
    mw160::RmsDetector det;
    det.prepare(kSampleRate);

    // Settle with 100 ms of 0.5f.
    const float steadyState = settleDetector(det, kSignalAmplitude, kSettleSamples);

    REQUIRE_THAT(static_cast<double>(steadyState),
                 WithinRel(0.5, 0.01));

    // Inject ONE Inf sample. Inf*Inf = Inf, which is non-finite.
    const float infOutput = det.processSample(std::numeric_limits<float>::infinity());

    // Output must not be NaN or Inf.
    REQUIRE_FALSE(std::isnan(infOutput));
    REQUIRE(std::isfinite(infOutput));

    // Recover over one time constant.
    float recovered = 0.0f;
    for (int i = 0; i < kTimeConstantSamples; ++i)
        recovered = det.processSample(kSignalAmplitude);

    const double relError = std::abs(static_cast<double>(recovered) - static_cast<double>(steadyState))
                          / static_cast<double>(steadyState);
    REQUIRE(relError < 0.005);
}

TEST_CASE("RmsDetector: processSampleFromSquared with NaN inputSquared directly",
          "[dsp][rms-detector][QA-DSP-002]")
{
    mw160::RmsDetector det;
    det.prepare(kSampleRate);

    // Settle with squared input = 0.25 (equivalent to amplitude 0.5).
    for (int i = 0; i < kSettleSamples; ++i)
        det.processSampleFromSquared(0.25);

    const float steadyState = det.processSampleFromSquared(0.25);

    REQUIRE_THAT(static_cast<double>(steadyState),
                 WithinRel(0.5, 0.01));

    // Inject NaN directly into processSampleFromSquared.
    const float nanOutput = det.processSampleFromSquared(
        std::numeric_limits<double>::quiet_NaN());

    // Output must not be NaN.
    REQUIRE_FALSE(std::isnan(nanOutput));
    REQUIRE(std::isfinite(nanOutput));

    // Recovery: feed normal squared values for one time constant.
    float recovered = 0.0f;
    for (int i = 0; i < kTimeConstantSamples; ++i)
        recovered = det.processSampleFromSquared(0.25);

    const double relError = std::abs(static_cast<double>(recovered) - static_cast<double>(steadyState))
                          / static_cast<double>(steadyState);
    REQUIRE(relError < 0.005);
}

TEST_CASE("RmsDetector: negative Inf input does not poison detector",
          "[dsp][rms-detector][QA-DSP-002]")
{
    mw160::RmsDetector det;
    det.prepare(kSampleRate);

    const float steadyState = settleDetector(det, kSignalAmplitude, kSettleSamples);

    // Inject -Inf. (-Inf)^2 = +Inf, caught by the same guard.
    const float negInfOutput = det.processSample(-std::numeric_limits<float>::infinity());

    REQUIRE_FALSE(std::isnan(negInfOutput));
    REQUIRE(std::isfinite(negInfOutput));

    float recovered = 0.0f;
    for (int i = 0; i < kTimeConstantSamples; ++i)
        recovered = det.processSample(kSignalAmplitude);

    const double relError = std::abs(static_cast<double>(recovered) - static_cast<double>(steadyState))
                          / static_cast<double>(steadyState);
    REQUIRE(relError < 0.005);
}
