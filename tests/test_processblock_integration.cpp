// QA-DSP-007: processBlock-layer integration tests.
//
// These tests exercise MW160Processor::processBlock at varying block sizes,
// stereo-link configurations, parameter automation, and mono layout to cover
// the gap identified by the QA pass: all prior Catch2 tests exercised bare
// DSP via Compressor::processSample, but nothing constructed the full
// AudioProcessor and ran processBlock end-to-end.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{

/// Generate a sine buffer (stereo, identical L/R) for a given number of samples.
void fillSine(juce::AudioBuffer<float>& buf, float amplitude, float freqHz,
              double sampleRate, int startSampleIndex)
{
    const double twoPiOverSr = 2.0 * M_PI * freqHz / sampleRate;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        float* data = buf.getWritePointer(ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            data[i] = amplitude * static_cast<float>(std::sin(twoPiOverSr * (startSampleIndex + i)));
    }
}

/// Run processBlock with a constant-amplitude sine for a given block size.
/// Returns the gain reduction (dB) reported by the meter after settling.
float runAndMeasureGR(int blockSize, double sampleRate,
                      float amplitude, float freqHz,
                      float threshold, float ratio,
                      bool stereoLink)
{
    MW160Processor processor;
    processor.prepareToPlay(sampleRate, blockSize);

    auto* threshParam = processor.apvts.getParameter("threshold");
    auto* ratioParam  = processor.apvts.getParameter("ratio");
    auto* linkParam   = processor.apvts.getParameter("stereoLink");
    threshParam->setValueNotifyingHost(threshParam->convertTo0to1(threshold));
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(ratio));
    linkParam->setValueNotifyingHost(stereoLink ? 1.0f : 0.0f);

    juce::AudioBuffer<float> buf(2, blockSize);
    juce::MidiBuffer midi;
    int sampleIndex = 0;

    // 1 second of settling
    const int totalSamples = static_cast<int>(sampleRate);
    const int numBlocks = totalSamples / blockSize;

    for (int b = 0; b < numBlocks; ++b)
    {
        fillSine(buf, amplitude, freqHz, sampleRate, sampleIndex);
        processor.processBlock(buf, midi);
        sampleIndex += blockSize;
    }

    return processor.getMeterGainReduction();
}

/// Compute RMS of a float vector.
float computeRms(const std::vector<float>& buf)
{
    if (buf.empty()) return 0.0f;
    double sum = 0.0;
    for (float s : buf)
        sum += static_cast<double>(s) * static_cast<double>(s);
    return static_cast<float>(std::sqrt(sum / static_cast<double>(buf.size())));
}

} // namespace

// ============================================================================
// Test 1: Block-size independence through processBlock
// ============================================================================

TEST_CASE("processBlock: output is block-size independent",
          "[plugin][processblock][QA-DSP-007]")
{
    // Run the full processor at different block sizes with the same
    // parameters and input signal. After sufficient settling time the
    // gain reduction meter should report the same value regardless of
    // block size. This is the processor-layer validation of the
    // QA-DSP-001 ParameterSmoother fix.

    constexpr double kSampleRate = 48000.0;
    constexpr float kAmplitude = 0.5f;
    constexpr float kFreqHz = 1000.0f;
    constexpr float kThreshold = -20.0f;
    constexpr float kRatio = 4.0f;

    const int blockSizes[] = { 32, 64, 128, 256, 512, 1024 };
    std::vector<float> grValues;

    for (int bs : blockSizes)
    {
        float gr = runAndMeasureGR(bs, kSampleRate,
                                   kAmplitude, kFreqHz, kThreshold, kRatio, true);
        INFO("Block size " << bs << ": GR = " << gr << " dB");
        grValues.push_back(gr);
    }

    // All block sizes should produce GR within 0.5 dB of each other.
    // Before the QA-DSP-001 fix, the deviation was up to 1.7 dB.
    const float refGR = grValues.front();
    for (size_t i = 1; i < grValues.size(); ++i)
    {
        INFO("Comparing block size " << blockSizes[i] << " (GR=" << grValues[i]
             << " dB) vs " << blockSizes[0] << " (GR=" << refGR << " dB)");
        REQUIRE_THAT(static_cast<double>(grValues[i]),
                     WithinAbs(static_cast<double>(refGR), 0.5));
    }
}

// ============================================================================
// Test 2: Stereo-link through processBlock
// ============================================================================

TEST_CASE("processBlock: stereo link produces matched GR on both channels",
          "[plugin][processblock][stereo-link][QA-DSP-007]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;
    constexpr float kAmplitude = 0.5f;
    constexpr float kFreqHz = 1000.0f;

    MW160Processor processor;
    processor.prepareToPlay(kSampleRate, kBlockSize);

    auto* threshParam = processor.apvts.getParameter("threshold");
    auto* ratioParam  = processor.apvts.getParameter("ratio");
    auto* linkParam   = processor.apvts.getParameter("stereoLink");
    threshParam->setValueNotifyingHost(threshParam->convertTo0to1(-20.0f));
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(4.0f));
    linkParam->setValueNotifyingHost(1.0f); // stereo link ON

    juce::AudioBuffer<float> buf(2, kBlockSize);
    juce::MidiBuffer midi;
    int sampleIndex = 0;

    // Settle for ~0.5 seconds
    for (int b = 0; b < 48; ++b)
    {
        fillSine(buf, kAmplitude, kFreqHz, kSampleRate, sampleIndex);
        processor.processBlock(buf, midi);
        sampleIndex += kBlockSize;
    }

    // Final block: check L and R are identical
    fillSine(buf, kAmplitude, kFreqHz, kSampleRate, sampleIndex);
    processor.processBlock(buf, midi);

    const float* dataL = buf.getReadPointer(0);
    const float* dataR = buf.getReadPointer(1);

    for (int i = 0; i < kBlockSize; ++i)
    {
        REQUIRE_THAT(static_cast<double>(dataL[i]),
                     WithinAbs(static_cast<double>(dataR[i]), 1e-7));
    }

    // Verify compression actually occurred
    float inputRms = kAmplitude / std::sqrt(2.0f); // sine RMS
    float outputRms = computeRms(std::vector<float>(dataL, dataL + kBlockSize));
    REQUIRE(outputRms < inputRms);
}

TEST_CASE("processBlock: stereo link off produces independent channel compression",
          "[plugin][processblock][stereo-link][QA-DSP-007]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    MW160Processor processor;
    processor.prepareToPlay(kSampleRate, kBlockSize);

    auto* threshParam = processor.apvts.getParameter("threshold");
    auto* ratioParam  = processor.apvts.getParameter("ratio");
    auto* outputParam = processor.apvts.getParameter("outputGain");
    auto* linkParam   = processor.apvts.getParameter("stereoLink");
    threshParam->setValueNotifyingHost(threshParam->convertTo0to1(-20.0f));
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(4.0f));
    outputParam->setValueNotifyingHost(outputParam->convertTo0to1(0.0f));
    linkParam->setValueNotifyingHost(0.0f); // stereo link OFF

    juce::AudioBuffer<float> buf(2, kBlockSize);
    juce::MidiBuffer midi;
    int sampleIndex = 0;

    // Feed loud L, quiet R
    const float loudAmp = 0.5f;
    const float quietAmp = 0.01f; // below threshold

    for (int b = 0; b < 96; ++b)
    {
        const double twoPiOverSr = 2.0 * M_PI * 1000.0 / kSampleRate;
        float* dataL = buf.getWritePointer(0);
        float* dataR = buf.getWritePointer(1);
        for (int i = 0; i < kBlockSize; ++i)
        {
            float phase = static_cast<float>(std::sin(twoPiOverSr * (sampleIndex + i)));
            dataL[i] = loudAmp * phase;
            dataR[i] = quietAmp * phase;
        }
        processor.processBlock(buf, midi);
        sampleIndex += kBlockSize;
    }

    // Measure final block output
    const float* outL = buf.getReadPointer(0);
    const float* outR = buf.getReadPointer(1);

    float rmsL = computeRms(std::vector<float>(outL, outL + kBlockSize));
    float rmsR = computeRms(std::vector<float>(outR, outR + kBlockSize));

    float rmsL_dB = 20.0f * std::log10(rmsL + 1e-30f);
    float rmsR_dB = 20.0f * std::log10(rmsR + 1e-30f);

    INFO("L output RMS: " << rmsL_dB << " dB, R output RMS: " << rmsR_dB << " dB");

    // L should be compressed significantly (input ~-6 dBFS, threshold -20 dBFS, 4:1)
    REQUIRE(rmsL_dB < -10.0f);

    // R should pass through largely unchanged (input ~-40 dBFS, well below threshold)
    float expectedR_dB = 20.0f * std::log10(quietAmp / std::sqrt(2.0f));
    REQUIRE_THAT(static_cast<double>(rmsR_dB),
                 WithinAbs(static_cast<double>(expectedR_dB), 2.0));
}

// ============================================================================
// Test 3: Parameter automation through processBlock (click-free)
// ============================================================================

TEST_CASE("processBlock: threshold automation produces smooth output transitions",
          "[plugin][processblock][automation][QA-DSP-007]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;
    constexpr float kAmplitude = 0.5f;
    constexpr float kFreqHz = 1000.0f;

    MW160Processor processor;
    processor.prepareToPlay(kSampleRate, kBlockSize);

    auto* threshParam = processor.apvts.getParameter("threshold");
    auto* ratioParam  = processor.apvts.getParameter("ratio");
    threshParam->setValueNotifyingHost(threshParam->convertTo0to1(-10.0f));
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(4.0f));

    juce::AudioBuffer<float> buf(2, kBlockSize);
    juce::MidiBuffer midi;
    int sampleIndex = 0;

    // Settle with initial threshold
    for (int b = 0; b < 48; ++b)
    {
        fillSine(buf, kAmplitude, kFreqHz, kSampleRate, sampleIndex);
        processor.processBlock(buf, midi);
        sampleIndex += kBlockSize;
    }

    // Now automate threshold from -10 to -40 over several blocks and check
    // for discontinuities
    float maxStep = 0.0f;
    float prevSample = buf.getReadPointer(0)[kBlockSize - 1];

    for (int b = 0; b < 20; ++b)
    {
        // Gradually lower threshold
        float t = static_cast<float>(b) / 19.0f;
        float newThreshold = -10.0f + t * (-30.0f); // -10 to -40
        threshParam->setValueNotifyingHost(threshParam->convertTo0to1(newThreshold));

        fillSine(buf, kAmplitude, kFreqHz, kSampleRate, sampleIndex);
        processor.processBlock(buf, midi);

        const float* data = buf.getReadPointer(0);
        for (int i = 0; i < kBlockSize; ++i)
        {
            float diff = std::fabs(data[i] - prevSample);
            if (diff > maxStep)
                maxStep = diff;
            prevSample = data[i];
        }
        sampleIndex += kBlockSize;
    }

    // The maximum adjacent-sample step should be reasonable for a 1kHz sine.
    // A 0.5-amplitude, 1kHz sine at 48kHz has a max slope of about 0.065
    // per sample. Allow generous headroom for compression changes but
    // reject clicks (>0.3 would indicate a discontinuity).
    INFO("Max adjacent-sample step during threshold automation: " << maxStep);
    REQUIRE(maxStep < 0.3f);
}

// ============================================================================
// Test 4: Mono processBlock
// ============================================================================

TEST_CASE("processBlock: mono layout produces correct compression",
          "[plugin][processblock][mono][QA-DSP-007]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;
    constexpr float kAmplitude = 0.5f;
    constexpr float kFreqHz = 1000.0f;

    MW160Processor processor;

    // Configure for mono
    auto monoLayout = juce::AudioProcessor::BusesLayout();
    monoLayout.inputBuses.add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    processor.setBusesLayout(monoLayout);

    processor.prepareToPlay(kSampleRate, kBlockSize);

    auto* threshParam = processor.apvts.getParameter("threshold");
    auto* ratioParam  = processor.apvts.getParameter("ratio");
    threshParam->setValueNotifyingHost(threshParam->convertTo0to1(-20.0f));
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(4.0f));

    juce::AudioBuffer<float> buf(1, kBlockSize);
    juce::MidiBuffer midi;
    int sampleIndex = 0;

    // Settle
    for (int b = 0; b < 96; ++b)
    {
        fillSine(buf, kAmplitude, kFreqHz, kSampleRate, sampleIndex);
        processor.processBlock(buf, midi);
        sampleIndex += kBlockSize;
    }

    // Final block
    fillSine(buf, kAmplitude, kFreqHz, kSampleRate, sampleIndex);
    processor.processBlock(buf, midi);

    const float* data = buf.getReadPointer(0);
    float outputRms = computeRms(std::vector<float>(data, data + kBlockSize));
    float inputRms = kAmplitude / std::sqrt(2.0f);

    INFO("Mono output RMS: " << 20.0f * std::log10(outputRms) << " dB");
    INFO("Mono input RMS: " << 20.0f * std::log10(inputRms) << " dB");

    // Output should be compressed below input
    REQUIRE(outputRms < inputRms);

    // GR meter should show compression
    float gr = processor.getMeterGainReduction();
    INFO("Gain reduction: " << gr << " dB");
    REQUIRE(gr < -1.0f);
}

// ============================================================================
// Test 5: Metering accuracy through processBlock
// ============================================================================

TEST_CASE("processBlock: meter readings reflect actual signal levels",
          "[plugin][processblock][metering][QA-DSP-007]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    MW160Processor processor;
    processor.prepareToPlay(kSampleRate, kBlockSize);

    // Set ratio to 1:1 (no compression) to verify metering without GR.
    auto* ratioParam = processor.apvts.getParameter("ratio");
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(1.0f));

    juce::AudioBuffer<float> buf(2, kBlockSize);
    juce::MidiBuffer midi;

    // Fill with a known-amplitude signal: peak = 0.5 => -6.02 dBFS
    const float peakAmplitude = 0.5f;
    for (int ch = 0; ch < 2; ++ch)
    {
        float* data = buf.getWritePointer(ch);
        for (int i = 0; i < kBlockSize; ++i)
            data[i] = peakAmplitude;
    }
    processor.processBlock(buf, midi);

    float inputLevel = processor.getMeterInputLevel();
    float outputLevel = processor.getMeterOutputLevel();
    float gr = processor.getMeterGainReduction();

    float expected_dB = 20.0f * std::log10(peakAmplitude);

    INFO("Input level: " << inputLevel << " dB, expected: " << expected_dB << " dB");
    INFO("Output level: " << outputLevel << " dB");
    INFO("GR: " << gr << " dB");

    REQUIRE_THAT(static_cast<double>(inputLevel),
                 WithinAbs(static_cast<double>(expected_dB), 0.5));

    // At 1:1 ratio, GR should be negligible
    REQUIRE(gr > -1.0f);
}

// ============================================================================
// Test 6: Silence produces no output artifacts
// ============================================================================

TEST_CASE("processBlock: silence in produces silence out",
          "[plugin][processblock][QA-DSP-007]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    MW160Processor processor;
    processor.prepareToPlay(kSampleRate, kBlockSize);

    auto* threshParam = processor.apvts.getParameter("threshold");
    auto* ratioParam  = processor.apvts.getParameter("ratio");
    threshParam->setValueNotifyingHost(threshParam->convertTo0to1(-20.0f));
    ratioParam->setValueNotifyingHost(ratioParam->convertTo0to1(4.0f));

    juce::AudioBuffer<float> buf(2, kBlockSize);
    juce::MidiBuffer midi;

    // Push several blocks of silence
    for (int b = 0; b < 10; ++b)
    {
        buf.clear();
        processor.processBlock(buf, midi);
    }

    // Output should be silence (no noise floor added by the compressor)
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* data = buf.getReadPointer(ch);
        for (int i = 0; i < kBlockSize; ++i)
        {
            REQUIRE(std::fabs(data[i]) < 1e-10f);
        }
    }
}
