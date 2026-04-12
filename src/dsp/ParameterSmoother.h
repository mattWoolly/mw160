#pragma once

#include <cmath>

namespace mw160 {

/// Smoothing type for ParameterSmoother.
enum class SmoothingType { Linear, Multiplicative };

/// Lightweight per-sample parameter smoother (pure C++, no JUCE dependency).
/// Ramps from the current value to a target over a fixed number of samples.
/// Mirrors juce::SmoothedValue behaviour so the DSP lib stays testable headlessly.
template <SmoothingType Type = SmoothingType::Linear>
class ParameterSmoother
{
public:
    /// Call once from prepare(). Sets the ramp duration in seconds.
    void reset(double sampleRate, double rampTimeSeconds)
    {
        stepsToRamp_ = static_cast<int>(sampleRate * rampTimeSeconds);
        if (stepsToRamp_ < 1)
            stepsToRamp_ = 1;
        countdown_ = 0;
    }

    /// Snap immediately to a value with no ramp.
    void setCurrentAndTarget(float value)
    {
        current_ = value;
        target_ = value;
        countdown_ = 0;
    }

    /// Set a new target. The smoother will ramp from the current value.
    void setTarget(float newTarget)
    {
        // Bit-exact comparison is intentional: this is an idempotency guard
        // asking "is the requested target literally the same float I already
        // stored?" — not a magnitude comparison.  The -Wfloat-equal warning
        // is a false positive here.
        if (newTarget == target_) // NOLINT(clang-diagnostic-float-equal)
            return;

        target_ = newTarget;
        countdown_ = stepsToRamp_;

        if constexpr (Type == SmoothingType::Linear)
        {
            step_ = (target_ - current_) / static_cast<float>(countdown_);
        }
        else
        {
            if (current_ > 0.0f && target_ > 0.0f)
                factor_ = std::pow(target_ / current_,
                                   1.0f / static_cast<float>(countdown_));
            else
                step_ = (target_ - current_) / static_cast<float>(countdown_);
        }
    }

    /// Return the next smoothed value (call once per sample).
    float getNextValue()
    {
        if (countdown_ <= 0)
            return target_;

        --countdown_;

        if constexpr (Type == SmoothingType::Linear)
        {
            current_ += step_;
        }
        else
        {
            if (current_ > 0.0f && target_ > 0.0f)
                current_ *= factor_;
            else
                current_ += step_;
        }

        if (countdown_ == 0)
            current_ = target_;

        return current_;
    }

    float getCurrentValue() const { return countdown_ > 0 ? current_ : target_; }
    bool  isSmoothing()     const { return countdown_ > 0; }

private:
    float current_ = 0.0f;
    float target_  = 0.0f;
    float step_    = 0.0f;
    float factor_  = 1.0f;
    int   stepsToRamp_ = 1;
    int   countdown_   = 0;
};

} // namespace mw160
