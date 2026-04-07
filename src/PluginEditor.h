#pragma once

#include "PluginProcessor.h"

/// Simple vertical bar meter for level or gain reduction display.
class MeterComponent : public juce::Component
{
public:
    enum class Direction { BottomUp, TopDown };

    explicit MeterComponent(Direction dir = Direction::BottomUp)
        : direction_(dir) {}

    void setLevel(float newLevel_dB)
    {
        if (std::abs(newLevel_dB - level_dB_) > 0.01f)
        {
            level_dB_ = newLevel_dB;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRect(bounds);

        // Map dB to 0..1 proportion
        const float minDb = (direction_ == Direction::TopDown) ? -40.0f : -60.0f;
        const float proportion = juce::jlimit(0.0f, 1.0f,
            (direction_ == Direction::TopDown)
                ? level_dB_ / minDb               // 0 dB → 0.0 (empty), -40 dB → 1.0 (full)
                : (level_dB_ - minDb) / -minDb);  // -60 dB → 0.0 (empty), 0 dB → 1.0 (full)

        if (proportion > 0.0f)
        {
            const float barHeight = bounds.getHeight() * proportion;

            if (direction_ == Direction::BottomUp)
            {
                // Green below -12, yellow -12 to -3, red above -3
                auto barBounds = bounds.withTop(bounds.getBottom() - barHeight);
                g.setColour(level_dB_ > -3.0f  ? juce::Colours::red
                          : level_dB_ > -12.0f ? juce::Colours::yellow
                          :                       juce::Colours::green);
                g.fillRect(barBounds);
            }
            else // TopDown — for GR meter
            {
                auto barBounds = bounds.withBottom(barHeight);
                g.setColour(juce::Colour(0xffff8800)); // amber
                g.fillRect(barBounds);
            }
        }

        // Border
        g.setColour(juce::Colours::grey);
        g.drawRect(bounds, 1.0f);
    }

private:
    Direction direction_;
    float level_dB_ = -100.0f;
};

class MW160Editor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit MW160Editor(MW160Processor&);
    ~MW160Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

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

    // Meters
    MeterComponent inputMeter  { MeterComponent::Direction::BottomUp };
    MeterComponent outputMeter { MeterComponent::Direction::BottomUp };
    MeterComponent grMeter     { MeterComponent::Direction::TopDown };
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;
    juce::Label grMeterLabel;

    // Displayed meter levels (for smooth decay)
    float displayedInputLevel_  = -100.0f;
    float displayedOutputLevel_ = -100.0f;
    float displayedGR_          = 0.0f;

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
