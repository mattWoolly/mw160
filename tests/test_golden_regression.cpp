#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "dsp/Compressor.h"

// =====================================================================
// Golden file format
// =====================================================================
// Header (20 bytes):
//   4 bytes  magic        "GLD1"
//   4 bytes  sampleCount  uint32_t
//   4 bytes  sampleRate   float (IEEE 754)
//   4 bytes  numChannels  uint32_t
//   4 bytes  reserved     (zero)
// Body:
//   sampleCount * numChannels * sizeof(float)  IEEE 754 float32 samples
//   Interleaved if multi-channel: L0 R0 L1 R1 ...
// =====================================================================

static constexpr char kMagic[4] = { 'G', 'L', 'D', '1' };
static constexpr uint32_t kHeaderSize = 20;

#ifndef GOLDEN_DIR
#error "GOLDEN_DIR must be defined by CMake"
#endif

// =====================================================================
// Helpers
// =====================================================================

static bool isRegenerateMode()
{
    const char* env = std::getenv("MW160_REGENERATE_GOLDEN");
    return env != nullptr && std::string(env) == "1";
}

static std::string goldenPath(const std::string& name)
{
    return std::string(GOLDEN_DIR) + "/" + name + ".golden";
}

static bool writeGoldenFile(const std::string& path,
                            const std::vector<float>& samples,
                            float sampleRate,
                            uint32_t numChannels)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    const uint32_t sampleCount = static_cast<uint32_t>(samples.size()) / numChannels;
    const uint32_t reserved = 0;

    out.write(kMagic, 4);
    out.write(reinterpret_cast<const char*>(&sampleCount), 4);
    out.write(reinterpret_cast<const char*>(&sampleRate), 4);
    out.write(reinterpret_cast<const char*>(&numChannels), 4);
    out.write(reinterpret_cast<const char*>(&reserved), 4);
    out.write(reinterpret_cast<const char*>(samples.data()),
              static_cast<std::streamsize>(samples.size() * sizeof(float)));
    return out.good();
}

struct GoldenData
{
    std::vector<float> samples;
    float sampleRate = 0.0f;
    uint32_t numChannels = 0;
    uint32_t sampleCount = 0;
};

static GoldenData readGoldenFile(const std::string& path)
{
    GoldenData data;
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        FAIL("Golden file not found: " << path
             << "\nRun with MW160_REGENERATE_GOLDEN=1 to generate.");
        return data;
    }

    char magic[4];
    in.read(magic, 4);
    REQUIRE(std::memcmp(magic, kMagic, 4) == 0);

    uint32_t reserved;
    in.read(reinterpret_cast<char*>(&data.sampleCount), 4);
    in.read(reinterpret_cast<char*>(&data.sampleRate), 4);
    in.read(reinterpret_cast<char*>(&data.numChannels), 4);
    in.read(reinterpret_cast<char*>(&reserved), 4);

    const size_t totalSamples = static_cast<size_t>(data.sampleCount) * data.numChannels;
    data.samples.resize(totalSamples);
    in.read(reinterpret_cast<char*>(data.samples.data()),
            static_cast<std::streamsize>(totalSamples * sizeof(float)));
    REQUIRE(in.good());

    return data;
}

// Generate a mono sine wave.
static std::vector<float> generateSine(double sampleRate, float amplitude,
                                       float frequency, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double w = 2.0 * M_PI * frequency / sampleRate;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = amplitude * static_cast<float>(std::sin(w * i));
    return buf;
}

// Amplitude for a sine whose RMS equals the given dBFS level.
static float sineAmplitudeForRms_dBFS(float rms_dBFS)
{
    return std::pow(10.0f, rms_dBFS / 20.0f) * std::sqrt(2.0f);
}

// Compute RMS of a float buffer.
static float measureRms(const std::vector<float>& buf)
{
    if (buf.empty()) return 0.0f;
    double sum = 0.0;
    for (float s : buf)
        sum += static_cast<double>(s) * static_cast<double>(s);
    return static_cast<float>(std::sqrt(sum / static_cast<double>(buf.size())));
}

// Compute RMS difference between two buffers in dB.
static float rmsDifference_dB(const std::vector<float>& a,
                               const std::vector<float>& b)
{
    REQUIRE(a.size() == b.size());
    std::vector<float> diff(a.size());
    for (size_t i = 0; i < a.size(); ++i)
        diff[i] = a[i] - b[i];
    const float diffRms = measureRms(diff);
    const float refRms = measureRms(a);
    if (refRms < 1e-10f) return (diffRms < 1e-10f) ? -200.0f : 0.0f;
    return 20.0f * std::log10(diffRms / refRms);
}

// Compute max absolute sample difference between two buffers.
static float maxAbsDifference(const std::vector<float>& a,
                               const std::vector<float>& b)
{
    REQUIRE(a.size() == b.size());
    float maxDiff = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
        maxDiff = std::max(maxDiff, std::abs(a[i] - b[i]));
    return maxDiff;
}

// =====================================================================
// Test configuration struct
// =====================================================================

struct GoldenTestConfig
{
    std::string name;
    float sampleRate = 44100.0f;
    float threshold_dB = -20.0f;
    float ratio = 4.0f;
    float outputGain_dB = 0.0f;
    bool overEasy = false;
    float mix = 100.0f;
    float inputAmplitude = 0.0f;   // 0 = silence
    float inputFrequency = 1000.0f;
    int durationSamples = 22050;   // 0.5s at 44.1kHz
};

// Process mono input through compressor with given config, return output.
static std::vector<float> processConfig(const GoldenTestConfig& cfg,
                                        const std::vector<float>& input)
{
    mw160::Compressor comp;
    comp.prepare(static_cast<double>(cfg.sampleRate), 512);
    comp.setThreshold(cfg.threshold_dB);
    comp.setRatio(cfg.ratio);
    comp.setOutputGain(cfg.outputGain_dB);
    comp.setOverEasy(cfg.overEasy);
    comp.setMix(cfg.mix);

    std::vector<float> output(input.size());
    for (size_t i = 0; i < input.size(); ++i)
        output[i] = comp.processSample(input[i]);
    return output;
}

// Run a golden test: generate input, process, compare or write golden file.
static void runGoldenTest(const GoldenTestConfig& cfg)
{
    // Generate input
    std::vector<float> input;
    if (cfg.inputAmplitude > 0.0f)
        input = generateSine(static_cast<double>(cfg.sampleRate),
                             cfg.inputAmplitude, cfg.inputFrequency,
                             cfg.durationSamples);
    else
        input.assign(static_cast<size_t>(cfg.durationSamples), 0.0f);

    // Process
    const auto output = processConfig(cfg, input);

    const std::string path = goldenPath(cfg.name);

    if (isRegenerateMode())
    {
        INFO("Regenerating golden file: " << path);
        REQUIRE(writeGoldenFile(path, output, cfg.sampleRate, 1));
        return;
    }

    // Compare against golden reference
    const auto golden = readGoldenFile(path);
    REQUIRE(golden.sampleCount == static_cast<uint32_t>(cfg.durationSamples));
    REQUIRE(golden.numChannels == 1);
    REQUIRE(golden.sampleRate == cfg.sampleRate);
    REQUIRE(golden.samples.size() == output.size());

    // Check max absolute sample difference
    const float maxDiff = maxAbsDifference(output, golden.samples);
    INFO("Max absolute sample difference: " << maxDiff);
    REQUIRE(maxDiff < 1e-6f);

    // Check RMS difference (should be well below -60 dB relative to signal)
    if (cfg.inputAmplitude > 0.0f)
    {
        const float rmsDiff_dB = rmsDifference_dB(golden.samples, output);
        INFO("RMS difference: " << rmsDiff_dB << " dB");
        REQUIRE(rmsDiff_dB < -80.0f);
    }
}

// =====================================================================
// Stereo linked golden test
// =====================================================================

struct StereoGoldenTestConfig
{
    std::string name;
    float sampleRate = 44100.0f;
    float threshold_dB = -20.0f;
    float ratio = 4.0f;
    float outputGain_dB = 0.0f;
    bool overEasy = false;
    float mix = 100.0f;
    float leftAmplitude = 0.0f;
    float rightAmplitude = 0.0f;
    float leftFrequency = 1000.0f;
    float rightFrequency = 1000.0f;
    int durationSamples = 22050;
};

static void runStereoGoldenTest(const StereoGoldenTestConfig& cfg)
{
    // Generate stereo input
    auto inputL = (cfg.leftAmplitude > 0.0f)
        ? generateSine(static_cast<double>(cfg.sampleRate),
                       cfg.leftAmplitude, cfg.leftFrequency, cfg.durationSamples)
        : std::vector<float>(static_cast<size_t>(cfg.durationSamples), 0.0f);

    auto inputR = (cfg.rightAmplitude > 0.0f)
        ? generateSine(static_cast<double>(cfg.sampleRate),
                       cfg.rightAmplitude, cfg.rightFrequency, cfg.durationSamples)
        : std::vector<float>(static_cast<size_t>(cfg.durationSamples), 0.0f);

    // Set up two linked compressors
    mw160::Compressor compL, compR;
    compL.prepare(static_cast<double>(cfg.sampleRate), 512);
    compR.prepare(static_cast<double>(cfg.sampleRate), 512);

    for (auto* comp : { &compL, &compR })
    {
        comp->setThreshold(cfg.threshold_dB);
        comp->setRatio(cfg.ratio);
        comp->setOutputGain(cfg.outputGain_dB);
        comp->setOverEasy(cfg.overEasy);
        comp->setMix(cfg.mix);
    }

    // Process with stereo linking
    std::vector<float> outputInterleaved(static_cast<size_t>(cfg.durationSamples) * 2);
    for (int i = 0; i < cfg.durationSamples; ++i)
    {
        const double sqL = static_cast<double>(inputL[static_cast<size_t>(i)])
                         * static_cast<double>(inputL[static_cast<size_t>(i)]);
        const double sqR = static_cast<double>(inputR[static_cast<size_t>(i)])
                         * static_cast<double>(inputR[static_cast<size_t>(i)]);
        const double linkedSquared = (sqL + sqR) * 0.5;

        outputInterleaved[static_cast<size_t>(i) * 2]     =
            compL.processSampleLinked(inputL[static_cast<size_t>(i)], linkedSquared);
        outputInterleaved[static_cast<size_t>(i) * 2 + 1] =
            compR.processSampleLinked(inputR[static_cast<size_t>(i)], linkedSquared);
    }

    const std::string path = goldenPath(cfg.name);

    if (isRegenerateMode())
    {
        INFO("Regenerating golden file: " << path);
        REQUIRE(writeGoldenFile(path, outputInterleaved, cfg.sampleRate, 2));
        return;
    }

    // Compare against golden reference
    const auto golden = readGoldenFile(path);
    REQUIRE(golden.sampleCount == static_cast<uint32_t>(cfg.durationSamples));
    REQUIRE(golden.numChannels == 2);
    REQUIRE(golden.sampleRate == cfg.sampleRate);
    REQUIRE(golden.samples.size() == outputInterleaved.size());

    const float maxDiff = maxAbsDifference(outputInterleaved, golden.samples);
    INFO("Max absolute sample difference: " << maxDiff);
    REQUIRE(maxDiff < 1e-6f);
}

// =====================================================================
// Test cases
// =====================================================================

static constexpr double kSampleRate = 44100.0;
static constexpr int kDuration = 22050;  // 0.5 seconds

TEST_CASE("Golden: 1kHz sine, threshold -20, ratio 4:1",
          "[golden][regression]")
{
    GoldenTestConfig cfg;
    cfg.name = "sine_1k_thresh-20_ratio4";
    cfg.sampleRate = static_cast<float>(kSampleRate);
    cfg.threshold_dB = -20.0f;
    cfg.ratio = 4.0f;
    cfg.inputAmplitude = sineAmplitudeForRms_dBFS(-6.0f);
    cfg.durationSamples = kDuration;
    runGoldenTest(cfg);
}

TEST_CASE("Golden: 1kHz sine, threshold -10, ratio 8:1 (heavy)",
          "[golden][regression]")
{
    GoldenTestConfig cfg;
    cfg.name = "sine_1k_thresh-10_ratio8";
    cfg.sampleRate = static_cast<float>(kSampleRate);
    cfg.threshold_dB = -10.0f;
    cfg.ratio = 8.0f;
    cfg.inputAmplitude = sineAmplitudeForRms_dBFS(-6.0f);
    cfg.durationSamples = kDuration;
    runGoldenTest(cfg);
}

TEST_CASE("Golden: 1kHz sine, threshold -20, ratio 60:1 (limiter)",
          "[golden][regression]")
{
    GoldenTestConfig cfg;
    cfg.name = "sine_1k_thresh-20_ratio60";
    cfg.sampleRate = static_cast<float>(kSampleRate);
    cfg.threshold_dB = -20.0f;
    cfg.ratio = 60.0f;
    cfg.inputAmplitude = sineAmplitudeForRms_dBFS(-6.0f);
    cfg.durationSamples = kDuration;
    runGoldenTest(cfg);
}

TEST_CASE("Golden: 1kHz sine, threshold -20, ratio 4:1, OverEasy",
          "[golden][regression]")
{
    GoldenTestConfig cfg;
    cfg.name = "sine_1k_thresh-20_ratio4_overeasy";
    cfg.sampleRate = static_cast<float>(kSampleRate);
    cfg.threshold_dB = -20.0f;
    cfg.ratio = 4.0f;
    cfg.overEasy = true;
    cfg.inputAmplitude = sineAmplitudeForRms_dBFS(-6.0f);
    cfg.durationSamples = kDuration;
    runGoldenTest(cfg);
}

TEST_CASE("Golden: silence produces silence",
          "[golden][regression]")
{
    GoldenTestConfig cfg;
    cfg.name = "silence";
    cfg.sampleRate = static_cast<float>(kSampleRate);
    cfg.threshold_dB = -20.0f;
    cfg.ratio = 4.0f;
    cfg.inputAmplitude = 0.0f;  // silence
    cfg.durationSamples = kDuration;
    runGoldenTest(cfg);
}

TEST_CASE("Golden: extreme settings (threshold -40, ratio 60:1)",
          "[golden][regression]")
{
    GoldenTestConfig cfg;
    cfg.name = "sine_1k_thresh-40_ratio60_extreme";
    cfg.sampleRate = static_cast<float>(kSampleRate);
    cfg.threshold_dB = -40.0f;
    cfg.ratio = 60.0f;
    cfg.inputAmplitude = sineAmplitudeForRms_dBFS(0.0f);
    cfg.durationSamples = kDuration;
    runGoldenTest(cfg);
}

TEST_CASE("Golden: 1kHz sine with 50% mix (parallel compression)",
          "[golden][regression]")
{
    GoldenTestConfig cfg;
    cfg.name = "sine_1k_thresh-20_ratio4_mix50";
    cfg.sampleRate = static_cast<float>(kSampleRate);
    cfg.threshold_dB = -20.0f;
    cfg.ratio = 4.0f;
    cfg.mix = 50.0f;
    cfg.inputAmplitude = sineAmplitudeForRms_dBFS(-6.0f);
    cfg.durationSamples = kDuration;
    runGoldenTest(cfg);
}

TEST_CASE("Golden: stereo linked, different amplitudes per channel",
          "[golden][regression]")
{
    StereoGoldenTestConfig cfg;
    cfg.name = "stereo_linked_asym";
    cfg.sampleRate = static_cast<float>(kSampleRate);
    cfg.threshold_dB = -20.0f;
    cfg.ratio = 4.0f;
    cfg.leftAmplitude = sineAmplitudeForRms_dBFS(-6.0f);
    cfg.rightAmplitude = sineAmplitudeForRms_dBFS(-12.0f);
    cfg.leftFrequency = 1000.0f;
    cfg.rightFrequency = 1000.0f;
    cfg.durationSamples = kDuration;
    runStereoGoldenTest(cfg);
}
