#include "RmsDetector.h"

namespace mw160 {

void RmsDetector::prepare(double sampleRate)
{
    coeff_ = std::exp(-1.0 / (sampleRate * kTimeConstantSeconds));
}

void RmsDetector::reset()
{
    runningSquared_ = 0.0;
}

float RmsDetector::processSample(float input)
{
    const double sq = static_cast<double>(input) * static_cast<double>(input);
    return processSampleFromSquared(sq);
}

float RmsDetector::processSampleFromSquared(double inputSquared)
{
    runningSquared_ = coeff_ * runningSquared_ + (1.0 - coeff_) * inputSquared;
    return static_cast<float>(std::sqrt(runningSquared_));
}

} // namespace mw160
