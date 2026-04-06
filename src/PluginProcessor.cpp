#include "PluginProcessor.h"

MW160Processor::MW160Processor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    thresholdParam_ = apvts.getRawParameterValue("threshold");
    ratioParam_ = apvts.getRawParameterValue("ratio");
    outputGainParam_ = apvts.getRawParameterValue("outputGain");
    overEasyParam_ = apvts.getRawParameterValue("overEasy");
    stereoLinkParam_ = apvts.getRawParameterValue("stereoLink");
}

MW160Processor::~MW160Processor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout MW160Processor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"threshold", 1},
        "Threshold",
        juce::NormalisableRange<float>(-40.0f, 20.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dBu")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ratio", 1},
        "Compression",
        juce::NormalisableRange<float>(1.0f, 60.0f, 0.1f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1},
        "Output Gain",
        juce::NormalisableRange<float>(-20.0f, 20.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"overEasy", 1},
        "OverEasy",
        false));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"stereoLink", 1},
        "Stereo Link",
        true));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix", 1},
        "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return layout;
}

const juce::String MW160Processor::getName() const { return JucePlugin_Name; }
bool MW160Processor::acceptsMidi() const { return false; }
bool MW160Processor::producesMidi() const { return false; }
bool MW160Processor::isMidiEffect() const { return false; }
double MW160Processor::getTailLengthSeconds() const { return 0.0; }

int MW160Processor::getNumPrograms() { return 1; }
int MW160Processor::getCurrentProgram() { return 0; }
void MW160Processor::setCurrentProgram(int) {}
const juce::String MW160Processor::getProgramName(int) { return {}; }
void MW160Processor::changeProgramName(int, const juce::String&) {}

void MW160Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        compressor_[ch].prepare(sampleRate, samplesPerBlock);
        compressor_[ch].reset();
    }
}

void MW160Processor::releaseResources()
{
}

bool MW160Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    // Must have same input and output layout
    if (mainInput != mainOutput)
        return false;

    // Support mono and stereo
    if (mainInput != juce::AudioChannelSet::mono()
        && mainInput != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void MW160Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Read parameters once per block
    const float threshold = thresholdParam_->load();
    const float ratio = ratioParam_->load();
    const float outputGain = outputGainParam_->load();
    const bool overEasy = overEasyParam_->load() >= 0.5f;
    const bool stereoLink = stereoLinkParam_->load() >= 0.5f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        compressor_[ch].setThreshold(threshold);
        compressor_[ch].setRatio(ratio);
        compressor_[ch].setOutputGain(outputGain);
        compressor_[ch].setOverEasy(overEasy);
    }

    if (stereoLink && numChannels == 2)
    {
        float* dataL = buffer.getWritePointer(0);
        float* dataR = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            const double sqL = static_cast<double>(dataL[i]) * static_cast<double>(dataL[i]);
            const double sqR = static_cast<double>(dataR[i]) * static_cast<double>(dataR[i]);
            const double linkedSquared = (sqL + sqR) * 0.5;

            dataL[i] = compressor_[0].processSampleLinked(dataL[i], linkedSquared);
            dataR[i] = compressor_[1].processSampleLinked(dataR[i], linkedSquared);
        }
    }
    else
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);

            for (int i = 0; i < numSamples; ++i)
                channelData[i] = compressor_[ch].processSample(channelData[i]);
        }
    }
}

juce::AudioProcessorEditor* MW160Processor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

bool MW160Processor::hasEditor() const { return true; }

void MW160Processor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MW160Processor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MW160Processor();
}
