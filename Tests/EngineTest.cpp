// Standalone DSP test for the Avel Ess engine.
//   g++ -O2 -std=c++17 -ISource Tests/EngineTest.cpp -o engine_test && ./engine_test
#include "DSP/Engine.h"
#include <chrono>
#include <cstdio>
#include <random>

using avel::dsp::Engine;
using avel::dsp::Params;

static int failures = 0;
static void check (bool ok, const char* what, double v)
{
    std::printf ("  [%s] %-58s %9.3f\n", ok ? " ok " : "FAIL", what, v);
    if (! ok) ++failures;
}

static constexpr double SR = 48000.0;
static constexpr int BLOCK = 256;

struct Stereo { std::vector<float> l, r; };

static void run (Engine& e, Stereo& s, const Params& p)
{
    const int n = (int) s.l.size();
    for (int i = 0; i < n; i += BLOCK)
    {
        const int len = std::min (BLOCK, n - i);
        float* ptr[2] = { s.l.data() + i, s.r.data() + i };
        e.process (ptr, 2, len, p);
    }
}

static double rms (const std::vector<float>& x, int a, int b)
{
    double s = 0; for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return std::sqrt (s / std::max (1, b - a));
}
static double db (double g) { return 20.0 * std::log10 (std::max (g, 1e-12)); }

// one-pole-pair highpassed noise ≈ an "s"
static std::vector<float> sibilant (int n, float gain, unsigned seed)
{
    std::mt19937 rng (seed); std::normal_distribution<float> nd (0.0f, 1.0f);
    avel::dsp::Biquad a, b; a.set (6000, 0.707, SR, true); b.set (6000, 0.707, SR, true);
    std::vector<float> v ((size_t) n);
    for (auto& x : v) x = b.process (a.process (nd (rng))) * gain;
    return v;
}
// bright vowel: saw-ish harmonic stack at f0, harmonics up to 12 kHz with gentle roll-off
static std::vector<float> vowel (int n, float f0, float gain)
{
    std::vector<float> v ((size_t) n, 0.0f);
    for (int k = 1; k * f0 < 12000; ++k)
    {
        const float amp = 1.0f / (float) k;
        for (int i = 0; i < n; ++i) v[(size_t) i] += amp * std::sin (2.0f * 3.14159265f * f0 * k * i / (float) SR);
    }
    float pk = 0; for (auto x : v) pk = std::max (pk, std::abs (x));
    for (auto& x : v) x *= gain / pk;
    return v;
}
static std::vector<float> sine (int n, float f, float gain)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = gain * std::sin (2.0f * 3.14159265f * f * i / (float) SR);
    return v;
}
// Goertzel magnitude of frequency f over [a,b)
static double tone (const std::vector<float>& x, double f, int a, int b)
{
    const double w = 2 * 3.14159265358979 * f / SR, c = 2 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = a; i < b; ++i) { const double s0 = x[(size_t) i] + c * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / (b - a);
}

int main()
{
    const int N = (int) SR * 2;

    std::printf ("Avel Ess engine test\n");

    // 1. transparency + latency (both processing modes, no reduction)
    for (int mode = 0; mode < 2; ++mode)
    {
        Engine e; e.prepare (SR, BLOCK);
        Params p; p.processing = mode; p.thresholdDb = 0.0f; p.lookaheadMs = 5.0f;
        auto in = sine (N, 440, 0.25f);
        auto in2 = sine (N, 1234, 0.1f); for (int i = 0; i < N; ++i) in[(size_t) i] += in2[(size_t) i];
        Stereo s { in, in };
        run (e, s, p);
        const int lat = e.latencyFor (p.lookaheadMs);
        double err = 0;
        for (int i = lat; i < N; ++i) err = std::max (err, (double) std::abs (s.l[(size_t) i] - in[(size_t) (i - lat)]));
        check (err < 1e-5, mode ? "split band: bit-transparent below threshold (max err)" : "wide band: bit-transparent below threshold (max err)", err);
        if (mode == 0) check (lat == 240 + e.getFirHalf(), "latency = lookahead + FIR centre (samples)", lat);
    }

    // 2. sibilance gets reduced, close to the range
    {
        Engine e; e.prepare (SR, BLOCK);
        Params p; p.thresholdDb = -40.0f; p.rangeDb = 10.0f; p.processing = 0;
        auto s1 = sibilant (N, 0.25f, 1);
        Stereo s { s1, s1 };
        run (e, s, p);
        const int lat = e.latencyFor (p.lookaheadMs);
        const double red = db (rms (s1, N / 2 - lat, N - lat)) - db (rms (s.l, N / 2, N));
        check (red > 8.5 && red < 10.3, "loud 's' reduced by ≈ range (10 dB)", red);
    }

    // 3. Single Vocal ignores a loud bright vowel, Allround reacts more
    double vowelGr[2] {};
    for (int mode = 0; mode < 2; ++mode)
    {
        Engine e; e.prepare (SR, BLOCK);
        Params p; p.mode = mode; p.thresholdDb = -36.0f; p.rangeDb = 12.0f; p.processing = 0;
        auto v = vowel (N, 180, 0.7f);
        Stereo s { v, v };
        run (e, s, p);
        const int lat = e.latencyFor (p.lookaheadMs);
        vowelGr[mode] = db (rms (v, N / 2 - lat, N - lat)) - db (rms (s.l, N / 2, N));
    }
    check (vowelGr[0] < 1.0, "Single Vocal: loud bright vowel, reduction (dB)", vowelGr[0]);
    check (vowelGr[1] > vowelGr[0] + 1.0, "Allround: same vowel triggers more (dB)", vowelGr[1]);

    // 4. "s" over the same vowel is still caught in Single Vocal
    {
        Engine e; e.prepare (SR, BLOCK);
        Params p; p.mode = 0; p.thresholdDb = -30.0f; p.rangeDb = 12.0f; p.processing = 1;
        auto v = vowel (N, 180, 0.3f); auto sib = sibilant (N, 0.2f, 7);
        std::vector<float> mix ((size_t) N);
        for (int i = 0; i < N; ++i) mix[(size_t) i] = v[(size_t) i] + (i > N / 2 ? sib[(size_t) i] : 0.0f);
        Stereo s { mix, mix };
        run (e, s, p);
        check (e.currentGr (0) > 6.0, "Single Vocal: 's' over a vowel still caught (GR dB)", e.currentGr (0));

        // split band leaves the vowel fundamental alone
        const int lat = e.latencyFor (p.lookaheadMs);
        const double a = tone (mix, 180, N * 3 / 4 - lat, N - lat), b = tone (s.l, 180, N * 3 / 4, N);
        check (std::abs (db (b) - db (a)) < 0.1, "split band: 180 Hz fundamental change while reducing (dB)", db (b) - db (a));
    }

    // 5. range cap holds on a huge burst
    {
        Engine e; e.prepare (SR, BLOCK);
        Params p; p.thresholdDb = -60.0f; p.rangeDb = 6.0f;
        auto s1 = sibilant (N, 0.9f, 3);
        Stereo s { s1, s1 };
        float worst = 0;
        for (int i = 0; i < N; i += BLOCK)
        {
            float* ptr[2] = { s.l.data() + i, s.r.data() + i };
            e.process (ptr, 2, BLOCK, p);
            for (auto& f : e.getFrames()) worst = std::max (worst, f.grDb);
        }
        check (worst <= 6.001f, "gain reduction never exceeds Range (max dB)", worst);
    }

    // 6. Mid mode leaves a pure side signal untouched; stereo link 0 keeps L independent
    {
        Engine e; e.prepare (SR, BLOCK);
        Params p; p.channels = 1; p.thresholdDb = -50.0f; p.rangeDb = 12.0f;
        auto s1 = sibilant (N, 0.3f, 5);
        std::vector<float> neg (s1); for (auto& x : neg) x = -x;
        Stereo s { s1, neg };
        run (e, s, p);
        const int lat = e.latencyFor (p.lookaheadMs);
        const double d = db (rms (s.l, N / 2, N)) - db (rms (s1, N / 2 - lat, N - lat));
        check (std::abs (d) < 0.05, "Mid mode: side-only 's' untouched (dB change)", d);
    }
    {
        Engine e; e.prepare (SR, BLOCK);
        Params p; p.link = 0.0f; p.thresholdDb = -40.0f; p.rangeDb = 12.0f; p.processing = 0;
        auto s1 = sibilant (N, 0.3f, 9); auto quiet = sine (N, 300, 0.2f);
        Stereo s { quiet, s1 };
        run (e, s, p);
        check (e.currentGr (0) < 0.01 && e.currentGr (1) > 6, "link 0 %: left (no 's') stays at 0 dB GR", e.currentGr (0));
        Engine e2; e2.prepare (SR, BLOCK); p.link = 1.0f;
        Stereo s2 { quiet, s1 };
        run (e2, s2, p);
        check (std::abs (e2.currentGr (0) - e2.currentGr (1)) < 0.01, "link 100 %: both sides share reduction (diff dB)", e2.currentGr (0) - e2.currentGr (1));
    }

    // 7. lookahead: reduction is in place when the "s" arrives
    {
        Engine e; e.prepare (SR, 64);
        Params p; p.thresholdDb = -40.0f; p.rangeDb = 12.0f; p.processing = 0; p.lookaheadMs = 5.0f;
        std::vector<float> x ((size_t) N, 0.0f);
        auto sib = sibilant (N, 0.3f, 11);
        const int onset = N / 2;
        for (int i = onset; i < N; ++i) x[(size_t) i] = sib[(size_t) i];
        Stereo s { x, x };
        std::vector<float> grAt ((size_t) N);
        for (int i = 0; i < N; i += 64)
        {
            float* ptr[2] = { s.l.data() + i, s.r.data() + i };
            e.process (ptr, 2, 64, p);
            for (int k = 0; k < 64; ++k) grAt[(size_t) (i + k)] = e.currentGr (0);
        }
        const int lat = e.latencyFor (p.lookaheadMs);
        // the onset reaches the output at onset + lat; GR at that moment:
        const int idx = ((onset + lat) / 64) * 64 + 63;
        check (grAt[(size_t) idx] > 8.0, "lookahead 5 ms: GR already applied at the onset (dB)", grAt[(size_t) idx]);
    }

    // 8. stability + CPU
    {
        Engine e; e.prepare (SR, 512);
        Params p; p.thresholdDb = -40; p.rangeDb = 24; p.processing = 1; p.lookaheadMs = 15;
        const int len = (int) SR * 20;
        auto v = vowel (len, 220, 0.4f); auto sib = sibilant (len, 0.5f, 13);
        Stereo s { v, sib };
        bool finite = true;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i + 512 <= len; i += 512)
        {
            p.lowHz = 3000.0f + 4000.0f * (float) std::sin (i * 1e-5);  // sweep while processing
            float* ptr[2] = { s.l.data() + i, s.r.data() + i };
            e.process (ptr, 2, 512, p);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        for (auto x : s.l) finite &= std::isfinite (x) && std::abs (x) < 4.0f;
        for (auto x : s.r) finite &= std::isfinite (x) && std::abs (x) < 4.0f;
        check (finite, "20 s stress with band sweep: output finite and bounded", finite);
        const double cpu = std::chrono::duration<double> (t1 - t0).count() / 20.0 * 100.0;
        check (cpu < 10.0, "CPU, split band, stereo, 48 kHz (% of one core)", cpu);
    }

    std::printf (failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
