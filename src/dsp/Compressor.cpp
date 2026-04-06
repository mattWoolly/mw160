#include "Compressor.h"

namespace mw160 {

void Compressor::prepare(double sampleRate, int /*maxBlockSize*/)
{
    detector_.prepare(sampleRate);
    ballistics_.prepare(sampleRate);

    thresholdSmoother_.reset(sampleRate, kSmoothingTime_s);
    ratioSmoother_.reset(sampleRate, kSmoothingTime_s);
    outputGainSmoother_.reset(sampleRate, kSmoothingTime_s);

    thresholdSmoother_.setCurrentAndTarget(0.0f);
    ratioSmoother_.setCurrentAndTarget(1.0f);
    outputGainSmoother_.setCurrentAndTarget(1.0f);  // 0 dB = linear 1.0
}

void Compressor::reset()
{
    detector_.reset();
    ballistics_.reset();
}

void Compressor::setThreshold(float threshold_dB)
{
    thresholdSmoother_.setTarget(threshold_dB);
}

void Compressor::setRatio(float ratio)
{
    ratioSmoother_.setTarget(ratio);
}

void Compressor::setOutputGain(float gain_dB)
{
    outputGainSmoother_.setTarget(std::pow(10.0f, gain_dB / 20.0f));
}

void Compressor::setOverEasy(bool enabled)
{
    overEasy_ = enabled;
}

float Compressor::processSample(float input)
{
    const float rmsLinear = detector_.processSample(input);
    return applyCompression(input, rmsLinear);
}

float Compressor::processSampleLinked(float input, double detectorInputSquared)
{
    const float rmsLinear = detector_.processSampleFromSquared(detectorInputSquared);
    return applyCompression(input, rmsLinear);
}

float Compressor::applyCompression(float input, float rmsLinear)
{
    // Read smoothed parameter values (one step per sample)
    const float threshold_dB = thresholdSmoother_.getNextValue();
    const float ratio        = ratioSmoother_.getNextValue();
    const float outputGain   = outputGainSmoother_.getNextValue();

    // Convert to dB (floor at -100 dB for silence)
    const float rms_dB = (rmsLinear > 0.0f)
                             ? 20.0f * std::log10(rmsLinear)
                             : kSilenceFloor_dB;

    // Compute instantaneous gain reduction
    const float kneeWidth = overEasy_ ? kOverEasyKneeWidth_dB : 0.0f;
    const float targetGR_dB = gainComputer_.computeGainReduction(rms_dB, threshold_dB, ratio, kneeWidth);

    // Smooth through ballistics envelope
    const float smoothedGR_dB = ballistics_.processSample(targetGR_dB);

    // Convert gain reduction to linear gain
    const float gain = std::pow(10.0f, smoothedGR_dB / 20.0f);

    // Apply gain reduction and output gain
    return input * gain * outputGain;
}

} // namespace mw160
