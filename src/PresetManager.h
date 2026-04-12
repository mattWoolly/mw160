#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"
#include <atomic>

class PresetManager
{
public:
    enum class SaveResult { Ok, InvalidName, AlreadyExists, WriteFailed };

    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);
    PresetManager(juce::AudioProcessorValueTreeState& apvts,
                  const juce::File& presetDir);

    int getNumPresets() const;
    juce::String getPresetName(int index) const;

    /// Load a preset by index. Returns true on success, false if the
    /// preset file is corrupt or the index is out of range.
    bool loadPreset(int index);

    bool isFactoryPreset(int index) const;
    int getNumFactoryPresets() const { return mw160::kNumFactoryPresets; }

    /// Save the current APVTS state as a user preset.
    /// Returns Ok on success, or a specific failure reason.
    /// When allowOverwrite is false and a file already exists, returns
    /// AlreadyExists so the caller can prompt for confirmation.
    SaveResult saveUserPreset(const juce::String& name,
                              bool allowOverwrite = false);

    void deleteUserPreset(int index);
    void scanUserPresets();

    int getCurrentIndex() const { return currentIndex_.load(std::memory_order_relaxed); }
    void setCurrentIndex(int i) { currentIndex_.store(i, std::memory_order_relaxed); }

    /// Validate a preset name for filesystem safety. Rejects empty names,
    /// path traversal, reserved Windows device names, and illegal characters.
    static bool isValidPresetName(const juce::String& name);

private:
    juce::AudioProcessorValueTreeState& apvts_;
    std::atomic<int> currentIndex_ { -1 };
    juce::File presetDir_;

    juce::StringArray userPresetNames_;
    juce::Array<juce::File> userPresetFiles_;

    juce::File getUserPresetDirectory() const;
    void applyFactoryPreset(int factoryIndex);
    bool loadUserPreset(int userIndex);
};
