#include "GainComputer.h"

namespace mw160 {

float GainComputer::computeGainReduction(float inputLevel_dB,
                                         float threshold_dB,
                                         float ratio) const
{
    // Below or at threshold: no compression
    if (inputLevel_dB <= threshold_dB)
        return 0.0f;

    // Ratio 1.0: passthrough, no compression regardless of level
    if (ratio <= 1.0f)
        return 0.0f;

    // Hard knee: GR = (threshold - input) * (1 - 1/ratio)
    // This yields a negative value (attenuation).
    // At ratio = 60 (infinity:1), 1/ratio ≈ 0.0167 so output is
    // nearly pinned at threshold.
    const float excess = inputLevel_dB - threshold_dB;
    return excess * (1.0f / ratio - 1.0f);
}

} // namespace mw160
