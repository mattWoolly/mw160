#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <chrono>
#include <cmath>
#include <vector>
#include <cstdio>
#include "dsp/Compressor.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<float> generateSine(double sampleRate, float amplitude,
                                       float frequency, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double twoPiOverSr = 2.0 * M_PI * frequency / sampleRate;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = amplitude
            * static_cast<float>(std::sin(twoPiOverSr * i));
    return buf;
}

// ---------------------------------------------------------------------------
// Performance measurement helper
// ---------------------------------------------------------------------------

struct PerfResult
{
    double wallTimeMs;      // total processing time in milliseconds
    double audioLengthMs;   // duration of processed audio in milliseconds
    double realtimeMultiple; // how many times faster than real-time
    double cpuPercent;      // estimated single-core CPU usage
};

// Process stereo audio through two Compressor instances (mirroring processBlock)
// and measure wall-clock time. Runs multiple iterations for stability.
static PerfResult measureStereoThroughput(double sampleRate, int blockSize,
                                          double audioDurationSec,
                                          bool softKnee, bool stereoLink,
                                          int iterations = 5)
{
    const int totalSamples = static_cast<int>(sampleRate * audioDurationSec);

    // Generate test signal: 1 kHz sine at -10 dBFS (typical compressed material)
    auto signalL = generateSine(sampleRate, 0.316f, 1000.0f, totalSamples);
    auto signalR = generateSine(sampleRate, 0.316f, 1200.0f, totalSamples);

    // Warm up compressors
    mw160::Compressor compL, compR;
    compL.prepare(sampleRate, blockSize);
    compR.prepare(sampleRate, blockSize);

    compL.setThreshold(-20.0f);
    compL.setRatio(4.0f);
    compL.setOutputGain(0.0f);
    compL.setSoftKnee(softKnee);
    compL.setMix(100.0f);

    compR.setThreshold(-20.0f);
    compR.setRatio(4.0f);
    compR.setOutputGain(0.0f);
    compR.setSoftKnee(softKnee);
    compR.setMix(100.0f);

    // Pre-warm: process 100ms to settle detector state
    const int warmupSamples = std::min(static_cast<int>(sampleRate * 0.1), totalSamples);
    for (int i = 0; i < warmupSamples; ++i)
    {
        if (stereoLink)
        {
            const double sqL = static_cast<double>(signalL[i]) * signalL[i];
            const double sqR = static_cast<double>(signalR[i]) * signalR[i];
            const double linked = (sqL + sqR) * 0.5;
            signalL[i] = compL.processSampleLinked(signalL[i], linked);
            signalR[i] = compR.processSampleLinked(signalR[i], linked);
        }
        else
        {
            signalL[i] = compL.processSample(signalL[i]);
            signalR[i] = compR.processSample(signalR[i]);
        }
    }

    // Timed section: process full audio duration, best of N iterations
    double bestTimeMs = 1e9;

    for (int iter = 0; iter < iterations; ++iter)
    {
        // Reset compressors to consistent state between iterations
        compL.prepare(sampleRate, blockSize);
        compR.prepare(sampleRate, blockSize);
        compL.setThreshold(-20.0f);
        compL.setRatio(4.0f);
        compL.setOutputGain(0.0f);
        compL.setSoftKnee(softKnee);
        compL.setMix(100.0f);
        compR.setThreshold(-20.0f);
        compR.setRatio(4.0f);
        compR.setOutputGain(0.0f);
        compR.setSoftKnee(softKnee);
        compR.setMix(100.0f);

        // Make working copies so each iteration processes same data
        auto workL = signalL;
        auto workR = signalR;

        auto start = std::chrono::high_resolution_clock::now();

        if (stereoLink)
        {
            for (int i = 0; i < totalSamples; ++i)
            {
                const double sqL = static_cast<double>(workL[i]) * workL[i];
                const double sqR = static_cast<double>(workR[i]) * workR[i];
                const double linked = (sqL + sqR) * 0.5;
                workL[i] = compL.processSampleLinked(workL[i], linked);
                workR[i] = compR.processSampleLinked(workR[i], linked);
            }
        }
        else
        {
            for (int i = 0; i < totalSamples; ++i)
            {
                workL[i] = compL.processSample(workL[i]);
                workR[i] = compR.processSample(workR[i]);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        bestTimeMs = std::min(bestTimeMs, ms);
    }

    const double audioLengthMs = audioDurationSec * 1000.0;
    const double realtimeMultiple = audioLengthMs / bestTimeMs;
    const double cpuPercent = (bestTimeMs / audioLengthMs) * 100.0;

    return { bestTimeMs, audioLengthMs, realtimeMultiple, cpuPercent };
}

// ---------------------------------------------------------------------------
// Benchmark tests
// ---------------------------------------------------------------------------

TEST_CASE("Performance: stereo throughput at 44.1 kHz, hard knee",
          "[performance][benchmark]")
{
    const auto result = measureStereoThroughput(
        44100.0, 512, /*audioDurationSec=*/2.0,
        /*softKnee=*/false, /*stereoLink=*/true);

    std::printf("\n  [44.1 kHz, hard knee, stereo-linked]\n");
    std::printf("    Wall time:    %.2f ms for %.0f ms audio\n",
                result.wallTimeMs, result.audioLengthMs);
    std::printf("    Real-time:    %.1fx\n", result.realtimeMultiple);
    std::printf("    CPU usage:    %.2f%%\n", result.cpuPercent);

    // Target: < 5% single-core CPU
    REQUIRE(result.cpuPercent < 5.0);
}

TEST_CASE("Performance: stereo throughput at 44.1 kHz, soft knee",
          "[performance][benchmark]")
{
    const auto result = measureStereoThroughput(
        44100.0, 512, /*audioDurationSec=*/2.0,
        /*softKnee=*/true, /*stereoLink=*/true);

    std::printf("\n  [44.1 kHz, soft knee, stereo-linked]\n");
    std::printf("    Wall time:    %.2f ms for %.0f ms audio\n",
                result.wallTimeMs, result.audioLengthMs);
    std::printf("    Real-time:    %.1fx\n", result.realtimeMultiple);
    std::printf("    CPU usage:    %.2f%%\n", result.cpuPercent);

    REQUIRE(result.cpuPercent < 5.0);
}

TEST_CASE("Performance: stereo throughput at 96 kHz, hard knee",
          "[performance][benchmark]")
{
    const auto result = measureStereoThroughput(
        96000.0, 512, /*audioDurationSec=*/2.0,
        /*softKnee=*/false, /*stereoLink=*/true);

    std::printf("\n  [96 kHz, hard knee, stereo-linked]\n");
    std::printf("    Wall time:    %.2f ms for %.0f ms audio\n",
                result.wallTimeMs, result.audioLengthMs);
    std::printf("    Real-time:    %.1fx\n", result.realtimeMultiple);
    std::printf("    CPU usage:    %.2f%%\n", result.cpuPercent);

    // 96 kHz is ~2.2x more samples; allow up to 11% CPU (proportional)
    REQUIRE(result.cpuPercent < 11.0);
}

TEST_CASE("Performance: stereo throughput at 96 kHz, soft knee",
          "[performance][benchmark]")
{
    const auto result = measureStereoThroughput(
        96000.0, 512, /*audioDurationSec=*/2.0,
        /*softKnee=*/true, /*stereoLink=*/true);

    std::printf("\n  [96 kHz, soft knee, stereo-linked]\n");
    std::printf("    Wall time:    %.2f ms for %.0f ms audio\n",
                result.wallTimeMs, result.audioLengthMs);
    std::printf("    Real-time:    %.1fx\n", result.realtimeMultiple);
    std::printf("    CPU usage:    %.2f%%\n", result.cpuPercent);

    REQUIRE(result.cpuPercent < 11.0);
}

TEST_CASE("Performance: independent channel mode throughput at 44.1 kHz",
          "[performance][benchmark]")
{
    const auto result = measureStereoThroughput(
        44100.0, 512, /*audioDurationSec=*/2.0,
        /*softKnee=*/false, /*stereoLink=*/false);

    std::printf("\n  [44.1 kHz, hard knee, independent channels]\n");
    std::printf("    Wall time:    %.2f ms for %.0f ms audio\n",
                result.wallTimeMs, result.audioLengthMs);
    std::printf("    Real-time:    %.1fx\n", result.realtimeMultiple);
    std::printf("    CPU usage:    %.2f%%\n", result.cpuPercent);

    REQUIRE(result.cpuPercent < 5.0);
}
