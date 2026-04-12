#pragma once

#include "RmsDetector.h"
#include "GainComputer.h"
#include "Ballistics.h"
#include "VcaSaturation.h"
#include "ParameterSmoother.h"

#include <cmath>

namespace mw160 {

/// Core compressor DSP engine.
/// Wires RmsDetector → GainComputer → Ballistics into a complete
/// feedforward compression pipeline matching the classic VCA compressor
/// signal path.
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

    /// Enable or disable soft knee mode.
    void setSoftKnee(bool enabled);

    /// Set the dry/wet mix (0–100%). 100% = fully compressed, 0% = dry.
    void setMix(float mixPercent);

    /// Process a single sample through the full compression pipeline.
    float processSample(float input);

    /// Process a sample using an externally-provided squared value for the
    /// detector. Used for stereo-linked mode where the detector input is
    /// the averaged (L² + R²) / 2.
    float processSampleLinked(float input, double detectorInputSquared);

    /// Returns the most recent smoothed gain reduction in dB (0 or negative).
    float getLastGainReduction_dB() const { return lastGainReduction_dB_; }

private:
    float applyCompression(float input, float rmsLinear);

    RmsDetector detector_;
    GainComputer gainComputer_;
    Ballistics ballistics_;
    VcaSaturation vcaSaturation_;

    ParameterSmoother<SmoothingType::Linear>         thresholdSmoother_;
    ParameterSmoother<SmoothingType::Linear>         ratioSmoother_;
    ParameterSmoother<SmoothingType::Multiplicative>  outputGainSmoother_;
    ParameterSmoother<SmoothingType::Linear>         mixSmoother_;

    bool softKnee_ = false;
    float lastGainReduction_dB_ = 0.0f;

    static constexpr float kSoftKneeWidth_dB = 10.0f;
    static constexpr float kSilenceFloor_dB = -100.0f;
    static constexpr double kSmoothingTime_s = 0.020;  // 20 ms
};

} // namespace mw160
