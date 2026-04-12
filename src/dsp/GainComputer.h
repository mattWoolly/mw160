#pragma once

namespace mw160 {

/// Gain computer for the classic VCA compression pipeline.
/// Converts an input level (dB) to a gain reduction value (dB) given
/// threshold, ratio, and optional knee width parameters.
///
/// Ratio semantics:
///   1.0                              = no compression (passthrough)
///   1.0 to kInfinityRatioThreshold   = normal compression (slope = 1/R - 1)
///   >= kInfinityRatioThreshold       = infinity:1 (slope pinned to -1.0,
///                                      output clamped at threshold above
///                                      the knee)
///
/// Knee width semantics:
///   0.0       = hard knee (original behaviour)
///   >0.0      = soft knee (quadratic interpolation per Giannoulis et al.)
///
/// Pure C++ — no JUCE dependency — no heap allocation.
class GainComputer
{
public:
    /// Ratio at or above which the slope is pinned to exactly -1.0
    /// (true infinity:1 limiter mode). Chosen with 10:1 headroom below
    /// the top-of-parameter-range (60.0) so the transition to limiter
    /// mode is gradual from a user perspective but lands on exact
    /// -1.0 well before the top of the control.
    static constexpr float kInfinityRatioThreshold = 50.0f;

    /// Compute the gain reduction in dB for a given input level.
    /// @param inputLevel_dB  detected input level in dB
    /// @param threshold_dB   compression threshold in dB
    /// @param ratio          compression ratio (1.0 = passthrough,
    ///                       >= kInfinityRatioThreshold = inf:1 limiter)
    /// @param kneeWidth_dB   knee width in dB (0 = hard knee, >0 = soft knee)
    /// @return gain reduction in dB (negative or zero)
    float computeGainReduction(float inputLevel_dB, float threshold_dB, float ratio,
                               float kneeWidth_dB = 0.0f) const;
};

} // namespace mw160
