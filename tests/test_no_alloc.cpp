#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <atomic>
#include <cstdlib>
#include <new>
#include "dsp/Compressor.h"
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------------
// Heap allocation tracking
// ---------------------------------------------------------------------------
// Override global operator new/delete to detect allocations during audio
// processing. A thread-local flag controls whether allocations are being
// monitored. When the flag is set, any allocation increments a counter.
//
// This approach is safe with Catch2: the flag is only enabled during the
// timed processing section, so Catch2's own allocations are not affected.
// ---------------------------------------------------------------------------

namespace {
    thread_local bool g_trackingAllocations = false;
    thread_local int  g_allocationCount = 0;
}

// NOLINTBEGIN(misc-new-delete-overloads)
void* operator new(std::size_t size)
{
    if (g_trackingAllocations)
        ++g_allocationCount;

    void* p = std::malloc(size);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t size)
{
    if (g_trackingAllocations)
        ++g_allocationCount;

    void* p = std::malloc(size);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
// NOLINTEND(misc-new-delete-overloads)

// RAII guard to enable/disable allocation tracking
struct AllocationTracker
{
    AllocationTracker()
    {
        g_allocationCount = 0;
        g_trackingAllocations = true;
    }
    ~AllocationTracker()
    {
        g_trackingAllocations = false;
    }

    int count() const { return g_allocationCount; }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("No-alloc: processSample makes zero heap allocations",
          "[performance][no-alloc]")
{
    // Set up compressor outside tracking zone
    mw160::Compressor comp;
    comp.prepare(44100.0, 512);
    comp.setThreshold(-20.0f);
    comp.setRatio(4.0f);
    comp.setOutputGain(3.0f);
    comp.setSoftKnee(false);
    comp.setMix(100.0f);

    // Pre-warm the compressor so any lazy init has happened
    for (int i = 0; i < 4410; ++i)
        comp.processSample(0.3f * std::sin(static_cast<float>(i) * 0.1f));

    // NOW track allocations: process 1 second of audio
    {
        AllocationTracker tracker;

        for (int i = 0; i < 44100; ++i)
        {
            const float sample = 0.3f * std::sin(static_cast<float>(i) * 0.1f);
            comp.processSample(sample);
        }

        REQUIRE(tracker.count() == 0);
    }
}

TEST_CASE("No-alloc: processSampleLinked makes zero heap allocations",
          "[performance][no-alloc]")
{
    mw160::Compressor compL, compR;
    compL.prepare(44100.0, 512);
    compR.prepare(44100.0, 512);

    compL.setThreshold(-20.0f);  compR.setThreshold(-20.0f);
    compL.setRatio(4.0f);        compR.setRatio(4.0f);
    compL.setOutputGain(0.0f);   compR.setOutputGain(0.0f);
    compL.setSoftKnee(true);     compR.setSoftKnee(true);
    compL.setMix(100.0f);        compR.setMix(100.0f);

    // Pre-warm
    for (int i = 0; i < 4410; ++i)
    {
        float sL = 0.3f * std::sin(static_cast<float>(i) * 0.1f);
        float sR = 0.3f * std::sin(static_cast<float>(i) * 0.13f);
        double sqL = static_cast<double>(sL) * sL;
        double sqR = static_cast<double>(sR) * sR;
        double linked = (sqL + sqR) * 0.5;
        compL.processSampleLinked(sL, linked);
        compR.processSampleLinked(sR, linked);
    }

    // Track allocations: process 1 second stereo-linked
    {
        AllocationTracker tracker;

        for (int i = 0; i < 44100; ++i)
        {
            float sL = 0.3f * std::sin(static_cast<float>(i) * 0.1f);
            float sR = 0.3f * std::sin(static_cast<float>(i) * 0.13f);
            double sqL = static_cast<double>(sL) * sL;
            double sqR = static_cast<double>(sR) * sR;
            double linked = (sqL + sqR) * 0.5;
            compL.processSampleLinked(sL, linked);
            compR.processSampleLinked(sR, linked);
        }

        REQUIRE(tracker.count() == 0);
    }
}

TEST_CASE("No-alloc: parameter changes during processing cause no allocations",
          "[performance][no-alloc]")
{
    mw160::Compressor comp;
    comp.prepare(44100.0, 512);
    comp.setThreshold(-20.0f);
    comp.setRatio(4.0f);
    comp.setOutputGain(0.0f);
    comp.setSoftKnee(false);
    comp.setMix(100.0f);

    // Pre-warm
    for (int i = 0; i < 4410; ++i)
        comp.processSample(0.3f * std::sin(static_cast<float>(i) * 0.1f));

    // Track allocations while changing parameters mid-stream
    {
        AllocationTracker tracker;

        for (int block = 0; block < 100; ++block)
        {
            // Simulate parameter changes every block (512 samples)
            comp.setThreshold(-20.0f + static_cast<float>(block % 10));
            comp.setRatio(2.0f + static_cast<float>(block % 8));
            comp.setOutputGain(static_cast<float>(block % 6) - 3.0f);
            comp.setSoftKnee(block % 2 == 0);
            comp.setMix(50.0f + static_cast<float>(block % 50));

            for (int i = 0; i < 512; ++i)
            {
                const float sample = 0.3f * std::sin(
                    static_cast<float>(block * 512 + i) * 0.1f);
                comp.processSample(sample);
            }
        }

        REQUIRE(tracker.count() == 0);
    }
}

TEST_CASE("No-alloc: MW160Processor::processBlock makes zero heap allocations",
          "[performance][no-alloc][plugin]")
{
    // Construct and prepare outside the tracking zone so JUCE internals,
    // APVTS setup, and any lazy compressor initialisation don't pollute the
    // allocation count.
    MW160Processor processor;

    constexpr double kSampleRate = 44100.0;
    constexpr int    kBlockSize  = 512;

    processor.prepareToPlay(kSampleRate, kBlockSize);

    // Ensure bypass is off — processBlock only heap-allocates the dry-copy
    // buffer during the ~20 ms bypass crossfade. The active (non-bypass) path
    // must be alloc-free.
    auto* bypassParam = processor.apvts.getParameter("bypass");
    REQUIRE(bypassParam != nullptr);
    bypassParam->setValueNotifyingHost(0.0f);

    // Build a stereo sine buffer and MIDI buffer outside the fence.
    juce::AudioBuffer<float> buffer(2, kBlockSize);
    juce::MidiBuffer         midi;

    {
        float* dataL = buffer.getWritePointer(0);
        float* dataR = buffer.getWritePointer(1);
        for (int i = 0; i < kBlockSize; ++i)
        {
            const float sample = 0.3f * std::sin(static_cast<float>(i) * 0.1f);
            dataL[i] = sample;
            dataR[i] = sample;
        }
    }

    // Pre-warm: run several blocks so ballistics, smoothers, and any lazy
    // internal state are fully settled before we start counting allocations.
    for (int b = 0; b < 100; ++b)
    {
        float* dataL = buffer.getWritePointer(0);
        float* dataR = buffer.getWritePointer(1);
        for (int i = 0; i < kBlockSize; ++i)
        {
            const float sample = 0.3f * std::sin(
                static_cast<float>(b * kBlockSize + i) * 0.1f);
            dataL[i] = sample;
            dataR[i] = sample;
        }
        processor.processBlock(buffer, midi);
    }

    // NOW track allocations: call processBlock for 1 second of audio (86 blocks).
    {
        AllocationTracker tracker;

        for (int b = 0; b < 86; ++b)
        {
            float* dataL = buffer.getWritePointer(0);
            float* dataR = buffer.getWritePointer(1);
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float sample = 0.3f * std::sin(
                    static_cast<float>((100 + b) * kBlockSize + i) * 0.1f);
                dataL[i] = sample;
                dataR[i] = sample;
            }
            processor.processBlock(buffer, midi);
        }

        REQUIRE(tracker.count() == 0);
    }
}
