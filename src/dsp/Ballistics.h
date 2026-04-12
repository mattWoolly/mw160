#pragma once

#include <cmath>
#include <algorithm>

namespace mw160 {

/// Program-dependent attack/release ballistics for the classic VCA compression pipeline.
/// Smooths gain reduction values with program-dependent attack (faster for larger
/// transients) and constant-rate release (~125 dB/sec).
///
/// Pure C++ — no JUCE dependency — no heap allocation.
class Ballistics
{
public:
    /// Compute timing coefficients from the sample rate.
    void prepare(double sampleRate);

    /// Clear internal state to zero gain reduction.
    void reset();

    /// Smooth a target gain reduction value through the ballistics envelope.
    /// @param targetGainReduction_dB  instantaneous target GR in dB (negative or zero)
    /// @return smoothed gain reduction in dB
    float processSample(float targetGainReduction_dB);

private:
    double sampleRate_ = 44100.0;
    double releaseStep_ = 0.0;       // dB per sample (positive value, ~125/fs)
    double currentGR_ = 0.0;         // current smoothed gain reduction in dB

    static constexpr double kReleaseRate_dBPerSec = 125.0;
    // Base attack time constant, scaled inversely with transient magnitude
    // raised to kAttackExponent to produce program-dependent behavior.
    // Larger transients see a shorter effective time constant, yielding
    // faster attack. The exponent > 1.0 strengthens the program-dependent
    // scaling to match published reference specs:
    //   10 dB GR → ~15 ms, 20 dB GR → ~5 ms, 30 dB GR → ~3 ms
    static constexpr double kBaseAttackTime_s = 0.025;
    static constexpr double kAttackExponent = 1.55;
};

} // namespace mw160
