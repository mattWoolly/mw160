#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// LED-segment bar meter inspired by the dbx 160A's LED arrays.
/// Renders individual segments that light up according to the current level.
/// Supports BottomUp (input/output level) and TopDown (gain reduction) directions.
class LedMeter : public juce::Component
{
public:
    enum class Direction { BottomUp, TopDown };

    explicit LedMeter(Direction dir = Direction::BottomUp, int numSegments = 20)
        : direction_(dir), numSegments_(numSegments) {}

    void setLevel(float newLevel_dB)
    {
        if (std::abs(newLevel_dB - level_dB_) > 0.05f)
        {
            level_dB_ = newLevel_dB;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const float segH = (bounds.getHeight() - (numSegments_ - 1) * kGap) / numSegments_;

        // Range mapping
        float minDb, maxDb;
        if (direction_ == Direction::TopDown)
        {
            // GR meter: 0 dB = nothing lit, -40 dB = fully lit
            minDb = 0.0f;
            maxDb = -40.0f;
        }
        else
        {
            minDb = -60.0f;
            maxDb = +6.0f;
        }

        const float range = maxDb - minDb;
        const float norm  = juce::jlimit(0.0f, 1.0f, (level_dB_ - minDb) / range);
        const int   litCount = static_cast<int>(norm * numSegments_ + 0.5f);

        for (int i = 0; i < numSegments_; ++i)
        {
            // Segment position: i=0 is at the "origin" end of the meter
            float segY;
            if (direction_ == Direction::BottomUp)
                segY = bounds.getBottom() - (i + 1) * (segH + kGap) + kGap;
            else
                segY = bounds.getY() + i * (segH + kGap);

            auto segBounds = juce::Rectangle<float>(bounds.getX(), segY,
                                                     bounds.getWidth(), segH);
            bool isLit = (i < litCount);

            // Segment colour based on position in the range
            juce::Colour litColour;
            if (direction_ == Direction::TopDown)
            {
                litColour = kAmber;
            }
            else
            {
                float segDb = minDb + (static_cast<float>(i + 0.5f) / numSegments_) * range;
                if (segDb > -3.0f)
                    litColour = kRed;
                else if (segDb > -12.0f)
                    litColour = kYellow;
                else
                    litColour = kGreen;
            }

            g.setColour(isLit ? litColour : litColour.withAlpha(0.10f));
            g.fillRoundedRectangle(segBounds, 1.0f);
        }
    }

private:
    Direction direction_;
    int       numSegments_;
    float     level_dB_ = -100.0f;

    static constexpr float kGap = 1.5f;

    static inline const juce::Colour kGreen  { 0xff00cc44 };
    static inline const juce::Colour kYellow { 0xffffcc00 };
    static inline const juce::Colour kRed    { 0xffee2200 };
    static inline const juce::Colour kAmber  { 0xffff8800 };
};
