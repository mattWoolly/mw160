#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/// Color palette for the mw160 editor.
///
/// Values are the exact hex constants from VISUAL_SPEC.md §3. The spec
/// requires these to be the only color literals anywhere in the editor --
/// `PluginEditor.cpp` must reference roles from this namespace, never raw
/// `0xff...` values.
namespace mw160::Palette
{
    // --- Backgrounds / panels ---
    inline const juce::Colour bgDeep           { 0xff161616 };
    inline const juce::Colour bgPanel          { 0xff1e1e1e };
    inline const juce::Colour surfaceLow       { 0xff222222 };
    inline const juce::Colour surfaceHigh      { 0xff2a2a2a };

    // --- Faceplate (brushed-metal band) ---
    inline const juce::Colour faceplateLow     { 0xff3a3a3a };
    inline const juce::Colour faceplateMid     { 0xff5a5a5a };
    inline const juce::Colour faceplateHigh    { 0xff6e6e6e };
    inline const juce::Colour faceplateScratch { 0xff7a7a7a };

    // --- Borders ---
    inline const juce::Colour borderHairline   { 0xff323232 };
    inline const juce::Colour borderRecessed   { 0xff0c0c0c };

    // --- Fasteners ---
    inline const juce::Colour screwBody        { 0xff3c3c3c };
    inline const juce::Colour screwHighlight   { 0xff8a8a8a };
    inline const juce::Colour screwSlot        { 0xff0a0a0a };

    // --- Knobs ---
    inline const juce::Colour knobMetalHigh    { 0xff6c6c6c };
    inline const juce::Colour knobMetalLow     { 0xff2c2c2c };
    inline const juce::Colour knobRing         { 0xff1a1a1a };
    inline const juce::Colour knobRingHighlight{ 0xff5a5a5a };
    inline const juce::Colour knobPointer      { 0xfff2f2f2 };
    inline const juce::Colour knobArcTrack     { 0xff2a2a2a };
    inline const juce::Colour knobArcFill      { 0xffc8a45c };
    inline const juce::Colour knobArcFillBypass{ 0xff5a5a5a };

    // --- Text ---
    inline const juce::Colour textBright       { 0xffececec };
    inline const juce::Colour textMid          { 0xffb8b8b8 };
    inline const juce::Colour textDim          { 0xff7a7a7a };
    inline const juce::Colour textInk          { 0xff0c0c0c };

    // --- LEDs ---
    inline const juce::Colour ledGreen         { 0xff27d05a };
    inline const juce::Colour ledGreenDark     { 0xff0e3a1a };
    inline const juce::Colour ledYellow        { 0xffffc830 };
    inline const juce::Colour ledYellowDark    { 0xff3a300e };
    inline const juce::Colour ledAmber         { 0xffff8a1c };
    inline const juce::Colour ledAmberDark     { 0xff3a220c };
    inline const juce::Colour ledRed           { 0xffff3826 };
    inline const juce::Colour ledRedDark       { 0xff3a0e0a };

    // --- Reserved (for a possible future sweep-meter variant -- do not
    //     draw in the baseline editor, per VISUAL_SPEC.md §6.1) ---
    inline const juce::Colour meterFaceCream   { 0xfff0e8cf };
    inline const juce::Colour meterRedZone     { 0xffc81818 };

    // --- Accents ---
    inline const juce::Colour accentEngagedGlow{ 0xffc8a45c };
}
