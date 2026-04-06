#pragma once

#include "RmsDetector.h"
#include "GainComputer.h"
#include "Ballistics.h"
#include "ParameterSmoother.h"

#include <cmath>

namespace mw160 {

/// Core compressor DSP engine.
/// Wires RmsDetector → GainComputer → Ballistics into a complete
/// feedforward compression pipeline matching the DBX 160 signal path.
/// Pure C++ — no JUCE dependency — so it can be unit-tested headlessly.
class Compressor
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    /// Set the compression threshold in dB.
    void setThreshold(float threshold_dB);

    /// Set the compression ratio (1.0 = passthrough, 60.0 = infinity:1).
    void setRatio(float ratio);

    /// Set the output (makeup) gain in dB.
    void setOutputGain(float gain_dB);

    /// Process a single sample through the full compression pipeline.
    float processSample(float input);

private:
    RmsDetector detector_;
    GainComputer gainComputer_;
    Ballistics ballistics_;

    ParameterSmoother<SmoothingType::Linear>         thresholdSmoother_;
    ParameterSmoother<SmoothingType::Linear>         ratioSmoother_;
    ParameterSmoother<SmoothingType::Multiplicative>  outputGainSmoother_;

    static constexpr float kSilenceFloor_dB = -100.0f;
    static constexpr double kSmoothingTime_s = 0.020;  // 20 ms
};

} // namespace mw160
