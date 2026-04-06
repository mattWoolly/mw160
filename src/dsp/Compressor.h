#pragma once

namespace mw160 {

/// Core compressor DSP engine.
/// Pure C++ — no JUCE dependency — so it can be unit-tested headlessly.
/// Skeleton only; real DSP arrives in MW160-7 through MW160-9.
class Compressor
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    /// Process a single sample and return the output.
    /// Currently unity-gain passthrough.
    float processSample(float input);

private:
    double sampleRate_ = 44100.0;
};

} // namespace mw160
