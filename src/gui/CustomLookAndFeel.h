#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// Hardware-inspired dark LookAndFeel for the MW160 compressor plugin.
/// Provides metallic rotary knobs with pointer lines and LED-style toggle buttons.
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, kBackground);
        setColour(juce::Slider::textBoxTextColourId,         kTextBright);
        setColour(juce::Slider::textBoxBackgroundColourId,   juce::Colour(0xff2a2a2a));
        setColour(juce::Slider::textBoxOutlineColourId,      juce::Colour(0xff3a3a3a));
        setColour(juce::Label::textColourId,                 kTextBright);
        setColour(juce::ToggleButton::textColourId,          kTextBright);

        // ComboBox colours
        setColour(juce::ComboBox::backgroundColourId,      juce::Colour(0xff2a2a2a));
        setColour(juce::ComboBox::textColourId,            kTextBright);
        setColour(juce::ComboBox::outlineColourId,         juce::Colour(0xff3a3a3a));
        setColour(juce::ComboBox::arrowColourId,           kAccent);

        // PopupMenu colours
        setColour(juce::PopupMenu::backgroundColourId,           kPanel);
        setColour(juce::PopupMenu::textColourId,                 kTextBright);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff404040));
        setColour(juce::PopupMenu::highlightedTextColourId,      kTextBright);

        // TextButton colours
        setColour(juce::TextButton::buttonColourId,        juce::Colour(0xff333333));
        setColour(juce::TextButton::textColourOffId,       kTextBright);
        setColour(juce::TextButton::buttonOnColourId,      juce::Colour(0xff404040));
        setColour(juce::TextButton::textColourOnId,        kTextBright);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& /*slider*/) override
    {
        const float radius    = juce::jmin(width, height) * 0.38f;
        const float centreX   = x + width * 0.5f;
        const float centreY   = y + height * 0.5f;
        const float angle     = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // --- Arc track (background) ---
        const float trackRadius = radius + 5.0f;
        {
            juce::Path arcBg;
            arcBg.addCentredArc(centreX, centreY, trackRadius, trackRadius,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(juce::Colour(0xff333333));
            g.strokePath(arcBg, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // --- Arc fill (active portion) ---
        {
            juce::Path arcFill;
            arcFill.addCentredArc(centreX, centreY, trackRadius, trackRadius,
                                  0.0f, rotaryStartAngle, angle, true);
            g.setColour(kAccent);
            g.strokePath(arcFill, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }

        // --- Outer ring ---
        g.setColour(juce::Colour(0xff3a3a3a));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        // --- Inner knob with gradient ---
        const float inner = radius * 0.82f;
        {
            juce::ColourGradient grad(juce::Colour(0xff5a5a5a), centreX, centreY - inner * 0.6f,
                                       juce::Colour(0xff383838), centreX, centreY + inner, false);
            g.setGradientFill(grad);
            g.fillEllipse(centreX - inner, centreY - inner, inner * 2.0f, inner * 2.0f);
        }

        // --- Ring border ---
        g.setColour(juce::Colour(0xff505050));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // --- Pointer line ---
        {
            const float ptrLen   = radius * 0.6f;
            const float ptrWidth = 2.5f;
            juce::Path ptr;
            ptr.addRoundedRectangle(-ptrWidth * 0.5f, -radius + 4.0f, ptrWidth, ptrLen, 1.0f);
            ptr.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
            g.setColour(kTextBright);
            g.fillPath(ptr);
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool /*shouldDrawButtonAsHighlighted*/,
                          bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        const float ledSize = 10.0f;
        const float pad     = 6.0f;

        auto ledRect = juce::Rectangle<float>(pad, (bounds.getHeight() - ledSize) * 0.5f,
                                               ledSize, ledSize);

        if (button.getToggleState())
        {
            // Glow
            g.setColour(kLedGreen.withAlpha(0.25f));
            g.fillEllipse(ledRect.expanded(3.0f));
            g.setColour(kLedGreen);
            g.fillEllipse(ledRect);
        }
        else
        {
            g.setColour(juce::Colour(0xff2a3a2a));
            g.fillEllipse(ledRect);
        }

        g.setColour(juce::Colour(0xff505050));
        g.drawEllipse(ledRect, 0.8f);

        // Label text
        auto textArea = bounds.withLeft(ledRect.getRight() + pad);
        g.setColour(button.getToggleState() ? kTextBright : kTextDim);
        g.setFont(juce::Font(13.0f));
        g.drawText(button.getButtonText(), textArea, juce::Justification::centredLeft);
    }

    // --- Colour constants ---
    static inline const juce::Colour kBackground { 0xff1e1e1e };
    static inline const juce::Colour kPanel      { 0xff282828 };
    static inline const juce::Colour kAccent     { 0xff888888 };
    static inline const juce::Colour kTextBright { 0xffe0e0e0 };
    static inline const juce::Colour kTextDim    { 0xff888888 };
    static inline const juce::Colour kLedGreen   { 0xff00cc44 };
    static inline const juce::Colour kLedYellow  { 0xffffcc00 };
    static inline const juce::Colour kLedRed     { 0xffee2200 };
    static inline const juce::Colour kLedAmber   { 0xffff8800 };
};
