#pragma once

namespace mw160 {

/// Hard-knee gain computer for the DBX 160 compression pipeline.
/// Converts an input level (dB) to a gain reduction value (dB) given
/// threshold and ratio parameters.
///
/// Ratio semantics (matching the 160A control range):
///   1.0       = no compression (passthrough)
///   1.0–59.9  = normal compression
///   60.0      = infinity:1 (limiter — output pinned at threshold)
///   >60.0     = negative ratios (output decreases as input rises above threshold)
///
/// Pure C++ — no JUCE dependency — no heap allocation.
class GainComputer
{
public:
    /// Compute the gain reduction in dB for a given input level.
    /// @param inputLevel_dB  detected input level in dB
    /// @param threshold_dB   compression threshold in dB
    /// @param ratio          compression ratio (1.0 = passthrough, 60.0 = inf:1)
    /// @return gain reduction in dB (negative or zero)
    float computeGainReduction(float inputLevel_dB, float threshold_dB, float ratio) const;
};

} // namespace mw160
