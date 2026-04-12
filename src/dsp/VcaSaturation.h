#pragma once

#include <cmath>
#include <algorithm>

namespace mw160 {

/// Models the subtle even-order harmonic distortion characteristic of a
/// discrete VCA gain element.  The amount of saturation scales with gain
/// reduction depth -- more GR means the VCA is working harder, producing
/// more coloration.
///
/// Applies a soft asymmetric waveshaper producing predominantly 2nd harmonic,
/// matching the NPN/PNP transistor asymmetry of a discrete VCA gain cell.
///
/// Pure C++ -- no JUCE dependency -- no heap allocation -- stateless.
class VcaSaturation
{
public:
    /// Apply VCA saturation to a single sample.
    /// @param input             audio sample (post gain-reduction)
    /// @param gainReduction_dB  current smoothed GR in dB (negative or zero)
    /// @return saturated sample
    float processSample(float input, float gainReduction_dB) const;

private:
    // Maximum saturation amount at full VCA depth (60+ dB GR).
    // Tuned to produce ~0.1-0.2% THD at moderate gain reduction (10-20 dB)
    // with typical signal levels.
    static constexpr float kMaxSaturation = 0.5f;

    // Gain reduction depth (dB) at which saturation reaches its maximum.
    static constexpr float kGrRange_dB = 60.0f;
};

} // namespace mw160
