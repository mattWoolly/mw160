#include <catch2/catch_test_macros.hpp>
#include "dsp/Compressor.h"

TEST_CASE("Smoke: Catch2 framework is operational", "[smoke]")
{
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("Smoke: Compressor default is unity-gain passthrough", "[smoke][dsp]")
{
    mw160::Compressor comp;
    comp.prepare(44100.0, 512);

    REQUIRE(comp.processSample(0.0f) == 0.0f);
    REQUIRE(comp.processSample(1.0f) == 1.0f);
    REQUIRE(comp.processSample(-0.5f) == -0.5f);
}
