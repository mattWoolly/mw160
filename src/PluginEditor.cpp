#include "PluginEditor.h"

namespace
{
    void setupKnob(juce::Slider& slider, juce::Label& label,
                   const juce::String& labelText,
                   juce::AudioProcessorEditor& editor)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
        editor.addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f));
        label.attachToComponent(&slider, false);
        editor.addAndMakeVisible(label);
    }

    void setupMeterLabel(juce::Label& label, const juce::String& text,
                         juce::Component& parent)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(10.0f).boldened());
        label.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        parent.addAndMakeVisible(label);
    }

    /// Draw a circular LED indicator with optional glow.
    void drawLed(juce::Graphics& g, juce::Rectangle<float> area,
                 juce::Colour colour, bool lit)
    {
        if (lit)
        {
            g.setColour(colour.withAlpha(0.2f));
            g.fillEllipse(area.expanded(2.5f));
            g.setColour(colour);
        }
        else
        {
            g.setColour(colour.withAlpha(0.15f));
        }
        g.fillEllipse(area);
        g.setColour(juce::Colour(0xff505050));
        g.drawEllipse(area, 0.6f);
    }
}

// =============================================================================

MW160Editor::MW160Editor(MW160Processor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&customLnf);

    setupKnob(thresholdSlider,  thresholdLabel,  "THRESHOLD",   *this);
    setupKnob(ratioSlider,      ratioLabel,      "COMPRESSION", *this);
    setupKnob(outputGainSlider, outputGainLabel, "OUTPUT",      *this);
    setupKnob(mixSlider,        mixLabel,        "MIX",         *this);

    addAndMakeVisible(overEasyButton);
    addAndMakeVisible(stereoLinkButton);

    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(grMeter);
    setupMeterLabel(inputMeterLabel,  "IN",  *this);
    setupMeterLabel(outputMeterLabel, "OUT", *this);
    setupMeterLabel(grMeterLabel,     "GR",  *this);

    // Create APVTS attachments after components are visible
    thresholdAttachment  = std::make_unique<SliderAttachment>(processorRef.apvts, "threshold",  thresholdSlider);
    ratioAttachment      = std::make_unique<SliderAttachment>(processorRef.apvts, "ratio",      ratioSlider);
    outputGainAttachment = std::make_unique<SliderAttachment>(processorRef.apvts, "outputGain", outputGainSlider);
    mixAttachment        = std::make_unique<SliderAttachment>(processorRef.apvts, "mix",        mixSlider);
    overEasyAttachment   = std::make_unique<ButtonAttachment>(processorRef.apvts, "overEasy",   overEasyButton);
    stereoLinkAttachment = std::make_unique<ButtonAttachment>(processorRef.apvts, "stereoLink", stereoLinkButton);

    setSize(600, 400);
    startTimerHz(30);
}

MW160Editor::~MW160Editor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// =============================================================================

void MW160Editor::timerCallback()
{
    constexpr float kDecay = 0.85f;

    // --- Input level (snap up, decay down) ---
    const float newIn = processorRef.getMeterInputLevel();
    displayedInputLevel_ = (newIn > displayedInputLevel_)
        ? newIn
        : std::max(newIn, displayedInputLevel_ * kDecay + (-100.0f) * (1.0f - kDecay));
    inputMeter.setLevel(displayedInputLevel_);

    // --- Output level ---
    const float newOut = processorRef.getMeterOutputLevel();
    displayedOutputLevel_ = (newOut > displayedOutputLevel_)
        ? newOut
        : std::max(newOut, displayedOutputLevel_ * kDecay + (-100.0f) * (1.0f - kDecay));
    outputMeter.setLevel(displayedOutputLevel_);

    // --- Gain reduction (snap deeper, decay toward 0) ---
    const float newGR = processorRef.getMeterGainReduction();
    displayedGR_ = (newGR < displayedGR_)
        ? newGR
        : displayedGR_ * kDecay;
    grMeter.setLevel(displayedGR_);

    // --- Threshold indicator LEDs ---
    const float threshold = processorRef.apvts.getRawParameterValue("threshold")->load();
    const bool  overEasy  = processorRef.apvts.getRawParameterValue("overEasy")->load() >= 0.5f;
    const float halfKnee  = overEasy ? 5.0f : 0.0f;  // 10 dB OverEasy knee / 2

    const bool wasAbove = ledAbove_;
    const bool wasBelow = ledBelow_;

    ledBelow_ = (displayedInputLevel_ < threshold + halfKnee);
    ledAbove_ = (displayedInputLevel_ > threshold - halfKnee);

    // Only repaint the header area if LED state changed
    if (wasAbove != ledAbove_ || wasBelow != ledBelow_)
        repaint(0, 0, getWidth(), 60);
}

// =============================================================================

void MW160Editor::paint(juce::Graphics& g)
{
    // --- Background ---
    g.fillAll(CustomLookAndFeel::kBackground);

    // --- Header panel ---
    auto headerBounds = getLocalBounds().removeFromTop(56).toFloat();
    {
        juce::ColourGradient headerGrad(
            juce::Colour(0xff2c2c2c), 0.0f, headerBounds.getY(),
            CustomLookAndFeel::kBackground, 0.0f, headerBounds.getBottom(), false);
        g.setGradientFill(headerGrad);
        g.fillRect(headerBounds);
    }

    // Title
    g.setColour(CustomLookAndFeel::kTextBright);
    g.setFont(juce::Font(24.0f).boldened());
    g.drawText("MW160", headerBounds.removeFromTop(32), juce::Justification::centred);

    // Subtitle
    g.setColour(CustomLookAndFeel::kTextDim);
    g.setFont(juce::Font(11.0f));
    g.drawText("VCA COMPRESSOR", headerBounds, juce::Justification::centred);

    // --- Separator line ---
    g.setColour(juce::Colour(0xff3a3a3a));
    g.fillRect(16.0f, 56.0f, static_cast<float>(getWidth() - 32), 1.0f);

    // --- Threshold indicator LEDs ---
    // Positioned to the right of the meter strip area
    const float ledY      = 80.0f;
    const float ledSize   = 8.0f;
    const float ledStartX = 230.0f;

    // BELOW led (green)
    drawLed(g, { ledStartX, ledY, ledSize, ledSize },
            CustomLookAndFeel::kLedGreen, ledBelow_);
    g.setColour(ledBelow_ ? CustomLookAndFeel::kTextBright : CustomLookAndFeel::kTextDim);
    g.setFont(juce::Font(10.0f));
    g.drawText("BELOW", ledStartX + ledSize + 4.0f, ledY - 1.0f, 42.0f, 12.0f,
               juce::Justification::centredLeft);

    // ABOVE led (red)
    const float aboveX = ledStartX + 60.0f;
    drawLed(g, { aboveX, ledY, ledSize, ledSize },
            CustomLookAndFeel::kLedRed, ledAbove_);
    g.setColour(ledAbove_ ? CustomLookAndFeel::kTextBright : CustomLookAndFeel::kTextDim);
    g.drawText("ABOVE", aboveX + ledSize + 4.0f, ledY - 1.0f, 42.0f, 12.0f,
               juce::Justification::centredLeft);

    // --- Knob section background panel ---
    auto knobPanel = juce::Rectangle<float>(8.0f, 248.0f,
                                             static_cast<float>(getWidth() - 16), 146.0f);
    g.setColour(CustomLookAndFeel::kPanel);
    g.fillRoundedRectangle(knobPanel, 4.0f);
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(knobPanel, 4.0f, 0.8f);
}

// =============================================================================

void MW160Editor::resized()
{
    // ----- Meter strip (left-center area, below header) -----
    const int meterTop     = 66;
    const int meterHeight  = 160;
    const int meterWidth   = 24;
    const int meterGap     = 14;
    const int labelHeight  = 14;
    const int meterStartX  = 40;

    // IN meter
    inputMeterLabel.setBounds(meterStartX, meterTop, meterWidth, labelHeight);
    inputMeter.setBounds(meterStartX, meterTop + labelHeight + 2, meterWidth, meterHeight);

    // OUT meter
    const int outX = meterStartX + meterWidth + meterGap;
    outputMeterLabel.setBounds(outX, meterTop, meterWidth, labelHeight);
    outputMeter.setBounds(outX, meterTop + labelHeight + 2, meterWidth, meterHeight);

    // GR meter
    const int grX = outX + meterWidth + meterGap + 10;
    grMeterLabel.setBounds(grX, meterTop, meterWidth, labelHeight);
    grMeter.setBounds(grX, meterTop + labelHeight + 2, meterWidth, meterHeight);

    // ----- Toggle buttons (right of meters) -----
    const int btnX = 230;
    const int btnW = 140;
    const int btnH = 24;
    overEasyButton.setBounds(btnX, 102, btnW, btnH);
    stereoLinkButton.setBounds(btnX, 130, btnW, btnH);

    // ----- Knob row (bottom section) -----
    const int knobAreaY = 254;
    const int knobLabelSpace = 16;  // space reserved above knobs for label
    const int knobHeight = 110;
    auto knobArea = juce::Rectangle<int>(16, knobAreaY + knobLabelSpace,
                                          getWidth() - 32, knobHeight);

    const int knobW = knobArea.getWidth() / 4;
    thresholdSlider.setBounds(knobArea.removeFromLeft(knobW));
    ratioSlider.setBounds(knobArea.removeFromLeft(knobW));
    outputGainSlider.setBounds(knobArea.removeFromLeft(knobW));
    mixSlider.setBounds(knobArea);
}
