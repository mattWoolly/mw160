#pragma once

#include "PluginProcessor.h"
#include "gui/CustomLookAndFeel.h"
#include "gui/LedMeter.h"

/// Custom GUI editor for the MW160 compressor plugin.
/// Hardware-inspired design with LED-segment meters, metallic knobs,
/// and threshold indicator LEDs modeled after the dbx 160A.
class MW160Editor : public juce::AudioProcessorEditor,
                    private juce::Timer,
                    private juce::ComboBox::Listener
{
public:
    explicit MW160Editor(MW160Processor&);
    ~MW160Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    MW160Processor& processorRef;

    CustomLookAndFeel customLnf;

    // --- Rotary knobs ---
    juce::Slider thresholdSlider;
    juce::Slider ratioSlider;
    juce::Slider outputGainSlider;
    juce::Slider mixSlider;

    juce::Label thresholdLabel;
    juce::Label ratioLabel;
    juce::Label outputGainLabel;
    juce::Label mixLabel;

    // --- Preset controls ---
    juce::ComboBox presetBox;
    juce::TextButton saveButton  { "Save" };
    juce::TextButton deleteButton { "Del" };
    juce::Label presetLabel;

    void refreshPresetList();
    void onSavePreset();
    void onDeletePreset();

    // --- Toggle buttons ---
    juce::ToggleButton overEasyButton  { "OverEasy" };
    juce::ToggleButton stereoLinkButton { "Stereo Link" };

    // --- LED-segment meters ---
    LedMeter inputMeter  { LedMeter::Direction::BottomUp, 20 };
    LedMeter outputMeter { LedMeter::Direction::BottomUp, 20 };
    LedMeter grMeter     { LedMeter::Direction::TopDown,  16 };

    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;
    juce::Label grMeterLabel;

    // --- Threshold indicator LED state ---
    bool ledAbove_ = false;
    bool ledBelow_ = true;

    // --- Meter smoothing state ---
    float displayedInputLevel_  = -100.0f;
    float displayedOutputLevel_ = -100.0f;
    float displayedGR_          = 0.0f;

    // --- APVTS attachments ---
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> ratioAttachment;
    std::unique_ptr<SliderAttachment> outputGainAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<ButtonAttachment> overEasyAttachment;
    std::unique_ptr<ButtonAttachment> stereoLinkAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MW160Editor)
};
