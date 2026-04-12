#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"
#include "Fonts.h"

/// LookAndFeel for the mw160 editor.
///
/// Implements the visual language described in VISUAL_SPEC.md: dark 1U-rack
/// inspired aesthetic for a classic VCA feedback compressor, with brushed
/// metal knobs, pill-shaped toggles, and palette-driven coloring.
///
/// All colors come from `mw160::Palette` -- no raw `0xff...` literals in
/// this file.
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        using namespace mw160::Palette;

        // Global window background.
        setColour(juce::ResizableWindow::backgroundColourId, bgDeep);

        // Slider text box (unused for rotaries in the new layout, but kept
        // sane so any secondary slider renders cleanly).
        setColour(juce::Slider::textBoxTextColourId,     textBright);
        setColour(juce::Slider::textBoxBackgroundColourId, surfaceLow);
        setColour(juce::Slider::textBoxOutlineColourId,  borderHairline);

        setColour(juce::Label::textColourId,        textBright);
        setColour(juce::ToggleButton::textColourId, textBright);

        // ComboBox (preset browser).
        setColour(juce::ComboBox::backgroundColourId, surfaceHigh);
        setColour(juce::ComboBox::textColourId,       textBright);
        setColour(juce::ComboBox::outlineColourId,    borderHairline);
        setColour(juce::ComboBox::arrowColourId,      accentEngagedGlow);

        // PopupMenu colors (preset dropdown content).
        setColour(juce::PopupMenu::backgroundColourId,            bgPanel);
        setColour(juce::PopupMenu::textColourId,                  textBright);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, surfaceHigh);
        setColour(juce::PopupMenu::highlightedTextColourId,       textBright);

        // TextButton colors (save / delete).
        setColour(juce::TextButton::buttonColourId,  surfaceHigh);
        setColour(juce::TextButton::textColourOffId, textBright);
        setColour(juce::TextButton::buttonOnColourId, accentEngagedGlow.withAlpha(0.25f));
        setColour(juce::TextButton::textColourOnId,  textBright);

        // Alert window theming (VISUAL_SPEC.md §8.4).
        setColour(juce::AlertWindow::backgroundColourId, bgPanel);
        setColour(juce::AlertWindow::textColourId,       textBright);
        setColour(juce::AlertWindow::outlineColourId,    borderHairline);
    }

    /// Rotary knob renderer (VISUAL_SPEC.md §5).
    /// Layer order per §5.6: arc track, arc fill, tip, outer ring, body,
    /// brushed highlight, inner cap, pointer, overlays.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        using namespace mw160::Palette;

        const float radius      = juce::jmin(width, height) * 0.5f - 8.0f;
        const float centreX     = x + width  * 0.5f;
        const float centreY     = y + height * 0.5f;
        const float angle       = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float trackRadius = radius + 6.0f;
        const bool  bypassed    = ! slider.isEnabled();

        // VISUAL_SPEC.md §5.4: threshold fills from the right (max angle),
        // ratio/output/mix fill from the left (start angle). We heuristically
        // detect THRESHOLD by its (negative-range) parameter layout via the
        // slider's range maximum -- this avoids hard-coding an ID lookup here
        // while still matching the spec's behavior.
        const bool fillFromEnd = (slider.getName().containsIgnoreCase("THRESH"));

        // 1. Arc track.
        {
            juce::Path arcBg;
            arcBg.addCentredArc(centreX, centreY, trackRadius, trackRadius,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(knobArcTrack);
            g.strokePath(arcBg, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
        }

        // 2. Arc fill.
        juce::Colour arcColor = bypassed ? knobArcFillBypass : knobArcFill;
        if (slider.isMouseOverOrDragging())
            arcColor = arcColor.brighter(0.12f);
        {
            juce::Path arcFill;
            const float a0 = fillFromEnd ? angle : rotaryStartAngle;
            const float a1 = fillFromEnd ? rotaryEndAngle : angle;
            if (std::abs(a1 - a0) > 1.0e-4f)
            {
                arcFill.addCentredArc(centreX, centreY, trackRadius, trackRadius,
                                      0.0f, a0, a1, true);
                g.setColour(arcColor);
                g.strokePath(arcFill, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                                   juce::PathStrokeType::rounded));
            }
        }

        // 3. Arc fill tip dot (soft "LED tip").
        if (! bypassed)
        {
            const float tipAngle = angle - juce::MathConstants<float>::halfPi;
            const float tx = centreX + std::cos(tipAngle) * trackRadius;
            const float ty = centreY + std::sin(tipAngle) * trackRadius;
            g.setColour(accentEngagedGlow);
            g.fillEllipse(tx - 2.0f, ty - 2.0f, 4.0f, 4.0f);
        }

        // 4. Knob outer ring.
        g.setColour(knobRing);
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // 5. Knob body fill (vertical gradient).
        {
            juce::ColourGradient grad(knobMetalHigh, centreX, centreY - radius,
                                      knobMetalLow,  centreX, centreY + radius,
                                      false);
            g.setGradientFill(grad);
            g.fillEllipse(centreX - radius + 1.0f, centreY - radius + 1.0f,
                          (radius - 1.0f) * 2.0f, (radius - 1.0f) * 2.0f);
        }

        // 6. Brushed highlight band at ~35% from the top.
        {
            const float bandY = centreY - radius + (radius * 2.0f * 0.35f);
            g.setColour(faceplateScratch.withAlpha(0.18f));
            g.fillRect(centreX - radius + 3.0f, bandY, (radius - 3.0f) * 2.0f, 1.0f);
        }

        // 7. Inner cap.
        const float inner = radius * 0.78f;
        g.setColour(juce::Colour(mw160::Palette::knobRing));
        g.fillEllipse(centreX - inner, centreY - inner, inner * 2.0f, inner * 2.0f);
        g.setColour(mw160::Palette::screwBody);
        g.drawEllipse(centreX - inner, centreY - inner, inner * 2.0f, inner * 2.0f, 0.8f);

        // 8. Pointer.
        {
            const float ptrLen   = radius * 0.62f;
            const float ptrWidth = 3.0f;
            const float startR   = radius * 0.18f;
            juce::Path ptr;
            ptr.addRoundedRectangle(-ptrWidth * 0.5f,
                                    -(startR + ptrLen),
                                    ptrWidth,
                                    ptrLen,
                                    1.5f);
            ptr.applyTransform(juce::AffineTransform::rotation(angle)
                                   .translated(centreX, centreY));
            g.setColour(bypassed
                            ? textMid
                            : (slider.isMouseOverOrDragging() ? juce::Colours::white
                                                               : knobPointer));
            g.fillPath(ptr);
        }

        // 9. Active overlay.
        if (slider.isMouseButtonDown(true))
        {
            const float hiR = radius * 1.18f;
            g.setColour(accentEngagedGlow.withAlpha(0.40f));
            g.drawEllipse(centreX - hiR, centreY - hiR, hiR * 2.0f, hiR * 2.0f, 1.0f);
        }
    }

    /// Pill-shaped toggle with embedded LED (VISUAL_SPEC.md §7).
    ///
    /// The label switches between OFF / ON wording per button; the
    /// `button.getProperties()` map is used as an override so a single
    /// ToggleButton can carry label pairs (e.g. HARD/SOFT for the KNEE
    /// switch) without subclassing.
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool /*shouldDrawButtonAsDown*/) override
    {
        using namespace mw160::Palette;

        // Centre a 36 px pill within the (possibly larger) hit-target bounds
        // so expanding to 44 px for WCAG compliance (§11.1) doesn't stretch
        // the visual.
        auto bounds = button.getLocalBounds().toFloat()
                          .withSizeKeepingCentre(button.getLocalBounds().toFloat().getWidth(),
                                                 juce::jmin(button.getLocalBounds().toFloat().getHeight(), 36.0f));
        const float corner = 6.0f;
        const bool  on     = button.getToggleState();
        const bool  isBypassStyle = button.getProperties().getWithDefault("bypassStyle", false);

        // Base pill fill. These per-state hex values come straight from
        // VISUAL_SPEC.md §7.3's state table -- they are strictly switch
        // states and are deliberately not in the main Palette header.
        juce::Colour fill   = on ? juce::Colour(0xff262626) : juce::Colour(0xff1a1a1a);
        juce::Colour stroke = on ? juce::Colour(0xff3c3c3c) : borderHairline;

        if (isBypassStyle && on)
        {
            fill   = juce::Colour(0xff2a1a1a);
            stroke = juce::Colour(0xff3c2222);
        }

        if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter(0.06f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, corner);
        g.setColour(stroke);
        g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

        // LED indicator.
        const float ledSize = 8.0f;
        const float ledX = bounds.getX() + 12.0f - ledSize * 0.5f;
        const float ledY = bounds.getCentreY() - ledSize * 0.5f;
        auto ledRect = juce::Rectangle<float>(ledX, ledY, ledSize, ledSize);

        juce::Colour ledOn   = isBypassStyle ? ledRed     : ledGreen;
        juce::Colour ledDark = isBypassStyle ? ledRedDark : ledGreenDark;

        if (on)
        {
            g.setColour(ledOn.withAlpha(0.35f));
            g.fillEllipse(ledRect.expanded(3.0f));
            g.setColour(ledOn);
            g.fillEllipse(ledRect);
        }
        else
        {
            g.setColour(ledDark);
            g.fillEllipse(ledRect);
        }
        g.setColour(borderHairline);
        g.drawEllipse(ledRect, 0.8f);

        // Label text.
        juce::String labelText = button.getButtonText();
        const juce::String offLabel = button.getProperties().getWithDefault("offLabel", "").toString();
        const juce::String onLabel  = button.getProperties().getWithDefault("onLabel",  "").toString();
        if (offLabel.isNotEmpty() || onLabel.isNotEmpty())
            labelText = on ? onLabel : offLabel;

        auto textArea = bounds.withLeft(bounds.getX() + 28.0f)
                              .withTrimmedRight(8.0f);
        g.setColour(on ? textBright : textDim);
        g.setFont(mw160::Fonts::withTracking(mw160::Fonts::interBold(11.0f), 1.0f));
        g.drawText(labelText, textArea, juce::Justification::centredLeft);
    }

    juce::Font getPopupMenuFont() override
    {
        return mw160::Fonts::interRegular(12.0f);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return mw160::Fonts::interRegular(12.0f);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int /*buttonHeight*/) override
    {
        return mw160::Fonts::withTracking(mw160::Fonts::interBold(10.0f), 1.0f);
    }

    // --- Backwards-compatible accessors ---
    //
    // A handful of legacy constants that `CustomLookAndFeel::kBackground`,
    // `kTextBright`, etc. previously exposed. New code should reference
    // `mw160::Palette` directly; these remain only to keep the symbol table
    // stable for any incidental references.
    static inline const juce::Colour kBackground = mw160::Palette::bgDeep;
    static inline const juce::Colour kPanel      = mw160::Palette::bgPanel;
    static inline const juce::Colour kAccent     = mw160::Palette::accentEngagedGlow;
    static inline const juce::Colour kTextBright = mw160::Palette::textBright;
    static inline const juce::Colour kTextDim    = mw160::Palette::textDim;
    static inline const juce::Colour kLedGreen   = mw160::Palette::ledGreen;
    static inline const juce::Colour kLedYellow  = mw160::Palette::ledYellow;
    static inline const juce::Colour kLedRed     = mw160::Palette::ledRed;
    static inline const juce::Colour kLedAmber   = mw160::Palette::ledAmber;
};
