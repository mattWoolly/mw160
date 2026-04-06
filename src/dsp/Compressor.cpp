#include "Compressor.h"

namespace mw160 {

void Compressor::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;
}

void Compressor::reset()
{
    // Will clear detector / ballistics state once implemented
}

float Compressor::processSample(float input)
{
    // Unity-gain passthrough until core DSP is implemented (MW160-7+)
    return input;
}

} // namespace mw160
