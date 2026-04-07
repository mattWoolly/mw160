#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"
#include <atomic>

class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    int getNumPresets() const;
    juce::String getPresetName(int index) const;
    void loadPreset(int index);

    bool isFactoryPreset(int index) const;
    int getNumFactoryPresets() const { return mw160::kNumFactoryPresets; }

    void saveUserPreset(const juce::String& name);
    void deleteUserPreset(int index);
    void scanUserPresets();

    int getCurrentIndex() const { return currentIndex_.load(std::memory_order_relaxed); }
    void setCurrentIndex(int i) { currentIndex_.store(i, std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState& apvts_;
    std::atomic<int> currentIndex_ { -1 };

    juce::StringArray userPresetNames_;
    juce::Array<juce::File> userPresetFiles_;

    juce::File getUserPresetDirectory() const;
    void applyFactoryPreset(int factoryIndex);
    void loadUserPreset(int userIndex);
};
