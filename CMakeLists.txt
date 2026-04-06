cmake_minimum_required(VERSION 3.22)
project(MW160 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE 7
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)

# Fetch Catch2 v3 for unit testing
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.4
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE Catch2)

# ---------- DSP library (pure C++, no JUCE dependency) ----------
add_library(MW160_DSP STATIC
    src/dsp/Ballistics.cpp
    src/dsp/Compressor.cpp
    src/dsp/GainComputer.cpp
    src/dsp/RmsDetector.cpp
)
target_include_directories(MW160_DSP PUBLIC src)

# Plugin target
juce_add_plugin(MW160
    COMPANY_NAME "MW Audio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MwAu
    PLUGIN_CODE Mw16
    BUNDLE_ID com.mwaudio.mw160
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "MW160"
)

target_sources(MW160
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
)

target_compile_definitions(MW160
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(MW160
    PRIVATE
        juce::juce_audio_utils
        MW160_DSP
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---------- Test target (headless, no JUCE GUI dependency) ----------
enable_testing()

add_executable(MW160Tests
    tests/test_smoke.cpp
    tests/test_ballistics.cpp
    tests/test_compressor_integration.cpp
    tests/test_gain_computer.cpp
    tests/test_rms_detector.cpp
)

target_link_libraries(MW160Tests
    PRIVATE
        MW160_DSP
        Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(MW160Tests)
