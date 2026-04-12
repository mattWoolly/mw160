#pragma once

#include "PluginProcessor.h"
#include "gui/CustomLookAndFeel.h"
#include "gui/LedMeter.h"
#include "gui/MeterModeButton.h"

/// Custom editor for the mw160 plugin.
///
/// Implements the 1U-rack-inspired visual language described in
/// `docs/VISUAL_SPEC.md`: brushed-metal faceplate header, three vertical
/// LED ladders, four rotary controls, a small switch cluster, and an
/// inline preset browser in the footer. All drawing is procedural
/// JUCE `Graphics` -- there are no raster assets.
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

    // Aspect-lock constrainer (VISUAL_SPEC.md §1.2).
    juce::ComponentBoundsConstrainer aspectConstrainer_;

    // --- Rotary knobs ---
    juce::Slider thresholdSlider;
    juce::Slider ratioSlider;
    juce::Slider outputGainSlider;
    juce::Slider mixSlider;

    juce::Label thresholdLabel;
    juce::Label ratioLabel;
    juce::Label outputGainLabel;
    juce::Label mixLabel;

    // --- Knob value readouts and unit captions ---
    juce::Label thresholdValue;
    juce::Label ratioValue;
    juce::Label outputGainValue;
    juce::Label mixValue;

    juce::Label thresholdUnit;
    juce::Label ratioUnit;
    juce::Label outputGainUnit;
    juce::Label mixUnit;

    // --- Preset controls ---
    juce::ComboBox presetBox;
    juce::TextButton saveButton   { "SAVE" };
    juce::TextButton deleteButton { "DEL" };
    juce::Label presetLabel;
    juce::Label versionLabel;

    void refreshPresetList();
    void onSavePreset();
    void onDeletePreset();
    void updateValueReadouts();

    // --- Toggle switches (KNEE / STEREO LINK / BYPASS) ---
    //
    // The KNEE switch's APVTS parameter ID remains "overEasy" for state
    // restore compatibility -- see the note at the parameter declaration
    // in `PluginProcessor.cpp`. The UI label is HARD / SOFT per
    // VISUAL_SPEC.md §7.4.
    juce::ToggleButton kneeButton       { "SOFT" };
    juce::ToggleButton stereoLinkButton { "ON" };
    juce::ToggleButton bypassButton     { "ON" };

    // --- Meter mode selector (IN | OUT | GR) ---
    MeterModeButton meterModeButton;

    // --- LED-segment meters ---
    LedMeter inputMeter  { LedMeter::Direction::BottomUp, 28 };
    LedMeter outputMeter { LedMeter::Direction::BottomUp, 28 };
    LedMeter grMeter     { LedMeter::Direction::TopDown,  20 };

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
    std::unique_ptr<ButtonAttachment> kneeAttachment;
    std::unique_ptr<ButtonAttachment> stereoLinkAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MW160Editor)
};
