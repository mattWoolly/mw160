#include "PluginEditor.h"

namespace
{
    void setupSlider(juce::Slider& slider, juce::Label& label,
                     const juce::String& labelText,
                     juce::AudioProcessorEditor& editor)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        editor.addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.attachToComponent(&slider, false);
        editor.addAndMakeVisible(label);
    }
}

MW160Editor::MW160Editor(MW160Processor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setupSlider(thresholdSlider, thresholdLabel, "Threshold", *this);
    setupSlider(ratioSlider, ratioLabel, "Ratio", *this);
    setupSlider(outputGainSlider, outputGainLabel, "Output Gain", *this);
    setupSlider(mixSlider, mixLabel, "Mix", *this);

    addAndMakeVisible(overEasyButton);
    addAndMakeVisible(stereoLinkButton);

    // Attachments must be created after the components are added
    thresholdAttachment  = std::make_unique<SliderAttachment>(processorRef.apvts, "threshold",  thresholdSlider);
    ratioAttachment      = std::make_unique<SliderAttachment>(processorRef.apvts, "ratio",      ratioSlider);
    outputGainAttachment = std::make_unique<SliderAttachment>(processorRef.apvts, "outputGain", outputGainSlider);
    mixAttachment        = std::make_unique<SliderAttachment>(processorRef.apvts, "mix",        mixSlider);
    overEasyAttachment   = std::make_unique<ButtonAttachment>(processorRef.apvts, "overEasy",   overEasyButton);
    stereoLinkAttachment = std::make_unique<ButtonAttachment>(processorRef.apvts, "stereoLink", stereoLinkButton);

    setSize(400, 300);
}

MW160Editor::~MW160Editor() = default;

void MW160Editor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);

    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawText("MW160", 0, 4, getWidth(), 20, juce::Justification::centred);
}

void MW160Editor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Title area
    bounds.removeFromTop(24);

    // Labels sit above sliders (attachToComponent handles this), reserve space
    bounds.removeFromTop(18);

    // Sliders area: 4 knobs in a row
    auto sliderArea = bounds.removeFromTop(120);
    const int sliderWidth = sliderArea.getWidth() / 4;

    thresholdSlider.setBounds(sliderArea.removeFromLeft(sliderWidth));
    ratioSlider.setBounds(sliderArea.removeFromLeft(sliderWidth));
    outputGainSlider.setBounds(sliderArea.removeFromLeft(sliderWidth));
    mixSlider.setBounds(sliderArea);

    // Spacing
    bounds.removeFromTop(16);

    // Toggle buttons
    auto buttonArea = bounds.removeFromTop(30);
    const int buttonWidth = buttonArea.getWidth() / 2;

    overEasyButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    stereoLinkButton.setBounds(buttonArea);
}
