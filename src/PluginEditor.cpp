#include "PluginEditor.h"

MW160Editor::MW160Editor(MW160Processor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(400, 300);
}

MW160Editor::~MW160Editor() = default;

void MW160Editor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("MW160", getLocalBounds(), juce::Justification::centred, 1);
}

void MW160Editor::resized()
{
}
