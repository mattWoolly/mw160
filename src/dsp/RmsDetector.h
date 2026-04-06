#pragma once

#include <cmath>

namespace mw160 {

/// Exponentially-weighted RMS level detector.
/// Models the DBX 160's true-RMS detection with a ~20 ms time constant.
/// Pure C++ — no JUCE dependency — no heap allocation on the audio thread.
class RmsDetector
{
public:
    /// Compute the smoothing coefficient from the sample rate.
    void prepare(double sampleRate);

    /// Clear internal state to zero.
    void reset();

    /// Process one input sample and return the current RMS level (linear).
    float processSample(float input);

    /// Process from a pre-computed squared value (for stereo-linked detection).
    float processSampleFromSquared(double inputSquared);

private:
    static constexpr double kTimeConstantSeconds = 0.020; // 20 ms

    double coeff_ = 0.0;
    double runningSquared_ = 0.0;
};

} // namespace mw160
