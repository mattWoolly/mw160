#include "Compressor.h"

namespace mw160 {

void Compressor::prepare(double sampleRate, int /*maxBlockSize*/)
{
    detector_.prepare(sampleRate);
    ballistics_.prepare(sampleRate);
}

void Compressor::reset()
{
    detector_.reset();
    ballistics_.reset();
}

void Compressor::setThreshold(float threshold_dB)
{
    threshold_dB_ = threshold_dB;
}

void Compressor::setRatio(float ratio)
{
    ratio_ = ratio;
}

void Compressor::setOutputGain(float gain_dB)
{
    outputGain_linear_ = std::pow(10.0f, gain_dB / 20.0f);
}

float Compressor::processSample(float input)
{
    // 1. Detect RMS level (linear)
    const float rmsLinear = detector_.processSample(input);

    // 2. Convert to dB (floor at -100 dB for silence)
    const float rms_dB = (rmsLinear > 0.0f)
                             ? 20.0f * std::log10(rmsLinear)
                             : kSilenceFloor_dB;

    // 3. Compute instantaneous gain reduction
    const float targetGR_dB = gainComputer_.computeGainReduction(rms_dB, threshold_dB_, ratio_);

    // 4. Smooth through ballistics envelope
    const float smoothedGR_dB = ballistics_.processSample(targetGR_dB);

    // 5. Convert gain reduction to linear gain
    const float gain = std::pow(10.0f, smoothedGR_dB / 20.0f);

    // 6. Apply gain reduction and output gain
    return input * gain * outputGain_linear_;
}

} // namespace mw160
