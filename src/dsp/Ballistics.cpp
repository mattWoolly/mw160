#include "Ballistics.h"

namespace mw160 {

void Ballistics::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    releaseStep_ = kReleaseRate_dBPerSec / sampleRate;
}

void Ballistics::reset()
{
    currentGR_ = 0.0;
}

float Ballistics::processSample(float targetGainReduction_dB)
{
    const double target = static_cast<double>(targetGainReduction_dB);
    const double diff = target - currentGR_;

    if (diff < 0.0)
    {
        // ATTACK: target is more negative (more compression needed).
        // Program-dependent: time constant scales inversely with the current
        // difference between target and actual GR. Larger transients see a
        // shorter effective time constant (faster attack). As the filter
        // converges and the difference shrinks, the attack naturally slows —
        // producing the nonlinear attack trajectory characteristic of the
        // DBX 160: fast onset that decelerates near final GR.
        const double magnitude = -diff;
        const double attackTime = kBaseAttackTime_s / std::max(1.0, magnitude);
        const double coeff = std::exp(-1.0 / (attackTime * sampleRate_));
        currentGR_ = coeff * currentGR_ + (1.0 - coeff) * target;
    }
    else
    {
        // RELEASE: constant rate at ~125 dB/sec.
        // currentGR_ is negative, releasing means moving toward 0.
        currentGR_ = std::min(currentGR_ + releaseStep_, target);
    }

    return static_cast<float>(currentGR_);
}

} // namespace mw160
