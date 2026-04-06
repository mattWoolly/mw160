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
    runningSquared_ = coeff_ * runningSquared_ + (1.0 - coeff_) * sq;
    return static_cast<float>(std::sqrt(runningSquared_));
}

} // namespace mw160
