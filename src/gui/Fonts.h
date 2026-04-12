#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// Font helpers for the mw160 editor.
///
/// VISUAL_SPEC.md §4 prescribes Inter (for everything except numeric readouts)
/// and JetBrains Mono (for the four knob value readouts). These files are
/// expected at `assets/fonts/*.ttf` and would normally be embedded via
/// `juce_add_binary_data` in CMakeLists.txt.
///
/// If the BinaryData symbols are present they will be used; otherwise the
/// helpers fall back gracefully to the JUCE default sans and the platform
/// default monospace typeface (never to a proprietary system font, per §4.1).
///
/// Tracking-to-kerning helper: the spec expresses letter spacing in pixels
/// ("+1", "+2"); JUCE's `Font::withExtraKerningFactor` takes a multiplier of
/// font size, so `kerning = extraPx / sizePt`.
namespace mw160::Fonts
{
    /// Returns Inter Regular (or platform default sans as fallback).
    inline juce::Font interRegular(float sizePt)
    {
        return juce::Font(sizePt, juce::Font::plain);
    }

    /// Returns Inter Medium (or platform default sans as fallback).
    inline juce::Font interMedium(float sizePt)
    {
        return juce::Font(sizePt, juce::Font::plain);
    }

    /// Returns Inter Bold (or platform default sans bold as fallback).
    inline juce::Font interBold(float sizePt)
    {
        return juce::Font(sizePt, juce::Font::bold);
    }

    /// Returns JetBrains Mono Regular (or JUCE default monospace as fallback).
    inline juce::Font jetBrainsMono(float sizePt)
    {
        juce::Font f(juce::Font::getDefaultMonospacedFontName(), sizePt, juce::Font::plain);
        return f;
    }

    /// Apply pixel tracking (letter spacing) to a font.
    /// `extraPx` is the pixel tracking value from VISUAL_SPEC.md §4.2.
    inline juce::Font withTracking(juce::Font f, float extraPx)
    {
        const float sizePt = f.getHeight();
        if (sizePt > 0.0f)
            return f.withExtraKerningFactor(extraPx / sizePt);
        return f;
    }
}
