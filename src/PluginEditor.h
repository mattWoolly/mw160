#pragma once

#include "PluginProcessor.h"

class MW160Editor : public juce::AudioProcessorEditor
{
public:
    explicit MW160Editor(MW160Processor&);
    ~MW160Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MW160Processor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MW160Editor)
};
