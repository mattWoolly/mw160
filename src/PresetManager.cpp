#include "PresetManager.h"

namespace
{
juce::File defaultPresetDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("MW Audio")
               .getChildFile("MW160")
               .getChildFile("Presets");
}
} // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts),
      presetDir_(defaultPresetDirectory())
{
    scanUserPresets();
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts,
                             const juce::File& presetDir)
    : apvts_(apvts),
      presetDir_(presetDir)
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

bool PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= getNumPresets())
        return false;

    if (index < mw160::kNumFactoryPresets)
    {
        applyFactoryPreset(index);
        currentIndex_ = index;
        return true;
    }

    if (!loadUserPreset(index - mw160::kNumFactoryPresets))
        return false;

    currentIndex_ = index;
    return true;
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

bool PresetManager::loadUserPreset(int userIndex)
{
    if (userIndex < 0 || userIndex >= userPresetFiles_.size())
        return false;

    const auto& file = userPresetFiles_[userIndex];
    auto xml = juce::parseXML(file);

    if (xml == nullptr || !xml->hasTagName(apvts_.state.getType()))
        return false;

    apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    return true;
}

// static
bool PresetManager::isValidPresetName(const juce::String& name)
{
    if (name.isEmpty())
        return false;

    // Reject path traversal and current-directory references
    if (name == ".." || name == ".")
        return false;

    // Reject path separators
    if (name.containsChar('/') || name.containsChar('\\'))
        return false;

    // Reject characters invalid in filenames across platforms
    const char illegal[] = { '<', '>', ':', '"', '|', '?', '*' };
    for (auto c : illegal)
        if (name.containsChar(c))
            return false;

    // Reject reserved Windows device names (case-insensitive)
    auto upper = name.toUpperCase();
    static const juce::StringArray reserved {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    if (reserved.contains(upper))
        return false;

    return true;
}

PresetManager::SaveResult
PresetManager::saveUserPreset(const juce::String& name, bool allowOverwrite)
{
    if (!isValidPresetName(name))
        return SaveResult::InvalidName;

    auto dir = getUserPresetDirectory();

    if (!dir.isDirectory())
        dir.createDirectory();

    auto file = dir.getChildFile(name + ".xml");

    if (file.existsAsFile() && !allowOverwrite)
        return SaveResult::AlreadyExists;

    auto state = apvts_.copyState();
    state.setProperty("pluginVersion", 1, nullptr);
    auto xml = state.createXml();

    if (xml == nullptr || !xml->writeTo(file))
        return SaveResult::WriteFailed;

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

    return SaveResult::Ok;
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
    return presetDir_;
}
