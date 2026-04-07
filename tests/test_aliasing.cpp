#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>
#include <cstdio>
#include "dsp/VcaSaturation.h"
#include "dsp/Compressor.h"

// ---------------------------------------------------------------------------
// Helpers (single-bin DFT for spectral analysis)
// ---------------------------------------------------------------------------

static std::vector<float> generateSine(double sampleRate, float amplitude,
                                       double frequency, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    const double omega = 2.0 * M_PI * frequency / sampleRate;
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = amplitude
            * static_cast<float>(std::sin(omega * i));
    return buf;
}

static double measureBinAmplitude(const std::vector<float>& buf,
                                  double sampleRate, double frequency,
                                  int startSample, int numSamples)
{
    double sinSum = 0.0, cosSum = 0.0;
    const double omega = 2.0 * M_PI * frequency / sampleRate;
    for (int i = 0; i < numSamples; ++i)
    {
        const double s = static_cast<double>(
            buf[static_cast<size_t>(startSample + i)]);
        const double phase = omega * (startSample + i);
        sinSum += s * std::sin(phase);
        cosSum += s * std::cos(phase);
    }
    sinSum *= 2.0 / numSamples;
    cosSum *= 2.0 / numSamples;
    return std::sqrt(sinSum * sinSum + cosSum * cosSum);
}

// ---------------------------------------------------------------------------
// Aliasing tests for VcaSaturation
// ---------------------------------------------------------------------------
// The waveshaper produces 2nd and 3rd harmonics. For a fundamental at
// frequency f, the harmonics land at 2f and 3f. If 2f or 3f exceed
// Nyquist (fs/2), they alias back into the audible band.
//
// At 44.1 kHz:
//   - f = 5 kHz:  2f = 10 kHz (ok), 3f = 15 kHz (ok)
//   - f = 8 kHz:  2f = 16 kHz (ok), 3f = 24 kHz -> aliases to 20.1 kHz (ok, above audible)
//   - f = 10 kHz: 2f = 20 kHz (ok), 3f = 30 kHz -> aliases to 14.1 kHz (AUDIBLE)
//   - f = 12 kHz: 2f = 24 kHz -> aliases to 20.1 kHz, 3f = 36 kHz -> aliases to 8.1 kHz (AUDIBLE)
//
// Because the saturation amount is very subtle (THD < 0.2%), the aliased
// components should be well below the audible threshold. We verify this.
// ---------------------------------------------------------------------------

TEST_CASE("Aliasing: VcaSaturation aliased harmonics at 44.1 kHz, 10 kHz fundamental",
          "[performance][aliasing]")
{
    static constexpr double kSampleRate = 44100.0;
    const mw160::VcaSaturation sat;

    // Test with a 10 kHz fundamental (worst reasonable case for music content).
    // 3f = 30 kHz aliases to 44.1 - 30 = 14.1 kHz.
    // NOTE: This tests the isolated waveshaper at full amplitude — a worst
    // case that doesn't occur in practice because the compressor reduces
    // the signal before saturation. See the full-chain test below.
    const double fundamental = 10000.0;
    const float amplitude = 0.5f;
    const float grAmount = -20.0f; // moderate GR for significant saturation

    const int numSamples = 44100; // 1 second for good frequency resolution

    auto input = generateSine(kSampleRate, amplitude, fundamental, numSamples);
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i)
        output[i] = sat.processSample(input[i], grAmount);

    // Measure fundamental amplitude
    const double h1 = measureBinAmplitude(output, kSampleRate, fundamental,
                                          0, numSamples);
    const double h1_dBFS = 20.0 * std::log10(h1);

    // Measure aliased 3rd harmonic: 30 kHz aliases to 44.1 - 30 = 14.1 kHz
    const double alias3f = kSampleRate - 3.0 * fundamental;
    const double aliasAmp = measureBinAmplitude(output, kSampleRate, alias3f,
                                                0, numSamples);
    const double alias_dBFS = (aliasAmp > 1e-15)
                              ? 20.0 * std::log10(aliasAmp)
                              : -200.0;

    std::printf("\n  [Isolated VcaSaturation aliasing @ 44.1 kHz, f=%g Hz, GR=%.0f dB]\n",
                fundamental, grAmount);
    std::printf("    Fundamental (%.0f Hz):      %.1f dBFS\n",
                fundamental, h1_dBFS);
    std::printf("    Aliased 3f  (%.0f Hz):      %.1f dBFS\n",
                alias3f, alias_dBFS);
    std::printf("    Alias below fundamental:  %.1f dB\n",
                h1_dBFS - alias_dBFS);

    // Isolated waveshaper worst case: aliased 3rd harmonic should be at
    // least 70 dB below the fundamental. In the full compressor chain,
    // levels are much lower (see full-chain test).
    REQUIRE(alias_dBFS - h1_dBFS < -70.0);
}

TEST_CASE("Aliasing: VcaSaturation aliased harmonics at 44.1 kHz, 12 kHz fundamental",
          "[performance][aliasing]")
{
    static constexpr double kSampleRate = 44100.0;
    const mw160::VcaSaturation sat;

    // Worst-case isolated test: 12 kHz at heavy GR.
    // 2f = 24 kHz aliases to 20.1 kHz (above most hearing range)
    // 3f = 36 kHz aliases to 8.1 kHz (audible frequency but very low level)
    const double fundamental = 12000.0;
    const float amplitude = 0.5f;
    const float grAmount = -30.0f; // heavier GR for more saturation

    const int numSamples = 44100;

    auto input = generateSine(kSampleRate, amplitude, fundamental, numSamples);
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i)
        output[i] = sat.processSample(input[i], grAmount);

    const double h1 = measureBinAmplitude(output, kSampleRate, fundamental,
                                          0, numSamples);
    const double h1_dBFS = 20.0 * std::log10(h1);

    // 3f = 36 kHz aliases to 44.1 - 36 = 8.1 kHz
    const double alias3f = kSampleRate - 3.0 * fundamental;
    const double alias3Amp = measureBinAmplitude(output, kSampleRate, alias3f,
                                                 0, numSamples);
    const double alias3_dBFS = (alias3Amp > 1e-15)
                               ? 20.0 * std::log10(alias3Amp)
                               : -200.0;

    // 2f = 24 kHz aliases to 44.1 - 24 = 20.1 kHz
    const double alias2f = kSampleRate - 2.0 * fundamental;
    const double alias2Amp = measureBinAmplitude(output, kSampleRate, alias2f,
                                                 0, numSamples);
    const double alias2_dBFS = (alias2Amp > 1e-15)
                               ? 20.0 * std::log10(alias2Amp)
                               : -200.0;

    std::printf("\n  [Isolated VcaSaturation aliasing @ 44.1 kHz, f=%g Hz, GR=%.0f dB]\n",
                fundamental, grAmount);
    std::printf("    Fundamental  (%.0f Hz):     %.1f dBFS\n",
                fundamental, h1_dBFS);
    std::printf("    Aliased 2f   (%.0f Hz):     %.1f dBFS\n",
                alias2f, alias2_dBFS);
    std::printf("    Aliased 3f   (%.0f Hz):     %.1f dBFS\n",
                alias3f, alias3_dBFS);

    // Aliased 3rd harmonic at 8.1 kHz (in audible range) should be at
    // least 70 dB below the fundamental. The aliased 2nd harmonic at
    // 20.1 kHz is above the audible range for most listeners.
    REQUIRE(alias3_dBFS - h1_dBFS < -70.0);
}

TEST_CASE("Aliasing: full compressor chain aliased harmonics at 44.1 kHz",
          "[performance][aliasing]")
{
    static constexpr double kSampleRate = 44100.0;

    // Test through the full compressor (not just isolated VcaSaturation)
    // to catch any aliasing from the complete signal path.
    mw160::Compressor comp;
    comp.prepare(kSampleRate, 512);
    comp.setThreshold(-20.0f);
    comp.setRatio(8.0f);        // heavy compression for maximum saturation
    comp.setOutputGain(0.0f);
    comp.setOverEasy(false);
    comp.setMix(100.0f);

    const double fundamental = 10000.0;
    const float amplitude = 0.5f;
    const int totalSamples = 88200; // 2 seconds

    auto signal = generateSine(kSampleRate, amplitude, fundamental, totalSamples);
    std::vector<float> output(signal.size());

    for (size_t i = 0; i < signal.size(); ++i)
        output[i] = comp.processSample(signal[i]);

    // Analyze the second half (after settling)
    const int analyzeStart = totalSamples / 2;
    const int analyzeLen = totalSamples / 2;

    const double h1 = measureBinAmplitude(output, kSampleRate, fundamental,
                                          analyzeStart, analyzeLen);
    const double h1_dBFS = 20.0 * std::log10(h1);

    // Aliased 3rd harmonic of 10 kHz at 44.1 kHz: lands at 14.1 kHz
    const double alias3f = kSampleRate - 3.0 * fundamental;
    const double alias3Amp = measureBinAmplitude(output, kSampleRate, alias3f,
                                                 analyzeStart, analyzeLen);
    const double alias3_dBFS = (alias3Amp > 1e-15)
                               ? 20.0 * std::log10(alias3Amp)
                               : -200.0;

    std::printf("\n  [Full compressor aliasing @ 44.1 kHz, f=%g Hz, ratio=8:1]\n",
                fundamental);
    std::printf("    Fundamental (%.0f Hz):      %.1f dBFS\n",
                fundamental, h1_dBFS);
    std::printf("    Aliased 3f  (%.0f Hz):      %.1f dBFS\n",
                alias3f, alias3_dBFS);

    // Aliased content should be below -80 dBFS (inaudible)
    REQUIRE(alias3_dBFS < -80.0);
}
