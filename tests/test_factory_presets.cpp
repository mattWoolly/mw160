#include <catch2/catch_test_macros.hpp>
#include <string>
#include <set>
#include "FactoryPresets.h"

TEST_CASE("Factory presets: correct count", "[presets]")
{
    REQUIRE(mw160::kNumFactoryPresets == 7);
}

TEST_CASE("Factory presets: parameter ranges are valid", "[presets]")
{
    for (int i = 0; i < mw160::kNumFactoryPresets; ++i)
    {
        const auto& p = mw160::kFactoryPresets[i];
        INFO("Preset index " << i << ": " << p.name);

        CHECK(p.threshold  >= -40.0f);
        CHECK(p.threshold  <=  20.0f);
        CHECK(p.ratio      >=   1.0f);
        CHECK(p.ratio      <=  60.0f);
        CHECK(p.outputGain >= -20.0f);
        CHECK(p.outputGain <=  20.0f);
        CHECK(p.mix        >=   0.0f);
        CHECK(p.mix        <= 100.0f);
    }
}

TEST_CASE("Factory presets: names are unique and non-empty", "[presets]")
{
    std::set<std::string> names;

    for (int i = 0; i < mw160::kNumFactoryPresets; ++i)
    {
        const auto& p = mw160::kFactoryPresets[i];
        INFO("Preset index " << i);

        REQUIRE(p.name != nullptr);
        REQUIRE(std::string(p.name).size() > 0);

        auto [it, inserted] = names.insert(p.name);
        CHECK(inserted); // fails if duplicate
    }
}

TEST_CASE("Factory presets: expected names present", "[presets]")
{
    const char* expectedNames[] = {
        "Kick Punch",
        "Snare Snap",
        "Drum Bus Glue",
        "Bass Control",
        "Parallel Smash",
        "Gentle Leveling",
        "Brick Wall",
    };

    for (const auto* expected : expectedNames)
    {
        bool found = false;
        for (int i = 0; i < mw160::kNumFactoryPresets; ++i)
        {
            if (std::string(mw160::kFactoryPresets[i].name) == expected)
            {
                found = true;
                break;
            }
        }
        INFO("Looking for: " << expected);
        CHECK(found);
    }
}

TEST_CASE("Factory presets: spot-check Kick Punch values", "[presets]")
{
    const auto& p = mw160::kFactoryPresets[0];
    REQUIRE(std::string(p.name) == "Kick Punch");
    CHECK(p.threshold == -10.0f);
    CHECK(p.ratio == 4.0f);
    CHECK(p.outputGain == 4.0f);
    CHECK(p.softKnee == false);
    CHECK(p.stereoLink == true);
    CHECK(p.mix == 100.0f);
}

TEST_CASE("Factory presets: spot-check Parallel Smash values", "[presets]")
{
    const auto& p = mw160::kFactoryPresets[4];
    REQUIRE(std::string(p.name) == "Parallel Smash");
    CHECK(p.ratio == 8.0f);
    CHECK(p.mix == 50.0f);
    CHECK(p.softKnee == false);
}

TEST_CASE("Factory presets: spot-check Brick Wall values", "[presets]")
{
    const auto& p = mw160::kFactoryPresets[6];
    REQUIRE(std::string(p.name) == "Brick Wall");
    CHECK(p.ratio == 60.0f);
    CHECK(p.softKnee == false);
}
