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

    void setupMeterLabel(juce::Label& label, const juce::String& text,
                         juce::Component& parent)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(11.0f));
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        parent.addAndMakeVisible(label);
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

    // Meters
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(grMeter);
    setupMeterLabel(inputMeterLabel, "IN", *this);
    setupMeterLabel(outputMeterLabel, "OUT", *this);
    setupMeterLabel(grMeterLabel, "GR", *this);

    // Attachments must be created after the components are added
    thresholdAttachment  = std::make_unique<SliderAttachment>(processorRef.apvts, "threshold",  thresholdSlider);
    ratioAttachment      = std::make_unique<SliderAttachment>(processorRef.apvts, "ratio",      ratioSlider);
    outputGainAttachment = std::make_unique<SliderAttachment>(processorRef.apvts, "outputGain", outputGainSlider);
    mixAttachment        = std::make_unique<SliderAttachment>(processorRef.apvts, "mix",        mixSlider);
    overEasyAttachment   = std::make_unique<ButtonAttachment>(processorRef.apvts, "overEasy",   overEasyButton);
    stereoLinkAttachment = std::make_unique<ButtonAttachment>(processorRef.apvts, "stereoLink", stereoLinkButton);

    setSize(500, 330);

    startTimerHz(30);
}

MW160Editor::~MW160Editor()
{
    stopTimer();
}

void MW160Editor::timerCallback()
{
    constexpr float decayRate = 0.85f;    // per-tick decay for level meters
    constexpr float grDecayRate = 0.85f;  // per-tick decay for GR (towards 0)

    // Input level
    const float newInput = processorRef.getMeterInputLevel();
    displayedInputLevel_ = (newInput > displayedInputLevel_)
                               ? newInput
                               : std::max(newInput, displayedInputLevel_ * decayRate + (-100.0f) * (1.0f - decayRate));
    inputMeter.setLevel(displayedInputLevel_);

    // Output level
    const float newOutput = processorRef.getMeterOutputLevel();
    displayedOutputLevel_ = (newOutput > displayedOutputLevel_)
                                ? newOutput
                                : std::max(newOutput, displayedOutputLevel_ * decayRate + (-100.0f) * (1.0f - decayRate));
    outputMeter.setLevel(displayedOutputLevel_);

    // Gain reduction (negative values; decay towards 0)
    const float newGR = processorRef.getMeterGainReduction();
    displayedGR_ = (newGR < displayedGR_)
                       ? newGR   // deeper GR — snap immediately
                       : displayedGR_ * grDecayRate;  // releasing — decay towards 0
    grMeter.setLevel(displayedGR_);
}

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

    // Meter strip on the right side
    auto meterStrip = bounds.removeFromRight(90);
    meterStrip.removeFromTop(18); // align with slider label space

    const int meterWidth = 20;
    const int meterSpacing = 10;
    const int totalMetersWidth = 3 * meterWidth + 2 * meterSpacing;
    const int meterXOffset = (meterStrip.getWidth() - totalMetersWidth) / 2;
    auto meterArea = meterStrip.withTrimmedLeft(meterXOffset).withWidth(totalMetersWidth);

    // Labels above meters
    auto meterLabelArea = meterArea.removeFromTop(16);
    inputMeterLabel.setBounds(meterLabelArea.removeFromLeft(meterWidth));
    meterLabelArea.removeFromLeft(meterSpacing);
    outputMeterLabel.setBounds(meterLabelArea.removeFromLeft(meterWidth));
    meterLabelArea.removeFromLeft(meterSpacing);
    grMeterLabel.setBounds(meterLabelArea.removeFromLeft(meterWidth));

    // Meter bars
    auto meterBarArea = meterArea.removeFromTop(140);
    inputMeter.setBounds(meterBarArea.removeFromLeft(meterWidth));
    meterBarArea.removeFromLeft(meterSpacing);
    outputMeter.setBounds(meterBarArea.removeFromLeft(meterWidth));
    meterBarArea.removeFromLeft(meterSpacing);
    grMeter.setBounds(meterBarArea.removeFromLeft(meterWidth));

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
