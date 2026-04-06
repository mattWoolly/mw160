#include "PluginProcessor.h"
#include "PluginEditor.h"

MW160Processor::MW160Processor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MW160Processor::~MW160Processor() = default;

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

void MW160Processor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
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

void MW160Processor::processBlock(juce::AudioBuffer<float>& /*buffer*/,
                                  juce::MidiBuffer& /*midiMessages*/)
{
    // Unity gain passthrough — audio buffer is left unmodified
}

juce::AudioProcessorEditor* MW160Processor::createEditor()
{
    return new MW160Editor(*this);
}

bool MW160Processor::hasEditor() const { return true; }

void MW160Processor::getStateInformation(juce::MemoryBlock& /*destData*/)
{
}

void MW160Processor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MW160Processor();
}
