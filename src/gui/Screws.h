#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"

/// Procedural corner-fastener drawing for the faceplate.
///
/// VISUAL_SPEC.md §9 describes four corner screws drawn directly in the
/// editor's `paint()` (not as child components). Each screw is five paint
/// operations and uses a deterministic per-index rotation so the four
/// screws look "tightened" rather than stamped at a uniform angle.
namespace mw160::Screws
{
    /// Per-screw rotation angles in degrees (indices 0..3 = LT, LB, RT, RB).
    /// VISUAL_SPEC.md §9.3 suggests these exact values.
    inline constexpr float kSlotAnglesDeg[4] = { 17.0f, -22.0f, 8.0f, -13.0f };

    /// Draw a single screw centred on `centre` with pixel radius `radius`
    /// and per-screw `index` (0..3) controlling the slot rotation.
    inline void drawScrew(juce::Graphics& g,
                          juce::Point<float> centre,
                          float radius,
                          int index)
    {
        using namespace mw160::Palette;

        const auto body = juce::Rectangle<float>(centre.x - radius,
                                                 centre.y - radius,
                                                 radius * 2.0f,
                                                 radius * 2.0f);

        // 1) Filled circle in screwBody.
        g.setColour(screwBody);
        g.fillEllipse(body);

        // 2) 1 px outer ring in borderHairline.
        g.setColour(borderHairline);
        g.drawEllipse(body, 1.0f);

        // 3) Inner radial gradient (top-to-bottom).
        {
            juce::ColourGradient grad(juce::Colour(0xff4a4a4a), centre.x, centre.y - radius,
                                      juce::Colour(0xff1a1a1a), centre.x, centre.y + radius,
                                      false);
            g.setGradientFill(grad);
            g.fillEllipse(body.reduced(1.2f));
        }

        // 4) Cross-slot -- two perpendicular rectangles, rotated per index.
        {
            const float slotLen  = radius * 1.2f;
            const float slotThk  = 1.2f;
            const int   idx      = juce::jlimit(0, 3, index);
            const float angleRad = juce::degreesToRadians(kSlotAnglesDeg[idx]);

            juce::Path slot;
            slot.addRoundedRectangle(-slotLen * 0.5f, -slotThk * 0.5f,
                                     slotLen, slotThk, 0.3f);
            slot.addRoundedRectangle(-slotThk * 0.5f, -slotLen * 0.5f,
                                     slotThk, slotLen, 0.3f);
            slot.applyTransform(juce::AffineTransform::rotation(angleRad)
                                    .translated(centre.x, centre.y));

            g.setColour(screwSlot);
            g.fillPath(slot);
        }

        // 5) Highlight arc on the top-left, suggesting an overhead light.
        {
            juce::Path hi;
            hi.addCentredArc(centre.x, centre.y, radius - 0.4f, radius - 0.4f,
                             0.0f,
                             juce::MathConstants<float>::pi * -0.85f,
                             juce::MathConstants<float>::pi * -0.25f,
                             true);
            g.setColour(screwHighlight.withAlpha(0.6f));
            g.strokePath(hi, juce::PathStrokeType(0.8f));
        }
    }
}
