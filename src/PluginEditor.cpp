#include "PluginEditor.h"
#include "gui/Palette.h"
#include "gui/Fonts.h"
#include "gui/Screws.h"
#include "gui/Geometry.h"

namespace
{
    // Reference canvas size -- all VISUAL_SPEC.md bounds are given at 900x360.
    constexpr int kRefW = 900;
    constexpr int kRefH = 360;

    /// Map a reference-canvas rectangle to the current editor size while
    /// preserving the 2.5:1 aspect ratio. Because the aspect ratio is
    /// locked (VISUAL_SPEC.md §1.2), width and height always scale by the
    /// same factor, so this is effectively a uniform scale.
    juce::Rectangle<float> scaleR(const juce::Rectangle<float>& refRect,
                                  int currentW, int currentH)
    {
        const float sx = static_cast<float>(currentW) / kRefW;
        const float sy = static_cast<float>(currentH) / kRefH;
        return { refRect.getX() * sx, refRect.getY() * sy,
                 refRect.getWidth() * sx, refRect.getHeight() * sy };
    }

    juce::Rectangle<int> scaleI(int refX, int refY, int refW, int refH,
                                int currentW, int currentH)
    {
        const float sx = static_cast<float>(currentW) / kRefW;
        const float sy = static_cast<float>(currentH) / kRefH;
        return juce::Rectangle<int>(juce::roundToInt(refX * sx),
                                    juce::roundToInt(refY * sy),
                                    juce::roundToInt(refW * sx),
                                    juce::roundToInt(refH * sy));
    }

    void setupRotary(juce::Slider& s, juce::AudioProcessorEditor& editor,
                     const juce::String& paramName,
                     const juce::String& helpText)
    {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        // VISUAL_SPEC.md §5.2 prescribes a 270 degree travel. Using the
        // canonical 1.25*pi / 2.75*pi range gives the 7:30 -> 4:30 sweep.
        s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                              juce::MathConstants<float>::pi * 2.75f,
                              true);
        s.setName(paramName);
        s.setTitle(paramName);
        s.setHelpText(helpText);
        editor.addAndMakeVisible(s);
    }

    void setupKnobLabel(juce::Label& label, const juce::String& text,
                        juce::Component& parent)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(mw160::Fonts::withTracking(mw160::Fonts::interBold(11.0f), 1.0f));
        label.setColour(juce::Label::textColourId, mw160::Palette::textBright);
        parent.addAndMakeVisible(label);
    }

    void setupValueLabel(juce::Label& label, juce::Component& parent)
    {
        label.setJustificationType(juce::Justification::centred);
        label.setFont(mw160::Fonts::jetBrainsMono(11.0f));
        label.setColour(juce::Label::textColourId, mw160::Palette::textBright);
        parent.addAndMakeVisible(label);
    }

    void setupUnitLabel(juce::Label& label, const juce::String& text,
                        juce::Component& parent)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(mw160::Fonts::interRegular(9.0f));
        label.setColour(juce::Label::textColourId, mw160::Palette::textMid);
        parent.addAndMakeVisible(label);
    }

    void setupMeterLabel(juce::Label& label, const juce::String& text,
                         juce::Component& parent, bool bright)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(mw160::Fonts::withTracking(mw160::Fonts::interBold(9.0f), 1.0f));
        label.setColour(juce::Label::textColourId,
                        bright ? mw160::Palette::textBright : mw160::Palette::textDim);
        parent.addAndMakeVisible(label);
    }

    /// Draw a circular LED indicator with optional glow.
    void drawLed(juce::Graphics& g, juce::Rectangle<float> area,
                 juce::Colour colour, juce::Colour darkColour, bool lit)
    {
        if (lit)
        {
            g.setColour(colour.withAlpha(0.30f));
            g.fillEllipse(area.expanded(3.0f));
            g.setColour(colour);
            g.fillEllipse(area);
        }
        else
        {
            g.setColour(darkColour);
            g.fillEllipse(area);
        }
        g.setColour(mw160::Palette::borderHairline);
        g.drawEllipse(area, 0.8f);
    }
}

// =============================================================================

MW160Editor::MW160Editor(MW160Processor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    using namespace mw160::Palette;

    setLookAndFeel(&customLnf);

    // Knobs -- names drive the "fill from end" heuristic in drawRotarySlider.
    setupRotary(thresholdSlider,  *this, "THRESHOLD", "Threshold level (dB)");
    setupRotary(ratioSlider,      *this, "RATIO",     "Compression ratio");
    setupRotary(outputGainSlider, *this, "OUTPUT",    "Output makeup gain (dB)");
    setupRotary(mixSlider,        *this, "MIX",       "Dry/wet mix (%)");

    setupKnobLabel(thresholdLabel,  "THRESHOLD", *this);
    setupKnobLabel(ratioLabel,      "RATIO",     *this);
    setupKnobLabel(outputGainLabel, "OUTPUT",    *this);
    setupKnobLabel(mixLabel,        "MIX",       *this);

    setupValueLabel(thresholdValue,  *this);
    setupValueLabel(ratioValue,      *this);
    setupValueLabel(outputGainValue, *this);
    setupValueLabel(mixValue,        *this);

    setupUnitLabel(thresholdUnit,  "dB",      *this);
    setupUnitLabel(ratioUnit,      "ratio",   *this);
    setupUnitLabel(outputGainUnit, "dB",      *this);
    // MIX is the modern-addition control and gets a "(mix)" caption to
    // distinguish it from the historical control set (VISUAL_SPEC.md §2.3).
    setupUnitLabel(mixUnit,        "(mix)",   *this);

    // --- Preset controls ---
    presetLabel.setText("PRESET", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredLeft);
    presetLabel.setFont(mw160::Fonts::withTracking(mw160::Fonts::interBold(9.0f), 1.0f));
    presetLabel.setColour(juce::Label::textColourId, textMid);
    addAndMakeVisible(presetLabel);

    presetBox.setTextWhenNothingSelected("Select preset...");
    presetBox.setTooltip("Select a factory or user preset");
    refreshPresetList();
    presetBox.addListener(this);
    addAndMakeVisible(presetBox);

    saveButton.setTooltip("Save current settings as a user preset");
    saveButton.onClick = [this] { onSavePreset(); };
    addAndMakeVisible(saveButton);

    deleteButton.setTooltip("Delete the selected user preset");
    deleteButton.onClick = [this] { onDeletePreset(); };
    addAndMakeVisible(deleteButton);

    versionLabel.setText(juce::String("mw160  v") + JucePlugin_VersionString,
                         juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setFont(mw160::Fonts::interRegular(9.0f));
    versionLabel.setColour(juce::Label::textColourId, textMid);
    addAndMakeVisible(versionLabel);

    // --- Switch cluster ---
    //
    // The KNEE, STEREO LINK, and BYPASS switches use the pill look drawn
    // by CustomLookAndFeel::drawToggleButton. Per-switch label overrides
    // live in the button's properties map so a single ToggleButton can
    // carry the HARD/SOFT wording without subclassing.
    kneeButton.setClickingTogglesState(true);
    kneeButton.getProperties().set("offLabel", "HARD");
    kneeButton.getProperties().set("onLabel",  "SOFT");
    kneeButton.setTitle("Knee");
    kneeButton.setHelpText("Knee shape: HARD or SOFT");
    addAndMakeVisible(kneeButton);

    stereoLinkButton.setClickingTogglesState(true);
    stereoLinkButton.getProperties().set("offLabel", "OFF");
    stereoLinkButton.getProperties().set("onLabel",  "ON");
    stereoLinkButton.setTitle("Stereo Link");
    stereoLinkButton.setHelpText("Link detector across channels");
    addAndMakeVisible(stereoLinkButton);

    bypassButton.setClickingTogglesState(true);
    bypassButton.getProperties().set("offLabel",    "OFF");
    bypassButton.getProperties().set("onLabel",     "ON");
    bypassButton.getProperties().set("bypassStyle", true);
    bypassButton.setTitle("Bypass");
    bypassButton.setHelpText("Bypass the compressor");
    addAndMakeVisible(bypassButton);

    meterModeButton.setTooltip("Meter display mode: IN / OUT / GR");
    addAndMakeVisible(meterModeButton);
    meterModeButton.onModeChange = [this](MeterModeButton::Mode)
    {
        // Repaint the meter label row and the tick columns (§6.6).
        inputMeterLabel.repaint();
        outputMeterLabel.repaint();
        grMeterLabel.repaint();
        repaint();
        // Update brightness.
        auto apply = [](juce::Label& L, bool bright)
        {
            L.setColour(juce::Label::textColourId,
                        bright ? mw160::Palette::textBright : mw160::Palette::textDim);
        };
        apply(inputMeterLabel,  meterModeButton.getMode() == MeterModeButton::Mode::In);
        apply(outputMeterLabel, meterModeButton.getMode() == MeterModeButton::Mode::Out);
        apply(grMeterLabel,     meterModeButton.getMode() == MeterModeButton::Mode::Gr);
    };

    // Meters + labels.
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(grMeter);
    setupMeterLabel(inputMeterLabel,  "IN",  *this, true);
    setupMeterLabel(outputMeterLabel, "OUT", *this, false);
    setupMeterLabel(grMeterLabel,     "GR",  *this, false);

    // APVTS attachments.
    thresholdAttachment  = std::make_unique<SliderAttachment>(processorRef.apvts, "threshold",  thresholdSlider);
    ratioAttachment      = std::make_unique<SliderAttachment>(processorRef.apvts, "ratio",      ratioSlider);
    outputGainAttachment = std::make_unique<SliderAttachment>(processorRef.apvts, "outputGain", outputGainSlider);
    mixAttachment        = std::make_unique<SliderAttachment>(processorRef.apvts, "mix",        mixSlider);
    kneeAttachment       = std::make_unique<ButtonAttachment>(processorRef.apvts, "softKnee",   kneeButton);
    stereoLinkAttachment = std::make_unique<ButtonAttachment>(processorRef.apvts, "stereoLink", stereoLinkButton);
    bypassAttachment     = std::make_unique<ButtonAttachment>(processorRef.apvts, "bypass",     bypassButton);

    // Tab focus order per VISUAL_SPEC.md §11.4:
    // presets -> THR -> RATIO -> OUT -> MIX -> KNEE -> STEREO -> BYPASS -> meter mode.
    presetBox.setExplicitFocusOrder(1);
    thresholdSlider.setExplicitFocusOrder(2);
    ratioSlider.setExplicitFocusOrder(3);
    outputGainSlider.setExplicitFocusOrder(4);
    mixSlider.setExplicitFocusOrder(5);
    kneeButton.setExplicitFocusOrder(6);
    stereoLinkButton.setExplicitFocusOrder(7);
    bypassButton.setExplicitFocusOrder(8);
    meterModeButton.setExplicitFocusOrder(9);

    // Aspect-locked resize (VISUAL_SPEC.md §1.2).
    aspectConstrainer_.setFixedAspectRatio(static_cast<double>(kRefW) / kRefH);
    aspectConstrainer_.setSizeLimits(720, 288, 1440, 576);
    setConstrainer(&aspectConstrainer_);
    setResizable(true, true);

    setSize(kRefW, kRefH);

    // Keep value readouts in sync with slider state.
    thresholdSlider.onValueChange = [this]
    {
        thresholdValue.setText(juce::String::formatted("%+.1f dB", thresholdSlider.getValue()),
                               juce::dontSendNotification);
    };
    ratioSlider.onValueChange = [this]
    {
        ratioValue.setText(juce::String::formatted("%.1f:1", ratioSlider.getValue()),
                           juce::dontSendNotification);
    };
    outputGainSlider.onValueChange = [this]
    {
        outputGainValue.setText(juce::String::formatted("%+.1f dB", outputGainSlider.getValue()),
                                juce::dontSendNotification);
    };
    mixSlider.onValueChange = [this]
    {
        mixValue.setText(juce::String::formatted("%.0f%%", mixSlider.getValue()),
                         juce::dontSendNotification);
    };
    updateValueReadouts();

    startTimerHz(30);
}

MW160Editor::~MW160Editor()
{
    stopTimer();
    setConstrainer(nullptr);
    setLookAndFeel(nullptr);
}

void MW160Editor::updateValueReadouts()
{
    thresholdValue .setText(juce::String::formatted("%+.1f dB",  thresholdSlider .getValue()), juce::dontSendNotification);
    ratioValue     .setText(juce::String::formatted("%.1f:1",    ratioSlider     .getValue()), juce::dontSendNotification);
    outputGainValue.setText(juce::String::formatted("%+.1f dB",  outputGainSlider.getValue()), juce::dontSendNotification);
    mixValue       .setText(juce::String::formatted("%.0f%%",    mixSlider       .getValue()), juce::dontSendNotification);
}

// =============================================================================

void MW160Editor::timerCallback()
{
    constexpr float kDecayInOut = 0.85f;
    constexpr float kDecayGR    = 0.80f;

    // --- Input level (snap up, decay down) ---
    const float newIn = processorRef.getMeterInputLevel();
    displayedInputLevel_ = (newIn > displayedInputLevel_)
        ? newIn
        : std::max(newIn, displayedInputLevel_ * kDecayInOut + (-100.0f) * (1.0f - kDecayInOut));
    inputMeter.setLevel(displayedInputLevel_);

    // --- Output level ---
    const float newOut = processorRef.getMeterOutputLevel();
    displayedOutputLevel_ = (newOut > displayedOutputLevel_)
        ? newOut
        : std::max(newOut, displayedOutputLevel_ * kDecayInOut + (-100.0f) * (1.0f - kDecayInOut));
    outputMeter.setLevel(displayedOutputLevel_);

    // --- Gain reduction (snap deeper, decay toward 0) ---
    const float newGR = processorRef.getMeterGainReduction();
    displayedGR_ = (newGR < displayedGR_)
        ? newGR
        : displayedGR_ * kDecayGR;
    grMeter.setLevel(displayedGR_);

    // --- Threshold indicator LEDs ---
    const float threshold = processorRef.apvts.getRawParameterValue("threshold")->load();
    const bool  softKnee  = processorRef.apvts.getRawParameterValue("softKnee")->load() >= 0.5f;
    const float halfKnee  = softKnee ? 5.0f : 0.0f;  // 10 dB soft knee / 2

    const bool wasAbove = ledAbove_;
    const bool wasBelow = ledBelow_;

    ledBelow_ = (displayedInputLevel_ < threshold + halfKnee);
    ledAbove_ = (displayedInputLevel_ > threshold - halfKnee);

    // Only repaint the header band if LED state changed (meters repaint
    // themselves on setLevel).
    if (wasAbove != ledAbove_ || wasBelow != ledBelow_)
    {
        const int headerH = juce::roundToInt(60.0f * static_cast<float>(getHeight()) / kRefH);
        repaint(0, 0, getWidth(), headerH);
    }
}

// =============================================================================

void MW160Editor::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox != &presetBox)
        return;

    const int selected = presetBox.getSelectedId();
    if (selected > 0)
    {
        if (processorRef.presetManager.loadPreset(selected - 1))
        {
            updateValueReadouts();
        }
        else
        {
            // Revert combo to the previous valid selection.
            const int prevIdx = processorRef.presetManager.getCurrentIndex();
            presetBox.setSelectedId(prevIdx >= 0 ? prevIdx + 1 : 0,
                                    juce::dontSendNotification);

            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Preset Load Error",
                "The selected preset could not be loaded. "
                "The file may be corrupt or incompatible.",
                "OK");
        }
    }
}

void MW160Editor::refreshPresetList()
{
    presetBox.clear(juce::dontSendNotification);
    auto& pm = processorRef.presetManager;
    pm.scanUserPresets();

    int id = 1;

    // Factory presets
    for (int i = 0; i < pm.getNumFactoryPresets(); ++i)
        presetBox.addItem(pm.getPresetName(i), id++);

    // Separator + user presets
    if (pm.getNumPresets() > pm.getNumFactoryPresets())
    {
        presetBox.addSeparator();
        for (int i = pm.getNumFactoryPresets(); i < pm.getNumPresets(); ++i)
            presetBox.addItem(pm.getPresetName(i), id++);
    }

    // Restore current selection
    const int idx = pm.getCurrentIndex();
    if (idx >= 0 && idx < pm.getNumPresets())
        presetBox.setSelectedId(idx + 1, juce::dontSendNotification);
}

void MW160Editor::onSavePreset()
{
    auto* aw = new juce::AlertWindow("Save Preset",
                                      "Enter a name for the preset:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", "", "Preset name:");
    aw->addButton("Save", 1);
    aw->addButton("Cancel", 0);

    juce::Component::SafePointer<MW160Editor> safeThis(this);
    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, aw](int result)
        {
            if (safeThis != nullptr && result == 1)
            {
                auto name = aw->getTextEditorContents("name").trim();
                if (name.isNotEmpty())
                    safeThis->trySavePreset(name, false);
            }
            delete aw;
        }));
}

void MW160Editor::trySavePreset(const juce::String& name, bool allowOverwrite)
{
    auto result = processorRef.presetManager.saveUserPreset(name, allowOverwrite);

    switch (result)
    {
        case PresetManager::SaveResult::Ok:
            refreshPresetList();
            break;

        case PresetManager::SaveResult::InvalidName:
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Invalid Name",
                "The preset name contains invalid characters. "
                "Avoid path separators, reserved system names, "
                "and characters like < > : \" | ? *",
                "OK");
            break;

        case PresetManager::SaveResult::AlreadyExists:
        {
            auto* confirm = new juce::AlertWindow(
                "Overwrite Preset",
                "A preset named \"" + name + "\" already exists. Overwrite it?",
                juce::MessageBoxIconType::WarningIcon);
            confirm->addButton("Overwrite", 1);
            confirm->addButton("Cancel", 0);

            juce::Component::SafePointer<MW160Editor> safeThis(this);
            confirm->enterModalState(true, juce::ModalCallbackFunction::create(
                [safeThis, confirm, name](int r)
                {
                    if (safeThis != nullptr && r == 1)
                        safeThis->trySavePreset(name, true);
                    delete confirm;
                }));
            break;
        }

        case PresetManager::SaveResult::WriteFailed:
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Save Failed",
                "The preset could not be written to disk. "
                "Check that the preset folder is writable.",
                "OK");
            break;
    }
}

void MW160Editor::onDeletePreset()
{
    auto& pm = processorRef.presetManager;
    const int idx = pm.getCurrentIndex();

    if (idx < 0)
        return;

    if (pm.isFactoryPreset(idx))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Factory Preset",
            "Factory presets cannot be deleted.",
            "OK");
        return;
    }

    const auto name = pm.getPresetName(idx);

    auto* aw = new juce::AlertWindow("Delete Preset",
                                      "Delete \"" + name + "\"?",
                                      juce::MessageBoxIconType::WarningIcon);
    aw->addButton("Delete", 1);
    aw->addButton("Cancel", 0);

    juce::Component::SafePointer<MW160Editor> safeThis(this);
    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, aw, idx](int result)
        {
            if (safeThis != nullptr && result == 1)
            {
                safeThis->processorRef.presetManager.deleteUserPreset(idx);
                safeThis->presetBox.setSelectedId(0, juce::dontSendNotification);
                safeThis->refreshPresetList();
            }
            delete aw;
        }));
}

// =============================================================================

void MW160Editor::paint(juce::Graphics& g)
{
    using namespace mw160::Palette;

    const float W = static_cast<float>(getWidth());
    const float H = static_cast<float>(getHeight());
    const float sx = W / kRefW;
    const float sy = H / kRefH;

    // --- Window background ---
    g.fillAll(bgDeep);

    // --- Body panel background ---
    {
        auto body = juce::Rectangle<float>(0.0f, 60.0f * sy, W, 240.0f * sy);
        g.setColour(bgPanel);
        g.fillRect(body);
    }

    // --- Header: brushed-metal faceplate band ---
    auto headerRect = juce::Rectangle<float>(0.0f, 0.0f, W, 60.0f * sy);
    {
        juce::ColourGradient hdrGrad(
            faceplateHigh, 0.0f, headerRect.getY(),
            faceplateLow,  0.0f, headerRect.getBottom(), false);
        hdrGrad.addColour(0.5, faceplateMid);
        g.setGradientFill(hdrGrad);
        g.fillRect(headerRect);

        // Brushed-metal scratches: a handful of faint horizontal lines at
        // deterministic positions.
        g.setColour(faceplateScratch.withAlpha(0.12f));
        for (int i = 0; i < 9; ++i)
        {
            const float yy = headerRect.getY() + headerRect.getHeight() * (0.1f + i * 0.085f);
            g.drawHorizontalLine(juce::roundToInt(yy),
                                 headerRect.getX() + 4.0f,
                                 headerRect.getRight() - 4.0f);
        }
    }

    // --- Wordmark "mw160" ---
    // §4.3 requires a two-pass draw for the engraved look.
    {
        auto wordmarkArea = juce::Rectangle<float>(64.0f * sx, 6.0f * sy, 360.0f * sx, 34.0f * sy);
        const auto wordFont = mw160::Fonts::interBold(28.0f * sy);

        g.setFont(wordFont);
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawText("mw160", wordmarkArea.translated(0.0f, 1.0f),
                   juce::Justification::centredLeft);
        g.setColour(textInk);
        g.drawText("mw160", wordmarkArea, juce::Justification::centredLeft);

        // Sublabel "VCA COMPRESSOR" — placed below the 34 px wordmark box
        // with an explicit gap to avoid overlap (DESIGN-REVIEW-002).
        auto subArea = juce::Rectangle<float>(64.0f * sx, 42.0f * sy, 360.0f * sx, 14.0f * sy);
        g.setFont(mw160::Fonts::withTracking(mw160::Fonts::interMedium(10.0f * sy), 2.0f));
        g.setColour(textInk.withAlpha(0.75f));
        g.drawText("VCA COMPRESSOR", subArea, juce::Justification::centredLeft);
    }

    // --- Fasteners (four corner screws) ---
    {
        const float screwR = 5.0f * juce::jmin(sx, sy);
        const juce::Point<float> screws[4] = {
            { 16.0f * sx, 16.0f * sy },
            { 16.0f * sx, 36.0f * sy },
            { 884.0f * sx, 16.0f * sy },
            { 884.0f * sx, 36.0f * sy }
        };
        for (int i = 0; i < 4; ++i)
            mw160::Screws::drawScrew(g, screws[i], screwR, i);
    }

    // --- Threshold LEDs (BELOW / ABOVE) ---
    {
        const float ledY    = 24.0f * sy;
        const float ledSize = 10.0f * juce::jmin(sx, sy);

        // BELOW group
        const float belowX = 448.0f * sx;
        drawLed(g, { belowX, ledY, ledSize, ledSize },
                ledGreen, ledGreenDark, ledBelow_);
        g.setColour(ledBelow_ ? textInk : textInk.withAlpha(0.5f));
        g.setFont(mw160::Fonts::withTracking(mw160::Fonts::interMedium(10.0f * sy), 1.0f));
        g.drawText("BELOW", belowX + ledSize + 6.0f * sx, ledY - 2.0f * sy,
                   56.0f * sx, ledSize + 4.0f * sy,
                   juce::Justification::centredLeft);

        // ABOVE group
        const float aboveX = 548.0f * sx;
        drawLed(g, { aboveX, ledY, ledSize, ledSize },
                ledRed, ledRedDark, ledAbove_);
        g.setColour(ledAbove_ ? textInk : textInk.withAlpha(0.5f));
        g.drawText("ABOVE", aboveX + ledSize + 6.0f * sx, ledY - 2.0f * sy,
                   56.0f * sx, ledSize + 4.0f * sy,
                   juce::Justification::centredLeft);
    }

    // --- Header / body separator ---
    {
        g.setColour(borderHairline);
        g.fillRect(8.0f * sx, 59.0f * sy, (kRefW - 16.0f) * sx, 1.0f);
    }

    // --- Meter strip panel (VISUAL_SPEC.md §2.2) ---
    {
        auto strip = scaleR({ 8.0f, 68.0f, 156.0f, 224.0f }, getWidth(), getHeight());
        g.setColour(bgDeep);
        g.fillRoundedRectangle(strip, 4.0f);
        g.setColour(borderHairline);
        g.drawRoundedRectangle(strip.reduced(0.5f), 4.0f,
                               mw160::Geometry::snap1px(1.0f, sx));
    }

    // --- Knob row panel (VISUAL_SPEC.md §2.3) ---
    {
        auto panel = scaleR({ 184.0f, 68.0f, 520.0f, 224.0f }, getWidth(), getHeight());
        g.setColour(surfaceLow);
        g.fillRoundedRectangle(panel, 4.0f);
        g.setColour(borderHairline);
        g.drawRoundedRectangle(panel.reduced(0.5f), 4.0f,
                               mw160::Geometry::snap1px(1.0f, sx));
    }

    // --- Switch cluster panel (VISUAL_SPEC.md §2.4) ---
    {
        auto panel = scaleR({ 720.0f, 68.0f, 172.0f, 224.0f }, getWidth(), getHeight());
        g.setColour(surfaceLow);
        g.fillRoundedRectangle(panel, 4.0f);
        g.setColour(borderHairline);
        g.drawRoundedRectangle(panel.reduced(0.5f), 4.0f,
                               mw160::Geometry::snap1px(1.0f, sx));
    }

    // --- Meter scale tick labels (VISUAL_SPEC.md §6.4 / §6.6) ---
    {
        const auto tickFont = mw160::Fonts::interRegular(8.0f * juce::jmin(sx, sy));
        g.setFont(tickFont);

        // §6.6: the selected mode's tick column is textBright; others textDim.
        const auto mode = meterModeButton.getMode();
        const bool inOutBright = (mode == MeterModeButton::Mode::In
                               || mode == MeterModeButton::Mode::Out);
        const bool grBright    = (mode == MeterModeButton::Mode::Gr);

        // Shared IN/OUT tick column between the two ladders.
        struct Tick { float ref_dB; const char* label; };
        const Tick inOutTicks[6] = {
            { +6.0f,  "+6"  }, {  0.0f,  "0"   }, { -6.0f,  "-6"  },
            { -12.0f, "-12" }, { -24.0f, "-24" }, { -48.0f, "-48" }
        };
        // Map dB to y within the IN/OUT ladder (bounds §2.2: y=96, h=176).
        auto inOutY = [sy](float db)
        {
            const float t = juce::jlimit(0.0f, 1.0f, (db - (-60.0f)) / (6.0f - (-60.0f)));
            const float y0 = 96.0f * sy;
            const float yH = 176.0f * sy;
            return y0 + yH * (1.0f - t);
        };
        // Tick column widened to 16 px (DESIGN-REVIEW-003): outputMeter shifted
        // to x=70 so the gap between IN right-edge (52) and OUT left-edge (70)
        // is 18 px, giving the label box enough room for "-48".
        const float shared_tickX = 51.0f * sx;  // tick mark start (just after IN right edge)
        const float shared_x    = 52.0f * sx;  // label start
        const float shared_w    = 18.0f * sx;  // wide enough for "-48"
        const auto  inOutCol    = inOutBright ? textBright : textDim;
        g.setColour(inOutCol);
        for (const auto& tk : inOutTicks)
        {
            const float yy = inOutY(tk.ref_dB);
            g.fillRect(shared_tickX, yy - 0.5f, 3.0f * sx, 1.0f);
            g.drawText(tk.label, shared_x, yy - 6.0f, shared_w, 12.0f,
                       juce::Justification::centred);
        }

        // GR ticks on the right of the GR ladder (§6.4).
        const Tick grTicks[5] = {
            { 0.0f,  "0"  }, { -3.0f,  "-3"  }, { -6.0f, "-6" },
            { -12.0f,"-12"}, { -20.0f, "-20" }
        };
        auto grY = [sy](float db)
        {
            // TopDown: 0 dB at top, -20 dB at bottom.
            const float t = juce::jlimit(0.0f, 1.0f, (0.0f - db) / 20.0f);
            const float y0 = 96.0f * sy;
            const float yH = 176.0f * sy;
            return y0 + yH * t;
        };
        const float gr_tickX = 140.0f * sx;  // tick mark start (right edge of ladder)
        const float gr_x    = 144.0f * sx;   // label start
        const float gr_w    = 16.0f  * sx;
        const auto  grCol   = grBright ? textBright : textDim;
        g.setColour(grCol);
        for (const auto& tk : grTicks)
        {
            const float yy = grY(tk.ref_dB);
            // Small tick mark connecting the label to the meter.
            g.fillRect(gr_tickX, yy - 0.5f, 3.0f * sx, 1.0f);
            g.drawText(tk.label, gr_x, yy - 6.0f, gr_w, 12.0f,
                       juce::Justification::centredLeft);
        }
    }

    // --- Footer band (VISUAL_SPEC.md §2.5) ---
    {
        auto footer = juce::Rectangle<float>(0.0f, 300.0f * sy, W, 60.0f * sy);
        juce::ColourGradient footGrad(
            surfaceHigh, 0.0f, footer.getY(),
            bgPanel,     0.0f, footer.getBottom(), false);
        g.setGradientFill(footGrad);
        g.fillRect(footer);

        g.setColour(borderHairline);
        g.fillRect(footer.getX(), footer.getY(), footer.getWidth(), 1.0f);
    }
}

// =============================================================================

void MW160Editor::resized()
{
    const int W = getWidth();
    const int H = getHeight();

    // --- Meter strip: three ladders ---
    // Bounds from VISUAL_SPEC.md §2.2.
    inputMeterLabel .setBounds(scaleI(20,  76, 36, 14, W, H));
    outputMeterLabel.setBounds(scaleI(66,  76, 36, 14, W, H));  // shifted +6 for wider tick column
    grMeterLabel    .setBounds(scaleI(114, 76, 36, 14, W, H));  // shifted +6

    inputMeter .setBounds(scaleI(24,  96, 28, 176, W, H));
    outputMeter.setBounds(scaleI(70,  96, 28, 176, W, H));  // shifted +6; tick column now 18 px
    grMeter    .setBounds(scaleI(118, 96, 28, 176, W, H));  // shifted +6

    // --- Knob row: four columns at 130 px width (§2.3) ---
    auto placeKnob = [&](juce::Slider& s, juce::Label& top,
                         juce::Label& value, juce::Label& unit, int colX)
    {
        top   .setBounds(scaleI(colX + 8,  80, 114, 16, W, H));
        s     .setBounds(scaleI(colX + 17, 104, 96, 96, W, H));
        value .setBounds(scaleI(colX + 8,  208, 114, 18, W, H));
        unit  .setBounds(scaleI(colX + 8,  228, 114, 14, W, H));
    };
    placeKnob(thresholdSlider,  thresholdLabel,  thresholdValue,  thresholdUnit,  184);
    placeKnob(ratioSlider,      ratioLabel,      ratioValue,      ratioUnit,      314);
    placeKnob(outputGainSlider, outputGainLabel, outputGainValue, outputGainUnit, 444);
    placeKnob(mixSlider,        mixLabel,        mixValue,        mixUnit,        574);

    // --- Switch cluster (§2.4) ---
    // Visual pill stays 36 px tall; the component is 44 px for WCAG 2.1
    // hit-target compliance (§11.1). The LookAndFeel centres the pill.
    kneeButton      .setBounds(scaleI(736,  88, 140, 44, W, H));
    stereoLinkButton.setBounds(scaleI(736, 132, 140, 44, W, H));
    bypassButton    .setBounds(scaleI(736, 176, 140, 44, W, H));
    meterModeButton .setBounds(scaleI(736, 228, 140, 44, W, H));

    // --- Footer (§2.5) ---
    presetLabel  .setBounds(scaleI(16,  312,  60, 14, W, H));
    presetBox    .setBounds(scaleI(16,  328, 540, 24, W, H));
    // Preset buttons: 64×36 hit target (§11.1), visual centred by LnF.
    saveButton   .setBounds(scaleI(564, 322,  64, 36, W, H));
    deleteButton .setBounds(scaleI(636, 322,  64, 36, W, H));
    versionLabel .setBounds(scaleI(708, 332, 176, 16, W, H));
}
