#pragma once

#include "PluginProcessor.h"

class MW160Editor : public juce::AudioProcessorEditor
{
public:
    explicit MW160Editor(MW160Processor&);
    ~MW160Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MW160Processor& processorRef;

    // Sliders
    juce::Slider thresholdSlider;
    juce::Slider ratioSlider;
    juce::Slider outputGainSlider;
    juce::Slider mixSlider;

    // Labels
    juce::Label thresholdLabel;
    juce::Label ratioLabel;
    juce::Label outputGainLabel;
    juce::Label mixLabel;

    // Toggle buttons
    juce::ToggleButton overEasyButton { "OverEasy" };
    juce::ToggleButton stereoLinkButton { "Stereo Link" };

    // APVTS attachments
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
