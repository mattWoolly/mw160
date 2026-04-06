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
