#include "GainComputer.h"

namespace mw160 {

float GainComputer::computeGainReduction(float inputLevel_dB,
                                         float threshold_dB,
                                         float ratio,
                                         float kneeWidth_dB) const
{
    // Ratio 1.0: passthrough, no compression regardless of level
    if (ratio <= 1.0f)
        return 0.0f;

    const float slope = 1.0f / ratio - 1.0f;   // always negative for ratio > 1

    // Hard knee (kneeWidth_dB <= 0)
    if (kneeWidth_dB <= 0.0f)
    {
        if (inputLevel_dB <= threshold_dB)
            return 0.0f;

        const float excess = inputLevel_dB - threshold_dB;
        return excess * slope;
    }

    // Soft knee (OverEasy) — Giannoulis et al. quadratic interpolation
    const float halfKnee = kneeWidth_dB / 2.0f;
    const float kneeBottom = threshold_dB - halfKnee;
    const float kneeTop    = threshold_dB + halfKnee;

    if (inputLevel_dB <= kneeBottom)
        return 0.0f;   // below knee: no compression

    if (inputLevel_dB >= kneeTop)
    {
        // above knee: full ratio (same as hard knee)
        const float excess = inputLevel_dB - threshold_dB;
        return excess * slope;
    }

    // in the knee region: quadratic interpolation
    const float diff = inputLevel_dB - kneeBottom;
    return slope * diff * diff / (2.0f * kneeWidth_dB);
}

} // namespace mw160
