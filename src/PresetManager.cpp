#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts)
{
    scanUserPresets();
}

int PresetManager::getNumPresets() const
{
    return mw160::kNumFactoryPresets + userPresetNames_.size();
}

juce::String PresetManager::getPresetName(int index) const
{
    if (index < 0 || index >= getNumPresets())
        return {};

    if (index < mw160::kNumFactoryPresets)
        return mw160::kFactoryPresets[index].name;

    return userPresetNames_[index - mw160::kNumFactoryPresets];
}

bool PresetManager::isFactoryPreset(int index) const
{
    return index >= 0 && index < mw160::kNumFactoryPresets;
}

void PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= getNumPresets())
        return;

    currentIndex_ = index;

    if (index < mw160::kNumFactoryPresets)
        applyFactoryPreset(index);
    else
        loadUserPreset(index - mw160::kNumFactoryPresets);
}

void PresetManager::applyFactoryPreset(int factoryIndex)
{
    const auto& p = mw160::kFactoryPresets[factoryIndex];

    if (auto* param = apvts_.getParameter("threshold"))
        param->setValueNotifyingHost(param->convertTo0to1(p.threshold));
    if (auto* param = apvts_.getParameter("ratio"))
        param->setValueNotifyingHost(param->convertTo0to1(p.ratio));
    if (auto* param = apvts_.getParameter("outputGain"))
        param->setValueNotifyingHost(param->convertTo0to1(p.outputGain));
    if (auto* param = apvts_.getParameter("softKnee"))
        param->setValueNotifyingHost(p.softKnee ? 1.0f : 0.0f);
    if (auto* param = apvts_.getParameter("stereoLink"))
        param->setValueNotifyingHost(p.stereoLink ? 1.0f : 0.0f);
    if (auto* param = apvts_.getParameter("mix"))
        param->setValueNotifyingHost(param->convertTo0to1(p.mix));
}

void PresetManager::loadUserPreset(int userIndex)
{
    if (userIndex < 0 || userIndex >= userPresetFiles_.size())
        return;

    const auto& file = userPresetFiles_[userIndex];
    auto xml = juce::parseXML(file);

    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

void PresetManager::saveUserPreset(const juce::String& name)
{
    auto dir = getUserPresetDirectory();

    if (!dir.isDirectory())
        dir.createDirectory();

    auto file = dir.getChildFile(name + ".xml");
    auto state = apvts_.copyState();
    auto xml = state.createXml();

    if (xml != nullptr)
        xml->writeTo(file);

    scanUserPresets();

    // Select the newly saved preset
    for (int i = 0; i < userPresetNames_.size(); ++i)
    {
        if (userPresetNames_[i] == name)
        {
            currentIndex_ = mw160::kNumFactoryPresets + i;
            break;
        }
    }
}

void PresetManager::deleteUserPreset(int index)
{
    const int userIndex = index - mw160::kNumFactoryPresets;

    if (userIndex < 0 || userIndex >= userPresetFiles_.size())
        return;

    userPresetFiles_[userIndex].deleteFile();
    scanUserPresets();
    currentIndex_ = -1;
}

void PresetManager::scanUserPresets()
{
    userPresetNames_.clear();
    userPresetFiles_.clear();

    auto dir = getUserPresetDirectory();

    if (!dir.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, "*.xml"))
    {
        auto file = entry.getFile();
        userPresetNames_.add(file.getFileNameWithoutExtension());
        userPresetFiles_.add(file);
    }
}

juce::File PresetManager::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("MW Audio")
               .getChildFile("MW160")
               .getChildFile("Presets");
}
