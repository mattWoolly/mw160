#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/Compressor.h"

using Catch::Matchers::WithinAbs;

static constexpr double kSampleRate = 44100.0;
static constexpr int kSettleSamples = static_cast<int>(kSampleRate * 0.5);
static constexpr int kMeasureSamples = static_cast<int>(kSampleRate * 0.2);

// Helper: generate a sine wave
static std::vector<float> generateSine(float amplitude, float frequency, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double twoPiOverSr = 2.0 * M_PI * frequency / kSampleRate;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = amplitude * static_cast<float>(std::sin(twoPiOverSr * i));
    return buf;
}

// Helper: compute RMS of a buffer region
static float measureRms(const std::vector<float>& buf, int startSample, int numSamples)
{
    double sum = 0.0;
    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        const double s = static_cast<double>(buf[static_cast<size_t>(i)]);
        sum += s * s;
    }
    return static_cast<float>(std::sqrt(sum / numSamples));
}

// Helper: set up a compressor with standard compression settings
static void setupCompressor(mw160::Compressor& comp, float threshold, float ratio)
{
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(threshold);
    comp.setRatio(ratio);
    comp.setOutputGain(0.0f);
    comp.setSoftKnee(false);
}

// ---------------------------------------------------------------------------

TEST_CASE("Stereo link: matched L/R signal -> identical GR on both channels",
          "[dsp][stereo-link]")
{
    mw160::Compressor compL, compR;
    setupCompressor(compL, -20.0f, 4.0f);
    setupCompressor(compR, -20.0f, 4.0f);

    const float amplitude = 0.4472f; // RMS ~ -10 dBFS, 10 dB above threshold
    const int totalSamples = kSettleSamples + kMeasureSamples;
    const auto signal = generateSine(amplitude, 1000.0f, totalSamples);

    std::vector<float> outL(signal.size()), outR(signal.size());

    for (size_t i = 0; i < signal.size(); ++i)
    {
        const double sqL = static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
        const double sqR = sqL; // matched signal
        const double linkedSquared = (sqL + sqR) * 0.5;

        outL[i] = compL.processSampleLinked(signal[i], linkedSquared);
        outR[i] = compR.processSampleLinked(signal[i], linkedSquared);
    }

    // After settling, both channels should produce identical output
    for (int i = kSettleSamples; i < totalSamples; ++i)
    {
        REQUIRE(outL[static_cast<size_t>(i)] == outR[static_cast<size_t>(i)]);
    }

    // Verify compression actually occurred
    const float inputRms = measureRms(signal, kSettleSamples, kMeasureSamples);
    const float outputRms = measureRms(outL, kSettleSamples, kMeasureSamples);
    REQUIRE(outputRms < inputRms);
}

TEST_CASE("Stereo link: signal on L only -> both channels get GR",
          "[dsp][stereo-link]")
{
    mw160::Compressor compL, compR;
    setupCompressor(compL, -20.0f, 4.0f);
    setupCompressor(compR, -20.0f, 4.0f);

    const float amplitude = 0.4472f;
    const int totalSamples = kSettleSamples + kMeasureSamples;
    const auto signalL = generateSine(amplitude, 1000.0f, totalSamples);
    std::vector<float> signalR(signalL.size(), 0.0f); // silence on R

    std::vector<float> outL(signalL.size()), outR(signalR.size());

    for (size_t i = 0; i < signalL.size(); ++i)
    {
        const double sqL = static_cast<double>(signalL[i]) * static_cast<double>(signalL[i]);
        const double sqR = 0.0; // R is silent
        const double linkedSquared = (sqL + sqR) * 0.5;

        outL[i] = compL.processSampleLinked(signalL[i], linkedSquared);
        outR[i] = compR.processSampleLinked(signalR[i], linkedSquared);
    }

    // L should be compressed (GR applied based on combined energy)
    const float inputRmsL = measureRms(signalL, kSettleSamples, kMeasureSamples);
    const float outputRmsL = measureRms(outL, kSettleSamples, kMeasureSamples);
    REQUIRE(outputRmsL < inputRmsL);

    // R is silence in, so output should be silence (0 * gain = 0)
    for (int i = kSettleSamples; i < totalSamples; ++i)
        REQUIRE(outR[static_cast<size_t>(i)] == 0.0f);

    // The GR on L should be less aggressive than without linking,
    // since the averaged energy is halved (only L contributing)
    mw160::Compressor compUnlinked;
    setupCompressor(compUnlinked, -20.0f, 4.0f);
    std::vector<float> outUnlinked(signalL.size());
    for (size_t i = 0; i < signalL.size(); ++i)
        outUnlinked[i] = compUnlinked.processSample(signalL[i]);

    const float outputRmsUnlinked = measureRms(outUnlinked, kSettleSamples, kMeasureSamples);
    // Linked output should be louder (less compression) since energy is halved
    REQUIRE(outputRmsL > outputRmsUnlinked);
}

TEST_CASE("Stereo link off: signal on L only -> only L gets GR, R passes through",
          "[dsp][stereo-link]")
{
    mw160::Compressor compL, compR;
    setupCompressor(compL, -20.0f, 4.0f);
    setupCompressor(compR, -20.0f, 4.0f);

    const float amplitude = 0.4472f;
    const int totalSamples = kSettleSamples + kMeasureSamples;
    const auto signalL = generateSine(amplitude, 1000.0f, totalSamples);
    // R has a quiet signal (below threshold)
    const auto signalR = generateSine(0.01f, 1000.0f, totalSamples);

    std::vector<float> outL(signalL.size()), outR(signalR.size());

    // Process independently (no linking) — each channel uses its own detector
    for (size_t i = 0; i < signalL.size(); ++i)
    {
        outL[i] = compL.processSample(signalL[i]);
        outR[i] = compR.processSample(signalR[i]);
    }

    // L should be compressed
    const float inputRmsL = measureRms(signalL, kSettleSamples, kMeasureSamples);
    const float outputRmsL = measureRms(outL, kSettleSamples, kMeasureSamples);
    REQUIRE(outputRmsL < inputRmsL);

    // R should pass through largely unchanged (signal is below threshold at -20 dB)
    const float inputRmsR = measureRms(signalR, kSettleSamples, kMeasureSamples);
    const float outputRmsR = measureRms(outR, kSettleSamples, kMeasureSamples);
    const float inputR_dB = 20.0f * std::log10(inputRmsR);
    const float outputR_dB = 20.0f * std::log10(outputRmsR);
    REQUIRE_THAT(static_cast<double>(outputR_dB),
                 WithinAbs(static_cast<double>(inputR_dB), 1.0));
}

TEST_CASE("Mono input: stereo link has no effect",
          "[dsp][stereo-link]")
{
    // When there's only one channel, processSample is used directly
    // regardless of stereo link setting. Verify the compressor works
    // identically in both cases.
    const float amplitude = 0.4472f;
    const int totalSamples = kSettleSamples + kMeasureSamples;
    const auto signal = generateSine(amplitude, 1000.0f, totalSamples);

    // Process with normal processSample (mono path)
    mw160::Compressor comp;
    setupCompressor(comp, -20.0f, 4.0f);

    std::vector<float> output(signal.size());
    for (size_t i = 0; i < signal.size(); ++i)
        output[i] = comp.processSample(signal[i]);

    // Process the same signal through processSampleLinked with self-linked input
    // (simulating what would happen if linked mode ran on mono — it shouldn't,
    // but verify the DSP is equivalent)
    mw160::Compressor compLinked;
    setupCompressor(compLinked, -20.0f, 4.0f);

    std::vector<float> outputLinked(signal.size());
    for (size_t i = 0; i < signal.size(); ++i)
    {
        const double sq = static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
        outputLinked[i] = compLinked.processSampleLinked(signal[i], sq);
    }

    // Both outputs should be identical — processSampleLinked with the same
    // channel's squared value is equivalent to processSample.
    for (int i = 0; i < totalSamples; ++i)
    {
        REQUIRE_THAT(static_cast<double>(outputLinked[static_cast<size_t>(i)]),
                     WithinAbs(static_cast<double>(output[static_cast<size_t>(i)]), 1e-6));
    }
}
