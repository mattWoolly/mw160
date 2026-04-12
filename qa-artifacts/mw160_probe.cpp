// QA-DSP probes for mw160. Compiled standalone and linked against libMW160_DSP.a.
//
// This program is intentionally chatty: it prints measurements rather than
// asserts, so a human can read the output and decide which gaps to file.

#include "dsp/Compressor.h"
#include "dsp/RmsDetector.h"
#include "dsp/Ballistics.h"
#include "dsp/VcaSaturation.h"
#include "dsp/GainComputer.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <limits>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine(double sampleRate, float amp, float freq, int n,
                        double startPhase = 0.0)
{
    std::vector<float> v(n);
    const double w = 2.0 * kPi * freq / sampleRate;
    for (int i = 0; i < n; ++i)
        v[i] = amp * static_cast<float>(std::sin(w * i + startPhase));
    return v;
}

double rms(const std::vector<float>& v, int s, int n)
{
    double a = 0.0;
    for (int i = s; i < s + n; ++i) a += double(v[i]) * double(v[i]);
    return std::sqrt(a / n);
}

double dft_bin(const std::vector<float>& v, double sr, double f, int s, int n)
{
    double sn = 0.0, cs = 0.0;
    const double w = 2.0 * kPi * f / sr;
    for (int i = 0; i < n; ++i) {
        const double x = v[s + i];
        const double ph = w * (s + i);
        sn += x * std::sin(ph);
        cs += x * std::cos(ph);
    }
    sn *= 2.0 / n;
    cs *= 2.0 / n;
    return std::sqrt(sn * sn + cs * cs);
}

void hr(const char* title)
{
    std::printf("\n========== %s ==========\n", title);
}

// ---------------------------------------------------------------------------
// 1. Static GR curve across ratios — sine input, full chain.
// ---------------------------------------------------------------------------
void probe_static_gr_curve()
{
    hr("Static GR curve (sine, full Compressor)");
    constexpr double sr = 48000.0;
    const float thresh = -20.0f;
    const float ratios[] = {2.0f, 4.0f, 8.0f, 60.0f}; // 60 == limiter
    const float excessTested[] = {3.0f, 6.0f, 10.0f, 20.0f, 30.0f};

    std::printf("threshold = %.1f dBFS\n", thresh);
    std::printf("%-8s %-8s %-12s %-12s %-12s\n",
                "ratio", "excess", "expectedGR", "measuredGR", "delta");

    for (float ratio : ratios) {
        for (float excess : excessTested) {
            mw160::Compressor c;
            c.prepare(sr, 512);
            c.setThreshold(thresh);
            c.setRatio(ratio);
            c.setOutputGain(0.0f);
            c.setSoftKnee(false);
            c.setMix(100.0f);

            const float rmsTarget_dB = thresh + excess;
            const float amp = std::pow(10.0f, rmsTarget_dB / 20.0f) * std::sqrt(2.0f);

            const int N = int(sr * 2.0); // 2 s settle
            auto in = sine(sr, amp, 1000.0f, N);
            std::vector<float> out(N);
            for (int i = 0; i < N; ++i)
                out[i] = c.processSample(in[i]);

            const int mlen = int(sr * 0.2);
            const int mstart = N - mlen;
            const double r_in = rms(in, mstart, mlen);
            const double r_out = rms(out, mstart, mlen);
            const double measGR = 20.0 * std::log10(r_out / r_in);

            float expGR;
            if (ratio <= 1.0f) expGR = 0.0f;
            else expGR = excess * (1.0f / ratio - 1.0f);

            std::printf("%-8.1f %-8.1f %-12.3f %-12.3f %-+12.3f\n",
                        ratio, excess, expGR, measGR, measGR - expGR);
        }
    }
}

// ---------------------------------------------------------------------------
// 2. Knee smoothness — sweep input around threshold and look at derivative.
// ---------------------------------------------------------------------------
void probe_knee_smoothness()
{
    hr("Knee smoothness (gain computer direct)");
    const mw160::GainComputer gc;
    const float thresh = 0.0f;
    const float ratio = 4.0f;

    auto sweep = [&](float kneeWidth, const char* label) {
        std::printf("\n[%s] knee width = %.1f dB\n", label, kneeWidth);
        std::printf("%-12s %-12s %-12s\n", "input(dB)", "GR(dB)", "dGR/dx");
        float prev = gc.computeGainReduction(-12.0f, thresh, ratio, kneeWidth);
        float maxJump = 0.0f, maxJumpAt = -100.0f;
        for (float x = -12.0f; x <= 12.0f; x += 0.25f) {
            float g = gc.computeGainReduction(x, thresh, ratio, kneeWidth);
            float d = (g - prev) / 0.25f;
            if (std::fabs(d) > std::fabs(maxJump)) { maxJump = d; maxJumpAt = x; }
            // Print only every 1 dB to keep output manageable
            if (std::fmod(std::round(x*4)/4, 1.0f) == 0.0f)
                std::printf("%-12.2f %-12.4f %-12.4f\n", x, g, d);
            prev = g;
        }
        std::printf("max |dGR/dx| = %.4f at x=%.2f dB\n", std::fabs(maxJump), maxJumpAt);
    };

    sweep(0.0f, "hard knee");
    sweep(10.0f, "soft knee (10 dB)");

    // Specifically test continuity of derivative for soft knee at kneeBottom and kneeTop
    std::printf("\n[derivative continuity at knee boundaries, soft knee 10 dB, ratio 4:1]\n");
    const float kw = 10.0f;
    const float ds = 0.001f;
    auto deriv = [&](float x) {
        return (gc.computeGainReduction(x + ds, thresh, ratio, kw) -
                gc.computeGainReduction(x - ds, thresh, ratio, kw)) / (2 * ds);
    };
    std::printf("d/dx GR @ kneeBottom -0.01 (just outside): %.4f\n", deriv(-5.0f - 0.01f));
    std::printf("d/dx GR @ kneeBottom +0.01 (just inside):  %.4f\n", deriv(-5.0f + 0.01f));
    std::printf("d/dx GR @ kneeTop    -0.01 (just inside):  %.4f\n", deriv(5.0f - 0.01f));
    std::printf("d/dx GR @ kneeTop    +0.01 (just outside): %.4f\n", deriv(5.0f + 0.01f));
    std::printf("(linear region slope expected = 1/R - 1 = %.4f)\n", 1.0f/ratio - 1.0f);
}

// ---------------------------------------------------------------------------
// 3. Block-size independence — sample-by-sample compressor doesn't have
//    blocks, but verify that processSample called in any chunking gives
//    bit-identical output. Test by running the SAME sequence with different
//    "block sizes" simulated by also calling parameter setters between blocks.
// ---------------------------------------------------------------------------
void probe_block_size_independence()
{
    hr("Block-size independence (no parameter changes)");
    constexpr double sr = 48000.0;
    const int N = int(sr * 1.0); // 1 second
    auto input = sine(sr, 0.5f, 1000.0f, N);

    auto runOnce = [&](int blockSize) {
        mw160::Compressor c;
        c.prepare(sr, blockSize);
        c.setThreshold(-20.0f);
        c.setRatio(4.0f);
        c.setOutputGain(0.0f);
        c.setSoftKnee(false);
        c.setMix(100.0f);
        std::vector<float> out(N);
        for (int i = 0; i < N; ++i)
            out[i] = c.processSample(input[i]);
        return out;
    };

    auto ref = runOnce(512);
    int blocks[] = {32, 64, 128, 256, 512, 1024, 2048};
    for (int b : blocks) {
        auto out = runOnce(b);
        double maxDiff = 0.0;
        int maxAt = -1;
        for (int i = 0; i < N; ++i) {
            double d = std::fabs(out[i] - ref[i]);
            if (d > maxDiff) { maxDiff = d; maxAt = i; }
        }
        std::printf("block=%-5d  max |out-ref|=%.3e at sample %d\n", b, maxDiff, maxAt);
    }
}

// As a separate test, exercise processBlock-style chunking AT THE SAMPLE LEVEL,
// to detect any state that depends on call ordering.
void probe_block_size_independence_chunked()
{
    hr("Block-size independence (chunked but identical params)");
    constexpr double sr = 48000.0;
    const int N = int(sr * 1.0);
    auto input = sine(sr, 0.5f, 1000.0f, N);

    auto runChunked = [&](int blockSize) {
        mw160::Compressor c;
        c.prepare(sr, blockSize);
        c.setThreshold(-20.0f);
        c.setRatio(4.0f);
        c.setOutputGain(0.0f);
        c.setSoftKnee(false);
        c.setMix(100.0f);
        std::vector<float> out(N);
        int i = 0;
        while (i < N) {
            int n = std::min(blockSize, N - i);
            // Mimic processBlock: call setters again per "block"
            c.setThreshold(-20.0f);
            c.setRatio(4.0f);
            c.setOutputGain(0.0f);
            c.setSoftKnee(false);
            c.setMix(100.0f);
            for (int k = 0; k < n; ++k, ++i)
                out[i] = c.processSample(input[i]);
        }
        return out;
    };

    // Reference: huge block (no per-block setter retriggers; ramp completes once)
    auto ref = runChunked(N);
    int blocks[] = {32, 64, 128, 256, 512, 1024, 2048};
    for (int b : blocks) {
        auto out = runChunked(b);
        double maxDiff = 0.0;
        int maxAt = -1;
        for (int i = 0; i < N; ++i) {
            double d = std::fabs(out[i] - ref[i]);
            if (d > maxDiff) { maxDiff = d; maxAt = i; }
        }
        std::printf("chunked block=%-5d  max |out-ref|=%.3e at sample %d\n", b, maxDiff, maxAt);
    }
}

// ---------------------------------------------------------------------------
// 4. Sample-rate independence of detector tau and release rate.
// ---------------------------------------------------------------------------
void probe_sample_rate_independence()
{
    hr("Sample-rate independence (RmsDetector tau & Ballistics release rate)");
    const double rates[] = {44100.0, 48000.0, 96000.0, 192000.0};

    // RMS detector: time to reach 0.795 from a step (~20 ms target)
    for (double sr : rates) {
        mw160::RmsDetector det;
        det.prepare(sr);
        det.reset();
        const int maxN = int(sr * 0.2);
        int sCross = -1;
        for (int i = 0; i < maxN; ++i) {
            float r = det.processSample(1.0f);
            if (sCross < 0 && r >= 0.7951f) { sCross = i; break; }
        }
        std::printf("RMS detector: sr=%.0f  samples to ~0.795 = %d  (%.3f ms)\n",
                    sr, sCross, (sCross / sr) * 1000.0);
    }

    // Ballistics release rate: drive to -20 dB then measure rate
    for (double sr : rates) {
        mw160::Ballistics b;
        b.prepare(sr);
        for (int i = 0; i < int(sr * 0.1); ++i) b.processSample(-20.0f);
        // Now release; sample at 100 samples in and 100+10ms in
        float gr = b.processSample(0.0f);
        for (int i = 0; i < 100; ++i) gr = b.processSample(0.0f);
        float a = gr;
        const int win = int(sr * 0.010);
        for (int i = 0; i < win; ++i) gr = b.processSample(0.0f);
        float c = gr;
        double rate = (c - a) / (win / sr);
        std::printf("Ballistics release: sr=%.0f  rate ~ %.2f dB/s\n", sr, rate);
    }
}

// ---------------------------------------------------------------------------
// 5. Denormals / NaN / Inf
// ---------------------------------------------------------------------------
bool isBad(float x)
{
    return std::isnan(x) || std::isinf(x);
}

void probe_denormals_nan()
{
    hr("Denormals / NaN / Inf safety");
    constexpr double sr = 48000.0;
    auto run = [&](const char* tag, const std::vector<float>& in, bool softKnee = false) {
        mw160::Compressor c;
        c.prepare(sr, 512);
        c.setThreshold(-20.0f);
        c.setRatio(4.0f);
        c.setOutputGain(0.0f);
        c.setSoftKnee(softKnee);
        c.setMix(100.0f);
        bool any = false;
        float minOut = std::numeric_limits<float>::infinity();
        float maxOut = -std::numeric_limits<float>::infinity();
        for (size_t i = 0; i < in.size(); ++i) {
            float y = c.processSample(in[i]);
            if (isBad(y)) any = true;
            if (y < minOut) minOut = y;
            if (y > maxOut) maxOut = y;
        }
        std::printf("%-32s  bad=%d  range=[%.3e, %.3e]\n",
                    tag, int(any), minOut, maxOut);
    };

    // 1) Long silence (denormal trap)
    {
        std::vector<float> in(int(sr * 1.0), 0.0f);
        run("silence (1 s)", in);
    }
    // 2) Silence -> 0 dBFS step -> silence
    {
        std::vector<float> in(int(sr * 1.0), 0.0f);
        for (int i = int(sr * 0.3); i < int(sr * 0.6); ++i) in[i] = 0.99f;
        run("silence -> step 0.99 -> silence", in);
    }
    // 3) DC at 0.5
    {
        std::vector<float> in(int(sr * 0.5), 0.5f);
        run("DC at 0.5 (500 ms)", in);
    }
    // 4) Subsonic 10 Hz tone
    {
        auto in = sine(sr, 0.5f, 10.0f, int(sr * 0.5));
        run("10 Hz tone (subsonic)", in);
    }
    // 5) Tiny denormal-magnitude sine
    {
        auto in = sine(sr, 1e-30f, 1000.0f, int(sr * 0.5));
        run("1e-30 sine", in);
    }
    // 6) NaN injection (should not propagate forever)
    {
        std::vector<float> in(int(sr * 0.2), 0.5f);
        in[100] = std::numeric_limits<float>::quiet_NaN();
        // Custom run to count NaN/Inf in OUTPUT to detect persistent poisoning
        mw160::Compressor c;
        c.prepare(sr, 512);
        c.setThreshold(-20.0f);
        c.setRatio(4.0f);
        int nanCount = 0;
        for (size_t i = 0; i < in.size(); ++i) {
            float y = c.processSample(in[i]);
            if (isBad(y)) ++nanCount;
        }
        std::printf("NaN injected at idx 100: output NaN/Inf count = %d / %zu samples\n",
                    nanCount, in.size());
        std::printf("(persistent if count > 1; transient if == 1)\n");
        // Test that detector RECOVERS after a NaN. Drive with loud sustained signal
        // afterwards and verify GR is applied.
        mw160::Compressor c2;
        c2.prepare(sr, 512);
        c2.setThreshold(-20.0f);
        c2.setRatio(4.0f);
        // Pre-warm
        for (int i = 0; i < int(sr * 0.1); ++i) c2.processSample(0.0f);
        // Inject NaN
        c2.processSample(std::numeric_limits<float>::quiet_NaN());
        // Now feed sustained loud signal at 0 dBFS RMS (should cause ~15 dB GR)
        const int Nrec = int(sr * 1.0);
        auto loud = sine(sr, std::sqrt(2.0f), 1000.0f, Nrec);
        std::vector<float> out(Nrec);
        for (int i = 0; i < Nrec; ++i) out[i] = c2.processSample(loud[i]);
        // Measure RMS in last 100 ms
        const int mlen = int(sr * 0.1);
        double r = rms(out, Nrec - mlen, mlen);
        double in_r = rms(loud, Nrec - mlen, mlen);
        std::printf("NaN-recovery test: input RMS = %.3f (%.1f dB), output RMS = %.3f (%.1f dB)\n",
                    in_r, 20*std::log10(in_r), r, 20*std::log10(r));
        std::printf("(if detector survived NaN: should be ~15 dB GR; if poisoned: 0 dB GR)\n");
        std::printf("metered GR after recovery = %.3f dB\n", c2.getLastGainReduction_dB());
    }
    // 7) Inf injection
    {
        std::vector<float> in(int(sr * 0.2), 0.5f);
        in[100] = std::numeric_limits<float>::infinity();
        mw160::Compressor c;
        c.prepare(sr, 512);
        c.setThreshold(-20.0f);
        c.setRatio(4.0f);
        int badCount = 0;
        for (size_t i = 0; i < in.size(); ++i) {
            float y = c.processSample(in[i]);
            if (isBad(y)) ++badCount;
        }
        std::printf("Inf injected at idx 100: output bad count = %d / %zu samples\n",
                    badCount, in.size());
    }
    // 8) Pure DC step from 0 to 1.0 (input level above any threshold)
    {
        std::vector<float> in(int(sr * 0.5), 1.0f);
        run("DC at 1.0", in);
    }
}

// ---------------------------------------------------------------------------
// 6. Zipper noise / parameter automation
// ---------------------------------------------------------------------------
void probe_zipper()
{
    hr("Parameter zipper noise (full compressor)");
    constexpr double sr = 48000.0;
    const int N = int(sr * 4.0);
    auto in = sine(sr, 0.5f, 1000.0f, N);
    std::vector<float> out(N);

    mw160::Compressor c;
    c.prepare(sr, 512);
    c.setThreshold(-10.0f);
    c.setRatio(4.0f);
    c.setOutputGain(0.0f);

    // Sweep threshold linearly from -10 to -40 over 4 seconds
    for (int i = 0; i < N; ++i) {
        const float t = -10.0f - 30.0f * float(i) / float(N);
        c.setThreshold(t);
        // Simultaneously sweep ratio from 2 to 20
        const float r = 2.0f + 18.0f * float(i) / float(N);
        c.setRatio(r);
        // And output gain from 0 to 6 dB and back
        const float g = 6.0f * std::sin(2 * kPi * 0.25f * (i / sr));
        c.setOutputGain(g);
        out[i] = c.processSample(in[i]);
    }

    // Look at sample-to-sample diff after the detector has settled
    double maxStep = 0.0;
    int atIdx = -1;
    for (int i = int(sr * 0.2) + 1; i < N; ++i) {
        double d = std::fabs(double(out[i]) - double(out[i-1]));
        if (d > maxStep) { maxStep = d; atIdx = i; }
    }
    std::printf("max sample-to-sample delta in swept output: %.4e at idx %d (%.3f s)\n",
                maxStep, atIdx, atIdx / sr);

    // Look for any NaN/Inf
    bool any = false;
    for (auto y : out) if (isBad(y)) { any = true; break; }
    std::printf("any NaN/Inf in swept output: %d\n", int(any));
}

// ---------------------------------------------------------------------------
// 7. THD spectrum: 2nd vs 3rd harmonic dominance under GR
// ---------------------------------------------------------------------------
void probe_thd_spectrum()
{
    hr("THD spectrum under GR (full chain)");
    constexpr double sr = 48000.0;
    const float thresh = -20.0f;

    // We want to test 2nd > 3rd harmonic at moderate GR (10-20 dB).
    // Use 100 Hz fundamental so harmonics are well below Nyquist.
    const double fund = 100.0;

    struct Cfg { float ratio; float excess; const char* label; };
    Cfg cfgs[] = {
        { 2.0f,  6.0f,  "2:1 6dB excess (~3 dB GR)" },
        { 4.0f,  20.0f, "4:1 20dB excess (~15 dB GR)" },
        { 8.0f,  20.0f, "8:1 20dB excess (~17.5 dB GR)" },
    };

    for (const auto& cfg : cfgs) {
        mw160::Compressor c;
        c.prepare(sr, 512);
        c.setThreshold(thresh);
        c.setRatio(cfg.ratio);
        c.setOutputGain(0.0f);
        c.setSoftKnee(false);
        c.setMix(100.0f);

        const float rmsTarget_dB = thresh + cfg.excess;
        const float amp = std::pow(10.0f, rmsTarget_dB / 20.0f) * std::sqrt(2.0f);

        const int N = int(sr * 2.0);
        auto in = sine(sr, amp, float(fund), N);
        std::vector<float> out(N);
        for (int i = 0; i < N; ++i) out[i] = c.processSample(in[i]);

        const int astart = int(sr * 1.0);
        const int alen = N - astart;
        const double h1 = dft_bin(out, sr, fund, astart, alen);
        const double h2 = dft_bin(out, sr, 2*fund, astart, alen);
        const double h3 = dft_bin(out, sr, 3*fund, astart, alen);
        const double h4 = dft_bin(out, sr, 4*fund, astart, alen);
        const double h5 = dft_bin(out, sr, 5*fund, astart, alen);

        // THD ≈ sqrt(h2² + h3² + h4² + h5²) / h1
        const double thd = std::sqrt(h2*h2 + h3*h3 + h4*h4 + h5*h5) / h1;

        const double meterGR = c.getLastGainReduction_dB();

        std::printf("\n%s\n", cfg.label);
        std::printf("  metered GR = %.2f dB\n", meterGR);
        std::printf("  h1 = %.6e (fund)\n", h1);
        std::printf("  h2 = %.6e  (%.1f dB rel fund)\n", h2, 20.0*std::log10(h2/h1));
        std::printf("  h3 = %.6e  (%.1f dB rel fund)\n", h3, 20.0*std::log10(h3/h1));
        std::printf("  h4 = %.6e  (%.1f dB rel fund)\n", h4, 20.0*std::log10(h4/h1));
        std::printf("  h5 = %.6e  (%.1f dB rel fund)\n", h5, 20.0*std::log10(h5/h1));
        std::printf("  THD ~ %.4f%%   (h2 > h3? %s)\n", thd * 100.0, h2 > h3 ? "yes" : "NO");
    }
}

// ---------------------------------------------------------------------------
// 8. Verify ballistics 90% targets per REFERENCE.md
// ---------------------------------------------------------------------------
void probe_attack_90pct()
{
    hr("Attack 90% timing (Ballistics direct, multiple sample rates)");
    const double rates[] = {44100.0, 48000.0, 96000.0, 192000.0};
    const float steps[] = {-10.0f, -20.0f, -30.0f};

    auto measure = [](double sr, float target) {
        mw160::Ballistics b;
        b.prepare(sr);
        const float thr = target * 0.9f;
        const int maxN = int(sr * 0.5);
        for (int i = 0; i < maxN; ++i) {
            float gr = b.processSample(target);
            if (gr <= thr) return double(i + 1) / sr * 1000.0;
        }
        return -1.0;
    };

    for (double sr : rates) {
        for (float t : steps) {
            double ms = measure(sr, t);
            std::printf("sr=%.0f  target=%-5.0f dB  -> %.3f ms\n", sr, t, ms);
        }
        std::printf("---\n");
    }
}

// ---------------------------------------------------------------------------
// 9. Stereo link energy semantics
// ---------------------------------------------------------------------------
void probe_stereo_link_l_only()
{
    hr("Stereo-link L-only -> R should still see GR");
    constexpr double sr = 48000.0;
    const int N = int(sr * 1.0);
    const float amp = std::pow(10.0f, -10.0f/20.0f) * std::sqrt(2.0f); // -10 dBFS RMS sine
    auto inL = sine(sr, amp, 1000.0f, N);
    std::vector<float> inR(N, 0.0f);

    mw160::Compressor cL, cR;
    cL.prepare(sr, 512); cR.prepare(sr, 512);
    cL.setThreshold(-20.0f); cR.setThreshold(-20.0f);
    cL.setRatio(4.0f); cR.setRatio(4.0f);
    cL.setMix(100.0f); cR.setMix(100.0f);

    std::vector<float> outL(N), outR(N);
    for (int i = 0; i < N; ++i) {
        const double sqL = double(inL[i]) * double(inL[i]);
        const double sqR = 0.0;
        const double linked = (sqL + sqR) * 0.5;
        outL[i] = cL.processSampleLinked(inL[i], linked);
        outR[i] = cR.processSampleLinked(inR[i], linked);
    }
    // Read cR's metered GR — should be < 0 because R received the linked detector
    std::printf("L metered GR after settle = %.3f dB\n", cL.getLastGainReduction_dB());
    std::printf("R metered GR after settle = %.3f dB\n", cR.getLastGainReduction_dB());
    std::printf("Both should be the same (linked).\n");
}

// ---------------------------------------------------------------------------
// 10. Topology probe: feedforward vs. feedback signature
// In a feedforward compressor, the steady-state GR exactly matches the
// theoretical curve. In a feedback compressor it's slightly soft (slope
// less than nominal because the detector reads the post-VCA signal which
// is already attenuated).
// ---------------------------------------------------------------------------
void probe_topology()
{
    hr("Topology check: feedforward vs feedback signature");
    constexpr double sr = 48000.0;
    const float thresh = -20.0f;
    const float ratio = 4.0f;
    const float excess = 20.0f;

    mw160::Compressor c;
    c.prepare(sr, 512);
    c.setThreshold(thresh);
    c.setRatio(ratio);

    const float amp = std::pow(10.0f, (thresh + excess) / 20.0f) * std::sqrt(2.0f);
    const int N = int(sr * 3.0);
    auto in = sine(sr, amp, 1000.0f, N);
    std::vector<float> out(N);
    for (int i = 0; i < N; ++i) out[i] = c.processSample(in[i]);

    const int mlen = int(sr * 0.5);
    const int mstart = N - mlen;
    const double r_in = rms(in, mstart, mlen);
    const double r_out = rms(out, mstart, mlen);
    const double measGR = 20.0 * std::log10(r_out / r_in);
    const double expGR_FF = excess * (1.0 / ratio - 1.0);
    // For feedback, steady state would solve y_dB = x_dB + (y_dB - thresh)*(1/R - 1)
    // => y_dB - x_dB = (y_dB - thresh)*(1/R - 1)
    // Let y = x + GR. Then GR = (x + GR - thresh)*(1/R - 1)
    // GR(1 - (1/R - 1)) = (x - thresh)*(1/R - 1)
    // GR * (2 - 1/R) = excess * (1/R - 1)
    // GR_FB = excess * (1/R - 1) / (2 - 1/R)
    const double expGR_FB = excess * (1.0/ratio - 1.0) / (2.0 - 1.0/ratio);
    std::printf("excess=%.1f dB ratio=%.1f:1\n", excess, ratio);
    std::printf("measured GR (sr=%.0f): %.3f dB\n", sr, measGR);
    std::printf("expected if feedforward: %.3f dB\n", expGR_FF);
    std::printf("expected if feedback:    %.3f dB\n", expGR_FB);
    std::printf("delta vs feedforward: %+.3f dB\n", measGR - expGR_FF);
    std::printf("delta vs feedback:    %+.3f dB\n", measGR - expGR_FB);
}

// ---------------------------------------------------------------------------
// 11. Output gain smoothing convention check
// REFERENCE.md §1.7 says output gain should be smoothed in a 20 ms LINEAR
// ramp; the code uses Multiplicative smoothing (constant ratio per sample =
// linear-in-dB). Both prevent zipper, but linear-in-dB is louder near the
// end of a downward ramp than linear-in-amp. Quantify the deviation.
// ---------------------------------------------------------------------------
void probe_output_gain_ramp_shape()
{
    hr("Output gain ramp shape (Linear vs Multiplicative)");
    constexpr double sr = 48000.0;
    mw160::Compressor c;
    c.prepare(sr, 512);
    c.setThreshold(-100.0f); // never compress
    c.setRatio(1.0f);
    c.setOutputGain(0.0f);

    // Pre-warm to settle
    for (int i = 0; i < int(sr * 0.1); ++i) c.processSample(1.0f);
    // Now request a 12 dB jump and capture per-sample gain
    c.setOutputGain(12.0f);
    const int rampSamples = int(sr * 0.020) + 8;
    std::vector<float> trace(rampSamples);
    for (int i = 0; i < rampSamples; ++i) trace[i] = c.processSample(1.0f);

    // Compare against ideal LINEAR-in-amplitude ramp from 1.0 to 10^(12/20)=3.981
    const float gFinal = std::pow(10.0f, 12.0f/20.0f);
    const int ramp = int(sr * 0.020);
    double maxDevLinear = 0.0;
    for (int i = 0; i < ramp && i < rampSamples; ++i) {
        const float frac = float(i + 1) / float(ramp);
        const float ideal = 1.0f + (gFinal - 1.0f) * frac;
        const double dev = std::fabs(trace[i] - ideal);
        if (dev > maxDevLinear) maxDevLinear = dev;
    }
    std::printf("12 dB output-gain ramp: %d samples\n", ramp);
    std::printf("first 8 samples of ramp: ");
    for (int i = 0; i < 8; ++i) std::printf("%.4f ", trace[i]);
    std::printf("\n");
    std::printf("last 4 samples (idx %d..%d): ", ramp-4, ramp-1);
    for (int i = ramp-4; i < ramp; ++i) std::printf("%.4f ", trace[i]);
    std::printf("\n");
    std::printf("ideal final value: %.4f\n", gFinal);
    std::printf("max abs deviation from linear-in-amp ramp: %.4f\n", maxDevLinear);
}

} // anonymous namespace

int main(int argc, char** argv)
{
    std::printf("mw160 QA-DSP probe — measurements only, no asserts\n");

    probe_static_gr_curve();
    probe_knee_smoothness();
    probe_block_size_independence();
    probe_block_size_independence_chunked();
    probe_sample_rate_independence();
    probe_denormals_nan();
    probe_zipper();
    probe_thd_spectrum();
    probe_attack_90pct();
    probe_stereo_link_l_only();
    probe_topology();
    probe_output_gain_ramp_shape();

    std::printf("\n[done]\n");
    return 0;
}
