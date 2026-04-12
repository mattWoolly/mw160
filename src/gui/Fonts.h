#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

/// Font helpers for the mw160 editor.
///
/// VISUAL_SPEC.md §4 prescribes Inter (for everything except numeric readouts)
/// and JetBrains Mono (for the four knob value readouts). Font files are
/// embedded via `juce_add_binary_data` in CMakeLists.txt and loaded here
/// using `Typeface::createSystemTypefaceFor`.
///
/// Tracking-to-kerning helper: the spec expresses letter spacing in pixels
/// ("+1", "+2"); JUCE's `Font::withExtraKerningFactor` takes a multiplier of
/// font size, so `kerning = extraPx / sizePt`.
namespace mw160::Fonts
{
    namespace detail
    {
        inline juce::Typeface::Ptr interRegularTypeface()
        {
            static auto tf = juce::Typeface::createSystemTypefaceFor(
                BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
            return tf;
        }

        inline juce::Typeface::Ptr interMediumTypeface()
        {
            static auto tf = juce::Typeface::createSystemTypefaceFor(
                BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
            return tf;
        }

        inline juce::Typeface::Ptr interBoldTypeface()
        {
            static auto tf = juce::Typeface::createSystemTypefaceFor(
                BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
            return tf;
        }

        inline juce::Typeface::Ptr jetBrainsMonoTypeface()
        {
            static auto tf = juce::Typeface::createSystemTypefaceFor(
                BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
            return tf;
        }
    }

    /// Returns Inter Regular at the given point size.
    inline juce::Font interRegular(float sizePt)
    {
        return juce::Font(detail::interRegularTypeface()).withHeight(sizePt);
    }

    /// Returns Inter Medium at the given point size.
    inline juce::Font interMedium(float sizePt)
    {
        return juce::Font(detail::interMediumTypeface()).withHeight(sizePt);
    }

    /// Returns Inter Bold at the given point size.
    inline juce::Font interBold(float sizePt)
    {
        return juce::Font(detail::interBoldTypeface()).withHeight(sizePt);
    }

    /// Returns JetBrains Mono Regular at the given point size.
    inline juce::Font jetBrainsMono(float sizePt)
    {
        return juce::Font(detail::jetBrainsMonoTypeface()).withHeight(sizePt);
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
