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
    // Guard: replace non-finite input with zero to prevent a single
    // NaN or Inf from permanently poisoning the running squared state.
    if (!std::isfinite(inputSquared))
        inputSquared = 0.0;

    runningSquared_ = coeff_ * runningSquared_ + (1.0 - coeff_) * inputSquared;
    return static_cast<float>(std::sqrt(runningSquared_));
}

} // namespace mw160
