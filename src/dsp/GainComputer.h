#pragma once

namespace mw160 {

/// Gain computer for the DBX 160 compression pipeline.
/// Converts an input level (dB) to a gain reduction value (dB) given
/// threshold, ratio, and optional knee width parameters.
///
/// Ratio semantics (matching the 160A control range):
///   1.0       = no compression (passthrough)
///   1.0–59.9  = normal compression
///   60.0      = infinity:1 (limiter — output pinned at threshold)
///   >60.0     = negative ratios (output decreases as input rises above threshold)
///
/// Knee width semantics:
///   0.0       = hard knee (original DBX 160 behaviour)
///   >0.0      = soft knee / OverEasy (quadratic interpolation per Giannoulis et al.)
///
/// Pure C++ — no JUCE dependency — no heap allocation.
class GainComputer
{
public:
    /// Compute the gain reduction in dB for a given input level.
    /// @param inputLevel_dB  detected input level in dB
    /// @param threshold_dB   compression threshold in dB
    /// @param ratio          compression ratio (1.0 = passthrough, 60.0 = inf:1)
    /// @param kneeWidth_dB   knee width in dB (0 = hard knee, >0 = soft knee)
    /// @return gain reduction in dB (negative or zero)
    float computeGainReduction(float inputLevel_dB, float threshold_dB, float ratio,
                               float kneeWidth_dB = 0.0f) const;
};

} // namespace mw160
