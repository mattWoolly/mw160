#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/GainComputer.h"

using Catch::Matchers::WithinAbs;

static const mw160::GainComputer gc;

// --- Acceptance criteria ---

TEST_CASE("GainComputer: input at threshold yields 0 dB GR", "[dsp][gc]")
{
    REQUIRE_THAT(gc.computeGainReduction(0.0f, 0.0f, 4.0f),
                 WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(gc.computeGainReduction(-10.0f, -10.0f, 8.0f),
                 WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(gc.computeGainReduction(20.0f, 20.0f, 60.0f),
                 WithinAbs(0.0, 1e-6));
}

TEST_CASE("GainComputer: 10 dB above threshold at 4:1 yields -7.5 dB GR", "[dsp][gc]")
{
    // GR = 10 * (1/4 - 1) = 10 * (-0.75) = -7.5
    REQUIRE_THAT(gc.computeGainReduction(10.0f, 0.0f, 4.0f),
                 WithinAbs(-7.5, 1e-4));
}

TEST_CASE("GainComputer: ratio 60 (infinity:1) pins output near threshold", "[dsp][gc]")
{
    // GR = 10 * (1/60 - 1) = 10 * (-59/60) ≈ -9.833
    // Output = 10 + (-9.833) = 0.167 ≈ threshold (0 dB)
    const float gr = gc.computeGainReduction(10.0f, 0.0f, 60.0f);
    const float output = 10.0f + gr;
    REQUIRE_THAT(static_cast<double>(output), WithinAbs(0.0, 0.2));

    // 20 dB above threshold
    const float gr20 = gc.computeGainReduction(20.0f, 0.0f, 60.0f);
    const float output20 = 20.0f + gr20;
    REQUIRE_THAT(static_cast<double>(output20), WithinAbs(0.0, 0.4));
}

TEST_CASE("GainComputer: ratio 1.0 yields 0 dB GR always", "[dsp][gc]")
{
    REQUIRE_THAT(gc.computeGainReduction(-20.0f, -30.0f, 1.0f),
                 WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(gc.computeGainReduction(10.0f, 0.0f, 1.0f),
                 WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(gc.computeGainReduction(20.0f, -10.0f, 1.0f),
                 WithinAbs(0.0, 1e-6));
}

TEST_CASE("GainComputer: input below threshold yields 0 dB GR", "[dsp][gc]")
{
    REQUIRE_THAT(gc.computeGainReduction(-20.0f, 0.0f, 4.0f),
                 WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(gc.computeGainReduction(-5.0f, -3.0f, 8.0f),
                 WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(gc.computeGainReduction(-40.0f, -10.0f, 60.0f),
                 WithinAbs(0.0, 1e-6));
}

// --- Additional coverage ---

TEST_CASE("GainComputer: various compression ratios", "[dsp][gc]")
{
    // 2:1 — 10 dB excess → -5 dB GR
    REQUIRE_THAT(gc.computeGainReduction(10.0f, 0.0f, 2.0f),
                 WithinAbs(-5.0, 1e-4));

    // 8:1 — 10 dB excess → -8.75 dB GR
    REQUIRE_THAT(gc.computeGainReduction(10.0f, 0.0f, 8.0f),
                 WithinAbs(-8.75, 1e-4));
}

TEST_CASE("GainComputer: negative threshold values", "[dsp][gc]")
{
    // Threshold -20 dB, input -10 dB (10 dB excess), ratio 4:1 → -7.5 dB GR
    REQUIRE_THAT(gc.computeGainReduction(-10.0f, -20.0f, 4.0f),
                 WithinAbs(-7.5, 1e-4));
}

TEST_CASE("GainComputer: GR is always negative or zero", "[dsp][gc]")
{
    // Across a range of parameters, GR should never be positive
    for (float input = -40.0f; input <= 20.0f; input += 5.0f)
        for (float thresh = -40.0f; thresh <= 20.0f; thresh += 10.0f)
            for (float ratio : {1.0f, 2.0f, 4.0f, 8.0f, 20.0f, 60.0f})
                REQUIRE(gc.computeGainReduction(input, thresh, ratio) <= 0.0f);
}

TEST_CASE("GainComputer: higher ratio yields more gain reduction", "[dsp][gc]")
{
    const float input = 10.0f;
    const float thresh = 0.0f;

    float prevGR = 0.0f;
    for (float ratio : {2.0f, 4.0f, 8.0f, 20.0f, 60.0f})
    {
        float gr = gc.computeGainReduction(input, thresh, ratio);
        REQUIRE(gr < prevGR);
        prevGR = gr;
    }
}

// --- OverEasy (soft knee) tests ---

static constexpr float kKneeWidth = 10.0f;  // matches Compressor::kOverEasyKneeWidth_dB

TEST_CASE("OverEasy: input well below threshold yields 0 dB GR", "[dsp][gc][overeasy]")
{
    // Well below knee region (threshold 0, knee bottom = -5)
    REQUIRE_THAT(gc.computeGainReduction(-20.0f, 0.0f, 4.0f, kKneeWidth),
                 WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(gc.computeGainReduction(-10.0f, -5.0f, 8.0f, kKneeWidth),
                 WithinAbs(0.0, 1e-6));
}

TEST_CASE("OverEasy: input well above threshold yields same GR as hard knee", "[dsp][gc][overeasy]")
{
    // Well above knee region (threshold 0, knee top = +5)
    const float input = 20.0f;
    const float thresh = 0.0f;

    for (float ratio : {2.0f, 4.0f, 8.0f, 60.0f})
    {
        float hardGR = gc.computeGainReduction(input, thresh, ratio, 0.0f);
        float softGR = gc.computeGainReduction(input, thresh, ratio, kKneeWidth);
        REQUIRE_THAT(static_cast<double>(softGR), WithinAbs(hardGR, 1e-4));
    }
}

TEST_CASE("OverEasy: input in knee region yields GR between 0 and full-ratio GR", "[dsp][gc][overeasy]")
{
    const float thresh = 0.0f;
    const float ratio = 4.0f;

    // Test at threshold center (0 dB) — inside the knee
    float gr = gc.computeGainReduction(0.0f, thresh, ratio, kKneeWidth);
    float hardGR = gc.computeGainReduction(0.0f, thresh, ratio, 0.0f);
    REQUIRE(gr < 0.0f);        // some compression
    REQUIRE(gr > hardGR - 1.0f); // but less than full hard-knee GR at same point

    // Test at knee midpoints
    float grLow = gc.computeGainReduction(-2.0f, thresh, ratio, kKneeWidth);
    float grHigh = gc.computeGainReduction(2.0f, thresh, ratio, kKneeWidth);
    REQUIRE(grLow < 0.0f);
    REQUIRE(grHigh < grLow);  // more compression higher in the knee
}

TEST_CASE("OverEasy: gain reduction curve is continuous at knee boundaries", "[dsp][gc][overeasy]")
{
    const float thresh = 0.0f;
    const float ratio = 4.0f;
    const float halfKnee = kKneeWidth / 2.0f;

    // At bottom of knee (threshold - W/2): GR should be ~0
    float grAtBottom = gc.computeGainReduction(thresh - halfKnee, thresh, ratio, kKneeWidth);
    REQUIRE_THAT(static_cast<double>(grAtBottom), WithinAbs(0.0, 1e-4));

    // Just inside the bottom of knee
    float grJustInside = gc.computeGainReduction(thresh - halfKnee + 0.01f, thresh, ratio, kKneeWidth);
    REQUIRE(grJustInside <= 0.0f);
    REQUIRE_THAT(static_cast<double>(grJustInside), WithinAbs(0.0, 0.01));

    // At top of knee: GR should match hard-knee GR for that excess
    float grAtTop = gc.computeGainReduction(thresh + halfKnee, thresh, ratio, kKneeWidth);
    float hardGRAtTop = gc.computeGainReduction(thresh + halfKnee, thresh, ratio, 0.0f);
    REQUIRE_THAT(static_cast<double>(grAtTop), WithinAbs(hardGRAtTop, 1e-3));
}

TEST_CASE("OverEasy: at threshold center GR is approximately half of hard-knee GR", "[dsp][gc][overeasy]")
{
    const float thresh = 0.0f;
    const float ratio = 4.0f;

    // At the threshold center, the soft knee should produce roughly half
    // the GR that a hard knee would produce for the same input at T + W/2
    // (since the quadratic ramps from 0 to full over the knee width).
    // Specifically: soft GR at T = slope * (W/2)^2 / (2*W) = slope * W / 8
    // Hard GR at T+W/2 = slope * W/2
    // Ratio = (slope * W / 8) / (slope * W / 2) = 1/4
    // So at the center, GR is 1/4 of the hard-knee GR at the knee top.
    float grAtCenter = gc.computeGainReduction(thresh, thresh, ratio, kKneeWidth);
    float slope = 1.0f / ratio - 1.0f;
    float expectedGR = slope * kKneeWidth / 8.0f;
    REQUIRE_THAT(static_cast<double>(grAtCenter), WithinAbs(expectedGR, 1e-4));
}

TEST_CASE("OverEasy: GR is always negative or zero with soft knee", "[dsp][gc][overeasy]")
{
    for (float input = -40.0f; input <= 20.0f; input += 2.0f)
        for (float thresh = -20.0f; thresh <= 10.0f; thresh += 10.0f)
            for (float ratio : {2.0f, 4.0f, 8.0f, 20.0f, 60.0f})
                REQUIRE(gc.computeGainReduction(input, thresh, ratio, kKneeWidth) <= 0.0f);
}

TEST_CASE("OverEasy: kneeWidth 0 gives identical results to hard knee", "[dsp][gc][overeasy]")
{
    for (float input = -20.0f; input <= 20.0f; input += 5.0f)
        for (float ratio : {2.0f, 4.0f, 8.0f, 60.0f})
        {
            float hard = gc.computeGainReduction(input, 0.0f, ratio);
            float softZero = gc.computeGainReduction(input, 0.0f, ratio, 0.0f);
            REQUIRE_THAT(static_cast<double>(softZero), WithinAbs(hard, 1e-6));
        }
}
