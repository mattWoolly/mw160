#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// Small geometry utilities for the editor.
///
/// VISUAL_SPEC.md §10.3 requires 1 px hairlines to be snapped to device
/// pixels so they do not blur at non-integer scales.
namespace mw160::Geometry
{
    /// Snap a 1 px stroke width to whole device pixels given the host scale.
    inline float snap1px(float strokeWidth, float scale)
    {
        if (scale <= 0.0f)
            return strokeWidth;
        return juce::roundToInt(strokeWidth * scale) / scale;
    }
}
