#include "VcaSaturation.h"

namespace mw160 {

float VcaSaturation::processSample(float input, float gainReduction_dB) const
{
    // Scale saturation amount with gain reduction depth.
    // More GR = VCA working harder = more coloration.
    const float grMagnitude = std::abs(gainReduction_dB);
    const float amount = std::clamp(grMagnitude / kGrRange_dB, 0.0f, 1.0f)
                         * kMaxSaturation;

    // Asymmetric waveshaper producing predominantly 2nd harmonic (even-order).
    // x^2 generates 2nd harmonic; x^3 adds subtle 3rd harmonic.
    // Coefficient ratio (0.05 vs 0.02) ensures even-order dominance,
    // matching the NPN/PNP asymmetry measured on the discrete VCA gain
    // element (-84 dBFS 2nd harmonic, -97 dBFS 3rd harmonic).
    const float x2 = input * input;
    return input + amount * (x2 * 0.05f - input * x2 * 0.02f);
}

} // namespace mw160
