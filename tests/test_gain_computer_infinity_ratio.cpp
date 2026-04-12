#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/GainComputer.h"
#include "dsp/Compressor.h"

using Catch::Matchers::WithinAbs;

// Regression tests for QA-UX-002: the ratio parameter must reach a true
// infinity:1 (limiter) slope at the top of its travel. The 1/R - 1 formula
// asymptotes toward -1.0 but never reaches it; GainComputer short-circuits
// the slope to exactly -1.0 for ratio >= kInfinityRatioThreshold so that
// the "Brick Wall" factory preset (ratio = 60.0) actually brick-walls.

namespace {
const mw160::GainComputer gc;
constexpr float kInfThresh = mw160::GainComputer::kInfinityRatioThreshold;
} // namespace

TEST_CASE("QA-UX-002: slope pinned to -1.0 exactly at ratio = kInfinityRatioThreshold",
          "[dsp][gain-computer][QA-UX-002]")
{
    // At exactly the threshold, the short-circuit must fire. With
    // threshold_dB = -20 and input -10 (10 dB excess), 10 dB of excess
    // must produce exactly -10 dB of GR.
    const float gr = gc.computeGainReduction(-10.0f, -20.0f, kInfThresh);
    REQUIRE_THAT(static_cast<double>(gr), WithinAbs(-10.0, 0.001));
}

TEST_CASE("QA-UX-002: slope pinned to -1.0 at ratio = 60 (top of range)",
          "[dsp][gain-computer][QA-UX-002]")
{
    // The top-of-travel value must also pin to -1.0.
    const float gr = gc.computeGainReduction(-10.0f, -20.0f, 60.0f);
    REQUIRE_THAT(static_cast<double>(gr), WithinAbs(-10.0, 0.001));
}

TEST_CASE("QA-UX-002: ratio = 49 still uses the 1/R - 1 formula (no short-circuit below threshold)",
          "[dsp][gain-computer][QA-UX-002]")
{
    // Regression guard: the short-circuit must not fire below the
    // kInfinityRatioThreshold. 10 dB excess at ratio 49 should give the
    // classic formula result: 10 * (1/49 - 1) = -9.79591...
    const float gr = gc.computeGainReduction(-10.0f, -20.0f, 49.0f);
    const float expected = 10.0f * (1.0f / 49.0f - 1.0f);
    REQUIRE_THAT(static_cast<double>(gr), WithinAbs(expected, 0.001));
    // And it must not be -10 exactly.
    REQUIRE(gr > -10.0f);
}

TEST_CASE("QA-UX-002: Brick Wall preset parameters pin output at threshold (pure GainComputer)",
          "[dsp][gain-computer][QA-UX-002]")
{
    // Mimic the "Brick Wall" factory preset: threshold -6 dB, ratio 60.
    // Inputs of -3, 0, +3 dBFS give 3/6/9 dB excess above threshold. The
    // gain reduction must exactly cancel the excess, pinning output at -6.
    const float threshold_dB = -6.0f;
    const float ratio = 60.0f;

    struct Case { float input; float expectedGR; };
    const Case cases[] = {
        { -3.0f, -3.0f },
        {  0.0f, -6.0f },
        {  3.0f, -9.0f },
    };

    for (const auto& c : cases)
    {
        const float gr = gc.computeGainReduction(c.input, threshold_dB, ratio);
        REQUIRE_THAT(static_cast<double>(gr), WithinAbs(c.expectedGR, 0.001));
        // Output (in the level domain, pre-make-up gain) is input + GR.
        const float output = c.input + gr;
        REQUIRE_THAT(static_cast<double>(output), WithinAbs(threshold_dB, 0.001));
    }
}

TEST_CASE("QA-UX-002: soft-knee path uses the pinned slope above the knee",
          "[dsp][gain-computer][QA-UX-002]")
{
    // Soft knee: threshold -10, knee width 6 (half-knee = 3, knee top = -7),
    // ratio 60. Input -4 is 6 dB above threshold, well outside the knee.
    // That 6 dB excess must produce exactly -6 dB GR.
    const float gr = gc.computeGainReduction(-4.0f, -10.0f, 60.0f, 6.0f);
    REQUIRE_THAT(static_cast<double>(gr), WithinAbs(-6.0, 0.001));
}

TEST_CASE("QA-UX-002: full Compressor with Brick Wall preset at feedback equilibrium",
          "[dsp][gain-computer][QA-UX-002]")
{
    // End-to-end sanity check: run the Compressor with the Brick Wall
    // factory preset parameters (threshold -6, ratio 60, outputGain 0).
    //
    // In feedback topology with slope pinned to -1.0 (infinity:1), the
    // closed-form equilibrium is:
    //   GR = excess * (-1) / (1 - (-1)) = -excess / 2
    //   output_dB = input_dB - excess / 2 = (input_dB + threshold) / 2
    //
    // This gives an effective 2:1 overall I/O ratio — characteristic of
    // feedback compressors at their maximum ratio setting.
    constexpr double kSampleRate = 44100.0;
    const float threshold_dB = -6.0f;
    const float ratio = 60.0f;
    const float excess_dB = 10.0f;

    const float rmsDesired_dB = threshold_dB + excess_dB;
    const float rmsDesired = std::pow(10.0f, rmsDesired_dB / 20.0f);
    const float amplitude = rmsDesired * std::sqrt(2.0f);

    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold_dB);
    comp.setRatio(ratio);
    comp.setOutputGain(0.0f);
    comp.setSoftKnee(false);
    comp.setMix(100.0f);

    const int totalSamples = static_cast<int>(kSampleRate);
    std::vector<float> output(static_cast<size_t>(totalSamples));
    const double twoPiOverSr = 2.0 * 3.14159265358979323846 * 1000.0 / kSampleRate;
    for (int i = 0; i < totalSamples; ++i)
    {
        const float s = amplitude * static_cast<float>(std::sin(twoPiOverSr * i));
        output[static_cast<size_t>(i)] = comp.processSample(s);
    }

    const int startSample = totalSamples / 2;
    double sum = 0.0;
    for (int i = startSample; i < totalSamples; ++i)
    {
        const double v = static_cast<double>(output[static_cast<size_t>(i)]);
        sum += v * v;
    }
    const float outputRms = static_cast<float>(std::sqrt(sum / (totalSamples - startSample)));
    const float outputRms_dB = 20.0f * std::log10(outputRms);

    // Feedback equilibrium: output ≈ (input + threshold) / 2
    const float expectedOutput_dB = (rmsDesired_dB + threshold_dB) / 2.0f;  // -1.0
    REQUIRE_THAT(static_cast<double>(outputRms_dB),
                 WithinAbs(static_cast<double>(expectedOutput_dB), 1.0));
}
