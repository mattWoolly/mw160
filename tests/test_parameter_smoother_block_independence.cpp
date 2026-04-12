#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include "dsp/ParameterSmoother.h"

// Simulate what MW160Processor::processBlock does: call setTarget once per
// block (unconditionally, with the same value every block), then drain the
// smoother sample-by-sample for the duration of that block.
static std::vector<float> runAtBlockSize(int blockSize, int numBlocks)
{
    mw160::ParameterSmoother<mw160::SmoothingType::Linear> smoother;
    smoother.reset(48000.0, 0.020); // 20 ms ramp at 48 kHz → 960 steps
    smoother.setCurrentAndTarget(0.0f); // start at 0, heading nowhere yet

    std::vector<float> output;
    output.reserve(static_cast<size_t>(blockSize * numBlocks));

    for (int b = 0; b < numBlocks; ++b)
    {
        // Mimic processBlock: unconditional setTarget at the top of every
        // block. The target stays constant (1.0) across all blocks.
        smoother.setTarget(1.0f);

        for (int i = 0; i < blockSize; ++i)
            output.push_back(smoother.getNextValue());
    }
    return output;
}

// --- QA-DSP-001: block-size independence regression tests ---

TEST_CASE("ParameterSmoother output is block-size independent under repeated identical setTarget",
          "[dsp][parameter-smoother][QA-DSP-001]")
{
    // 4096 total samples spans well past the 20 ms ramp (960 samples at 48 kHz).
    // Both block sizes must produce the same sample-by-sample trajectory.
    const int totalSamples = 4096;

    const auto small = runAtBlockSize(64,   totalSamples / 64);   // 64 blocks
    const auto large = runAtBlockSize(1024, totalSamples / 1024); // 4 blocks

    REQUIRE(small.size() == large.size());

    // Every sample must agree within float tolerance.
    // Without the fix: the 64-sample-block smoother re-triggers the ramp
    // 64 times before it would naturally converge, yielding a trajectory
    // that lags the 1024-block version by ~0.01-0.02 at each index.
    constexpr float kTolerance = 1.0e-5f;
    for (size_t i = 0; i < small.size(); ++i)
    {
        REQUIRE_THAT(small[i], Catch::Matchers::WithinAbs(large[i], kTolerance));
    }
}

TEST_CASE("ParameterSmoother block-size independence: max dB difference < 0.1 dB",
          "[dsp][parameter-smoother][QA-DSP-001]")
{
    // Quantitative bar from the ticket spec: measured delta must be < 0.1 dB.
    const int totalSamples = 4096;

    const auto small = runAtBlockSize(64,   totalSamples / 64);
    const auto large = runAtBlockSize(1024, totalSamples / 1024);

    REQUIRE(small.size() == large.size());

    float maxAbsDiff_dB = 0.0f;
    for (size_t i = 0; i < small.size(); ++i)
    {
        const float s = small[i];
        const float l = large[i];
        if (s > 1e-6f && l > 1e-6f)
        {
            const float diff_dB = std::fabs(20.0f * std::log10(s / l));
            if (diff_dB > maxAbsDiff_dB)
                maxAbsDiff_dB = diff_dB;
        }
    }

    REQUIRE(maxAbsDiff_dB < 0.1f);
}

TEST_CASE("ParameterSmoother: legitimate target change still restarts ramp correctly",
          "[dsp][parameter-smoother][QA-DSP-001]")
{
    // Guard against the fix accidentally suppressing genuine target changes.
    // Set target to 0.5, process some samples, then change to 1.0 — the
    // smoother must ramp toward the new target (not stay at the old one).
    mw160::ParameterSmoother<mw160::SmoothingType::Linear> smoother;
    smoother.reset(48000.0, 0.020); // 960-step ramp
    smoother.setCurrentAndTarget(0.0f);

    // Start ramping toward 0.5
    smoother.setTarget(0.5f);

    // Drain half the ramp (480 samples)
    for (int i = 0; i < 480; ++i)
        smoother.getNextValue();

    // Now change the target mid-ramp to 1.0. The smoother must accept the
    // new destination and eventually reach it.
    smoother.setTarget(1.0f);

    // Drain enough samples to complete any 20 ms ramp from the current
    // position (960 more samples is more than enough).
    float lastValue = 0.0f;
    for (int i = 0; i < 960; ++i)
        lastValue = smoother.getNextValue();

    // After draining, the smoother must have reached (or be at) 1.0.
    REQUIRE_THAT(lastValue, Catch::Matchers::WithinAbs(1.0f, 1.0e-5f));
}
