#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"

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

    int getCurrentIndex() const { return currentIndex_; }
    void setCurrentIndex(int i) { currentIndex_ = i; }

private:
    juce::AudioProcessorValueTreeState& apvts_;
    int currentIndex_ = -1;

    juce::StringArray userPresetNames_;
    juce::Array<juce::File> userPresetFiles_;

    juce::File getUserPresetDirectory() const;
    void applyFactoryPreset(int factoryIndex);
    void loadUserPreset(int userIndex);
};
