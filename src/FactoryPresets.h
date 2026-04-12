#pragma once

namespace mw160 {

struct FactoryPresetData
{
    const char* name;
    float threshold;    // dBFS, range: [-40, +20]
    float ratio;        // :1,  range: [1, 60]
    float outputGain;   // dB,  range: [-20, +20]
    bool  overEasy;
    bool  stereoLink;
    float mix;          // %,   range: [0, 100]
};

inline constexpr FactoryPresetData kFactoryPresets[] =
{
    { "Kick Punch",      -10.0f,  4.0f, 4.0f, false, true,  100.0f },
    { "Snare Snap",       -8.0f,  5.0f, 3.0f, false, true,  100.0f },
    { "Drum Bus Glue",   -12.0f,  4.0f, 3.0f, true,  true,  100.0f },
    { "Bass Control",    -14.0f,  3.0f, 2.0f, true,  false, 100.0f },
    { "Parallel Smash",  -20.0f,  8.0f, 6.0f, false, true,   50.0f },
    { "Gentle Leveling",  -8.0f,  2.0f, 1.0f, true,  true,  100.0f },
    { "Brick Wall",       -6.0f, 60.0f, 0.0f, false, true,  100.0f },
};

inline constexpr int kNumFactoryPresets =
    static_cast<int>(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]));

} // namespace mw160
