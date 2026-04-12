#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"
#include "Fonts.h"

/// Three-state segmented selector: IN | OUT | GR.
///
/// VISUAL_SPEC.md §2.4 / §6.6 / §7.5 describes this control as a small
/// three-segment pill that lives in the switch cluster. In the current
/// 3-meter layout it does not hide ladders -- it only highlights the
/// corresponding meter's top label and tick column. This preserves the
/// "one meter, mode-switched" identity of the original hardware idiom and
/// keeps the door open for a future single-meter flavor variant.
class MeterModeButton : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    enum class Mode { In = 0, Out = 1, Gr = 2 };

    MeterModeButton()
    {
        setInterceptsMouseClicks(true, false);
    }

    void setMode(Mode m, juce::NotificationType notify = juce::sendNotification)
    {
        if (mode_ == m)
            return;
        mode_ = m;
        repaint();
        if (notify != juce::dontSendNotification && onModeChange)
            onModeChange(mode_);
    }

    Mode getMode() const noexcept { return mode_; }

    std::function<void(Mode)> onModeChange;

    void paint(juce::Graphics& g) override
    {
        using namespace mw160::Palette;

        // Centre a 36 px pill within the (possibly larger) hit-target bounds
        // for WCAG §11.1 compliance.
        auto bounds = getLocalBounds().toFloat()
                          .withSizeKeepingCentre(getLocalBounds().toFloat().getWidth(),
                                                 juce::jmin(getLocalBounds().toFloat().getHeight(), 36.0f));
        const float radius = 6.0f;

        // Outer pill.
        g.setColour(bgPanel);
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(borderHairline);
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);

        // Three equal segments.
        const float segW = bounds.getWidth() / 3.0f;
        const char* labels[3] = { "IN", "OUT", "GR" };
        for (int i = 0; i < 3; ++i)
        {
            auto seg = juce::Rectangle<float>(bounds.getX() + segW * i,
                                              bounds.getY(),
                                              segW,
                                              bounds.getHeight());
            const bool selected = (static_cast<int>(mode_) == i);
            const bool hover    = (hoverSeg_ == i);

            if (selected)
            {
                g.setColour(surfaceHigh);
                g.fillRoundedRectangle(seg.reduced(2.0f), radius - 2.0f);
            }
            else if (hover)
            {
                g.setColour(juce::Colour(0xff262626));
                g.fillRoundedRectangle(seg.reduced(2.0f), radius - 2.0f);
            }

            // Divider between segments (except the last).
            if (i < 2)
            {
                g.setColour(borderHairline);
                g.fillRect(seg.getRight() - 0.5f, seg.getY() + 4.0f,
                           1.0f, seg.getHeight() - 8.0f);
            }

            g.setColour(selected ? textBright : textDim);
            g.setFont(mw160::Fonts::withTracking(mw160::Fonts::interBold(10.0f), 1.0f));
            g.drawText(labels[i], seg, juce::Justification::centred);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int seg = hitSegment(e.position);
        if (seg != hoverSeg_)
        {
            hoverSeg_ = seg;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoverSeg_ != -1)
        {
            hoverSeg_ = -1;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int seg = hitSegment(e.position);
        if (seg >= 0)
            setMode(static_cast<Mode>(seg));
    }

private:
    int hitSegment(juce::Point<float> p) const
    {
        auto bounds = getLocalBounds().toFloat();
        if (! bounds.contains(p))
            return -1;
        const float segW = bounds.getWidth() / 3.0f;
        return juce::jlimit(0, 2, static_cast<int>((p.x - bounds.getX()) / segW));
    }

    Mode mode_     { Mode::In };
    int  hoverSeg_ { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterModeButton)
};
