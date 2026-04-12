#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"

/// Vertical LED-ladder meter for the mw160 editor.
///
/// VISUAL_SPEC.md §6 prescribes segmented LED ladders for IN, OUT, and GR
/// rendering on this classic VCA feedback compressor. Segment counts and
/// dB ranges are taken directly from §6.2. Only vertical ladders are
/// supported -- the reserved cream-face sweep meter is explicitly forbidden
/// in the baseline editor.
class LedMeter : public juce::Component
{
public:
    enum class Direction { BottomUp, TopDown };

    /// `numSegments` defaults to 28 (IN/OUT); use 20 for the GR ladder.
    explicit LedMeter(Direction dir = Direction::BottomUp, int numSegments = 28)
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
        using namespace mw160::Palette;

        auto bounds = getLocalBounds().toFloat();

        // --- Recessed background (VISUAL_SPEC.md §6.2 inset) ---
        g.setColour(bgDeep);
        g.fillRoundedRectangle(bounds, 2.0f);
        g.setColour(borderRecessed);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);

        auto inner = bounds.reduced(3.0f);

        const float segH = (inner.getHeight() - (numSegments_ - 1) * kGap) / numSegments_;

        // Range mapping per VISUAL_SPEC.md §6.2.
        float minDb, maxDb;
        if (direction_ == Direction::TopDown)
        {
            // GR ladder: 0 dB = nothing lit, -20 dB = fully lit.
            minDb = 0.0f;
            maxDb = -20.0f;
        }
        else
        {
            // IN/OUT ladder: -60 dBFS .. +6 dBFS = 66 dB / 28 segments.
            minDb = -60.0f;
            maxDb = +6.0f;
        }

        const float range = maxDb - minDb;
        const float norm  = juce::jlimit(0.0f, 1.0f, (level_dB_ - minDb) / range);
        const int   litCount = static_cast<int>(norm * numSegments_ + 0.5f);

        for (int i = 0; i < numSegments_; ++i)
        {
            // Segment position: i=0 is at the "origin" end of the meter.
            float segY;
            if (direction_ == Direction::BottomUp)
                segY = inner.getBottom() - (i + 1) * (segH + kGap) + kGap;
            else
                segY = inner.getY() + i * (segH + kGap);

            auto segBounds = juce::Rectangle<float>(inner.getX() + 1.0f, segY,
                                                    inner.getWidth() - 2.0f, segH);
            const bool isLit = (i < litCount);

            // Choose lit color per §6.2.
            juce::Colour litColor;
            juce::Colour unlitColor;
            if (direction_ == Direction::TopDown)
            {
                // GR ladder -- amber for 0..-6 dB, red for deeper GR.
                const float segDb = minDb + (static_cast<float>(i + 0.5f) / numSegments_) * range;
                if (segDb > -6.0f)
                {
                    litColor   = ledAmber;
                    unlitColor = ledAmberDark;
                }
                else
                {
                    litColor   = ledRed;
                    unlitColor = ledRedDark;
                }
            }
            else
            {
                const float segDb = minDb + (static_cast<float>(i + 0.5f) / numSegments_) * range;
                if (segDb > -3.0f)
                {
                    litColor   = ledRed;
                    unlitColor = ledRedDark;
                }
                else if (segDb > -12.0f)
                {
                    litColor   = ledYellow;
                    unlitColor = ledYellowDark;
                }
                else
                {
                    litColor   = ledGreen;
                    unlitColor = ledGreenDark;
                }
            }

            if (isLit)
            {
                // Per-segment 1 px outer halo (§6.2 "emissive" appearance).
                g.setColour(litColor.withAlpha(0.30f));
                g.fillRoundedRectangle(segBounds.expanded(1.0f, 0.5f), 1.0f);
                g.setColour(litColor);
                g.fillRoundedRectangle(segBounds, 1.0f);
            }
            else
            {
                g.setColour(unlitColor);
                g.fillRoundedRectangle(segBounds, 1.0f);
                g.setColour(borderHairline);
                g.drawRoundedRectangle(segBounds, 1.0f, 0.5f);
            }
        }
    }

private:
    Direction direction_;
    int       numSegments_;
    float     level_dB_ = -100.0f;

    static constexpr float kGap = 2.0f;
};
