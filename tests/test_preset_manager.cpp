// Tests for MW160-32: preset system hardening.
//
// Covers:
//   QA-CONF-003  – loadPreset returns false on corrupt user preset XML
//   QA-CONF-004  – state XML carries a pluginVersion attribute
//   QA-UX-010    – saveUserPreset validates names, detects duplicates,
//                   and reports write failures

#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"
#include "PresetManager.h"

// ============================================================================
// QA-UX-010: preset name validation
// ============================================================================

TEST_CASE("isValidPresetName accepts normal names",
          "[presets][QA-UX-010]")
{
    CHECK(PresetManager::isValidPresetName("My Preset"));
    CHECK(PresetManager::isValidPresetName("Kick_Punch_v2"));
    CHECK(PresetManager::isValidPresetName("Bass 808 (loud)"));
    CHECK(PresetManager::isValidPresetName("preset-with-dashes"));
    CHECK(PresetManager::isValidPresetName("123"));
}

TEST_CASE("isValidPresetName rejects empty string",
          "[presets][QA-UX-010]")
{
    CHECK_FALSE(PresetManager::isValidPresetName(""));
}

TEST_CASE("isValidPresetName rejects path traversal",
          "[presets][QA-UX-010]")
{
    CHECK_FALSE(PresetManager::isValidPresetName(".."));
    CHECK_FALSE(PresetManager::isValidPresetName("."));
    CHECK_FALSE(PresetManager::isValidPresetName("foo/bar"));
    CHECK_FALSE(PresetManager::isValidPresetName("foo\\bar"));
}

TEST_CASE("isValidPresetName rejects reserved Windows device names",
          "[presets][QA-UX-010]")
{
    CHECK_FALSE(PresetManager::isValidPresetName("CON"));
    CHECK_FALSE(PresetManager::isValidPresetName("con"));
    CHECK_FALSE(PresetManager::isValidPresetName("PRN"));
    CHECK_FALSE(PresetManager::isValidPresetName("prn"));
    CHECK_FALSE(PresetManager::isValidPresetName("AUX"));
    CHECK_FALSE(PresetManager::isValidPresetName("NUL"));
    CHECK_FALSE(PresetManager::isValidPresetName("COM1"));
    CHECK_FALSE(PresetManager::isValidPresetName("com9"));
    CHECK_FALSE(PresetManager::isValidPresetName("LPT1"));
    CHECK_FALSE(PresetManager::isValidPresetName("lpt9"));
}

TEST_CASE("isValidPresetName rejects illegal filename characters",
          "[presets][QA-UX-010]")
{
    CHECK_FALSE(PresetManager::isValidPresetName("foo<bar"));
    CHECK_FALSE(PresetManager::isValidPresetName("foo>bar"));
    CHECK_FALSE(PresetManager::isValidPresetName("foo:bar"));
    CHECK_FALSE(PresetManager::isValidPresetName("foo\"bar"));
    CHECK_FALSE(PresetManager::isValidPresetName("foo|bar"));
    CHECK_FALSE(PresetManager::isValidPresetName("foo?bar"));
    CHECK_FALSE(PresetManager::isValidPresetName("foo*bar"));
}

// ============================================================================
// QA-CONF-003: loadPreset returns false on corrupt user preset
// ============================================================================

TEST_CASE("loadPreset returns false for corrupt user preset XML",
          "[presets][QA-CONF-003]")
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("mw160_test_load_corrupt");
    tempDir.createDirectory();

    // Write a file with a wrong root tag
    auto badFile = tempDir.getChildFile("BadPreset.xml");
    badFile.replaceWithText("<wrongTag>not a valid preset</wrongTag>");

    MW160Processor processor;
    PresetManager pm(processor.apvts, tempDir);

    REQUIRE(pm.getNumPresets() == mw160::kNumFactoryPresets + 1);

    const int badIndex = mw160::kNumFactoryPresets;
    CHECK_FALSE(pm.loadPreset(badIndex));

    // currentIndex should remain untouched (-1 at construction)
    CHECK(pm.getCurrentIndex() == -1);

    tempDir.deleteRecursively();
}

TEST_CASE("loadPreset returns false for unparseable XML file",
          "[presets][QA-CONF-003]")
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("mw160_test_load_garbage");
    tempDir.createDirectory();

    auto badFile = tempDir.getChildFile("Garbage.xml");
    badFile.replaceWithText("this is not xml at all {{{}}}");

    MW160Processor processor;
    PresetManager pm(processor.apvts, tempDir);

    const int badIndex = mw160::kNumFactoryPresets;
    CHECK_FALSE(pm.loadPreset(badIndex));

    tempDir.deleteRecursively();
}

TEST_CASE("loadPreset succeeds for valid user preset",
          "[presets][QA-CONF-003]")
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("mw160_test_load_valid");
    tempDir.createDirectory();

    // Save a valid preset, then load it back via a fresh manager
    MW160Processor processor;
    {
        PresetManager pm(processor.apvts, tempDir);
        auto result = pm.saveUserPreset("ValidPreset");
        REQUIRE(result == PresetManager::SaveResult::Ok);
    }

    PresetManager pm2(processor.apvts, tempDir);
    const int userIdx = mw160::kNumFactoryPresets;
    CHECK(pm2.loadPreset(userIdx));
    CHECK(pm2.getCurrentIndex() == userIdx);

    tempDir.deleteRecursively();
}

TEST_CASE("loadPreset succeeds for factory presets",
          "[presets][QA-CONF-003]")
{
    MW160Processor processor;
    PresetManager pm(processor.apvts);

    CHECK(pm.loadPreset(0));
    CHECK(pm.getCurrentIndex() == 0);
}

// ============================================================================
// QA-UX-010: saveUserPreset validation and duplicate detection
// ============================================================================

TEST_CASE("saveUserPreset returns InvalidName for bad names",
          "[presets][QA-UX-010]")
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("mw160_test_save_invalid");

    MW160Processor processor;
    PresetManager pm(processor.apvts, tempDir);

    CHECK(pm.saveUserPreset("") == PresetManager::SaveResult::InvalidName);
    CHECK(pm.saveUserPreset("..") == PresetManager::SaveResult::InvalidName);
    CHECK(pm.saveUserPreset("CON") == PresetManager::SaveResult::InvalidName);
    CHECK(pm.saveUserPreset("foo/bar") == PresetManager::SaveResult::InvalidName);
    CHECK(pm.saveUserPreset("bad*name") == PresetManager::SaveResult::InvalidName);

    tempDir.deleteRecursively();
}

TEST_CASE("saveUserPreset returns AlreadyExists on duplicate without overwrite",
          "[presets][QA-UX-010]")
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("mw160_test_save_dup");

    MW160Processor processor;
    PresetManager pm(processor.apvts, tempDir);

    REQUIRE(pm.saveUserPreset("TestPreset") == PresetManager::SaveResult::Ok);

    // Second save without allowOverwrite → AlreadyExists
    CHECK(pm.saveUserPreset("TestPreset") == PresetManager::SaveResult::AlreadyExists);

    tempDir.deleteRecursively();
}

TEST_CASE("saveUserPreset succeeds on duplicate with allowOverwrite",
          "[presets][QA-UX-010]")
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("mw160_test_save_overwrite");

    MW160Processor processor;
    PresetManager pm(processor.apvts, tempDir);

    REQUIRE(pm.saveUserPreset("TestPreset") == PresetManager::SaveResult::Ok);

    // Overwrite allowed → Ok
    CHECK(pm.saveUserPreset("TestPreset", true) == PresetManager::SaveResult::Ok);

    tempDir.deleteRecursively();
}

TEST_CASE("saveUserPreset writes valid XML that can be loaded back",
          "[presets][QA-UX-010]")
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("mw160_test_save_roundtrip");

    MW160Processor processor;
    PresetManager pm(processor.apvts, tempDir);

    REQUIRE(pm.saveUserPreset("Roundtrip") == PresetManager::SaveResult::Ok);

    // Re-scan and load
    pm.scanUserPresets();
    const int idx = mw160::kNumFactoryPresets;
    REQUIRE(pm.getNumPresets() == mw160::kNumFactoryPresets + 1);
    CHECK(pm.loadPreset(idx));

    tempDir.deleteRecursively();
}

// ============================================================================
// QA-CONF-004: pluginVersion attribute in saved state
// ============================================================================

TEST_CASE("getStateInformation includes pluginVersion attribute",
          "[plugin][state][QA-CONF-004]")
{
    MW160Processor processor;

    juce::MemoryBlock memoryBlock;
    processor.getStateInformation(memoryBlock);
    REQUIRE(memoryBlock.getSize() > 0);

    auto xml = processor.getXmlFromBinary(memoryBlock.getData(),
                                          static_cast<int>(memoryBlock.getSize()));
    REQUIRE(xml != nullptr);

    auto state = juce::ValueTree::fromXml(*xml);
    CHECK(static_cast<int>(state.getProperty("pluginVersion", 0)) == 1);
}

TEST_CASE("saveUserPreset includes pluginVersion in preset XML",
          "[presets][state][QA-CONF-004]")
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("mw160_test_version_preset");

    MW160Processor processor;
    PresetManager pm(processor.apvts, tempDir);
    REQUIRE(pm.saveUserPreset("Versioned") == PresetManager::SaveResult::Ok);

    auto presetFile = tempDir.getChildFile("Versioned.xml");
    REQUIRE(presetFile.existsAsFile());

    auto xml = juce::parseXML(presetFile);
    REQUIRE(xml != nullptr);

    auto state = juce::ValueTree::fromXml(*xml);
    CHECK(static_cast<int>(state.getProperty("pluginVersion", 0)) == 1);

    tempDir.deleteRecursively();
}

TEST_CASE("setStateInformation tolerates missing pluginVersion (legacy state)",
          "[plugin][state][QA-CONF-004]")
{
    MW160Processor processor;

    // Create a state WITHOUT pluginVersion (simulates legacy save)
    auto state = processor.apvts.copyState();
    // Do NOT set pluginVersion
    auto xml = state.createXml();
    REQUIRE(xml != nullptr);

    juce::MemoryBlock legacyBlock;
    juce::AudioProcessor::copyXmlToBinary(*xml, legacyBlock);

    // Should not crash or reject the state
    processor.setStateInformation(legacyBlock.getData(),
                                  static_cast<int>(legacyBlock.getSize()));

    // Verify parameters are still accessible
    auto* param = processor.apvts.getParameter("threshold");
    REQUIRE(param != nullptr);
}
