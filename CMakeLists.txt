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
    src/dsp/VcaSaturation.cpp
)
target_include_directories(MW160_DSP PUBLIC src)

# Plugin target
juce_add_plugin(MW160
    COMPANY_NAME "mwAudio"
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
    PRODUCT_NAME "mw160"
)

# ---------- Embedded font binary data (VISUAL_SPEC.md §4.1) ----------
juce_add_binary_data(FontBinaryData
    SOURCES
        assets/fonts/Inter-Regular.ttf
        assets/fonts/Inter-Medium.ttf
        assets/fonts/Inter-Bold.ttf
        assets/fonts/JetBrainsMono-Regular.ttf
)

target_sources(MW160
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/PresetManager.cpp
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
        FontBinaryData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---------- Umbrella target -------------------------------------------------
# JUCE auto-generates MW160_All (depends on MW160_VST3, MW160_AU, MW160_Standalone).
# Use  cmake --build build --target MW160_All  to build every plugin format.
# The bare "MW160" target only builds the shared-code static library.
# -----------------------------------------------------------------------------

# ---------- Test target (headless, no JUCE GUI dependency) ----------
enable_testing()

add_executable(MW160Tests
    tests/test_smoke.cpp
    tests/test_ballistics.cpp
    tests/test_compressor_integration.cpp
    tests/test_gain_computer.cpp
    tests/test_gain_computer_infinity_ratio.cpp
    tests/test_mix.cpp
    tests/test_parameter_smoothing.cpp
    tests/test_rms_detector.cpp
    tests/test_stereo_link.cpp
    tests/test_vca_saturation.cpp
    tests/test_metering.cpp
    tests/test_timing_calibration.cpp
    tests/test_gain_staging_calibration.cpp
    tests/test_golden_regression.cpp
    tests/test_performance.cpp
    tests/test_aliasing.cpp
    tests/test_factory_presets.cpp
    tests/test_plugin_processor_state.cpp
    tests/test_plugin_parameter_display_names.cpp
    tests/test_plugin_parameter_labels.cpp
    tests/test_parameter_smoother_block_independence.cpp
    tests/test_rms_detector_nan_guard.cpp
    tests/test_plugin_bypass.cpp
    tests/test_processblock_integration.cpp
    tests/test_preset_manager.cpp
)

target_link_libraries(MW160Tests
    PRIVATE
        MW160_DSP
        MW160
        juce::juce_audio_processors
        Catch2::Catch2WithMain
)

# Pass the golden reference directory path to tests at compile time
target_compile_definitions(MW160Tests
    PRIVATE
        GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden"
        _USE_MATH_DEFINES   # expose M_PI via <cmath> on MSVC; no-op on GCC/Clang
)

# ---------- No-alloc test (separate executable, overrides global operator new) ----------
add_executable(MW160NoAllocTests
    tests/test_no_alloc.cpp
)

target_link_libraries(MW160NoAllocTests
    PRIVATE
        MW160_DSP
        MW160
        juce::juce_audio_processors
        Catch2::Catch2WithMain
)

target_compile_definitions(MW160NoAllocTests
    PRIVATE
        _USE_MATH_DEFINES   # defensive: expose M_PI on MSVC if a future source needs it
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(MW160Tests)
catch_discover_tests(MW160NoAllocTests)
