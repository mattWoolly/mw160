#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Compressor.h"
#include "dsp/ParameterSmoother.h"
#include "PresetManager.h"
#include <atomic>

class MW160Processor : public juce::AudioProcessor
{
public:
    MW160Processor();
    ~MW160Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /// Peak input level in dB (max across channels), updated per block.
    float getMeterInputLevel()     const { return meterInputLevel_.load(std::memory_order_relaxed); }
    /// Peak output level in dB (max across channels), updated per block.
    float getMeterOutputLevel()    const { return meterOutputLevel_.load(std::memory_order_relaxed); }
    /// Current gain reduction in dB (0 or negative), updated per block.
    float getMeterGainReduction()  const { return meterGainReduction_.load(std::memory_order_relaxed); }

private:
    static constexpr int kMaxChannels = 2;
    mw160::Compressor compressor_[kMaxChannels];

    std::atomic<float>* thresholdParam_ = nullptr;
    std::atomic<float>* ratioParam_ = nullptr;
    std::atomic<float>* outputGainParam_ = nullptr;
    std::atomic<float>* overEasyParam_ = nullptr;
    std::atomic<float>* stereoLinkParam_ = nullptr;
    std::atomic<float>* mixParam_ = nullptr;
    std::atomic<float>* bypassParam_ = nullptr;

    mw160::ParameterSmoother<mw160::SmoothingType::Linear> bypassSmoother_;

    std::atomic<float> meterInputLevel_    { -100.0f };
    std::atomic<float> meterOutputLevel_   { -100.0f };
    std::atomic<float> meterGainReduction_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MW160Processor)
};
