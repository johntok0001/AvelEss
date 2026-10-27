#pragma once

// Avel Ess — de-esser engine. Header-only, no JUCE dependency, so it can be tested standalone.
//
// Signal flow (per processed channel):
//   input ─┬─────────────────────────────── ring buffer ─► delayed by lookahead (+ FIR centre)
//          │                                                   │
//          └► detection band (IIR HP4 / LP2) ─► envelope ─►    │
//             full-band envelope (Single Vocal only) ─► gain computer ─► smoothed gain
//                                                              │
//   Wide band : out = d * g
//   Split band: out = d + (g - 1) * bp      (bp = linear-phase FIR band of the delayed input,
//                                            so g = 1 is bit-transparent and the split never colours)
//   Audition  : out = bp                    (listen to the band being processed)

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace avel::dsp
{
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void set (double f, double q, double sr, bool highpass)
        {
            f = std::clamp (f, 10.0, sr * 0.49);
            const double w = 2.0 * 3.14159265358979323846 * f / sr, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
            const double a0 = 1.0 + al;
            if (highpass) { b0 = (1.0 + cw) * 0.5; b1 = -(1.0 + cw); b2 = b0; }
            else          { b0 = (1.0 - cw) * 0.5; b1 = 1.0 - cw;    b2 = b0; }
            b0 /= a0; b1 /= a0; b2 /= a0; a1 = -2.0 * cw / a0; a2 = (1.0 - al) / a0;
        }
        inline float process (float x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
        void reset() { z1 = z2 = 0; }
    };

    struct Params
    {
        int   mode        = 0;       // 0 Single Vocal, 1 Allround
        float thresholdDb = -26.0f;
        float rangeDb     = 9.0f;    // maximum reduction
        float releaseMs   = 40.0f;
        float lookaheadMs = 5.0f;
        float link        = 1.0f;    // 0..1
        int   processing  = 1;       // 0 Wide band, 1 Split band
        int   channels    = 0;       // 0 Stereo, 1 Mid, 2 Side
        float lowHz       = 5500.0f;
        float highHz      = 20000.0f;
        bool  audition    = false;
        float inputDb     = 0.0f;
        float outputDb    = 0.0f;
    };

    struct Frame { float inDb, detDb, grDb, outDb; };

    class Engine
    {
    public:
        static constexpr float maxLookaheadMs = 15.0f;
        static constexpr float frameMs = 5.0f;

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate;
            numTaps = (int) std::lround (255.0 * sr / 48000.0) | 1;
            numTaps = std::clamp (numTaps, 127, 511);
            firHalf = (numTaps - 1) / 2;
            maxLook = (int) std::ceil (maxLookaheadMs * 0.001 * sr);

            int size = 1;
            while (size < maxLook + numTaps + 8) size <<= 1;
            ringSize = size; ringMask = size - 1;

            for (auto& c : ch)
            {
                c.ring.assign ((size_t) ringSize * 2, 0.0f);
                c.reset();
            }

            h.assign ((size_t) numTaps, 0.0f);
            kaiser.resize ((size_t) numTaps);
            const double beta = 8.0, i0b = bessel0 (beta);
            for (int n = 0; n < numTaps; ++n)
            {
                const double r = 2.0 * n / (numTaps - 1) - 1.0;
                kaiser[(size_t) n] = (float) (bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b);
            }

            frameLen = std::max (16, (int) std::lround (frameMs * 0.001 * sr));
            frames.clear();
            frames.reserve ((size_t) (maxBlock / frameLen + 4));
            frameCount = 0; fIn = fOut = fGr = 0.0f; fDet = -120.0f;

            curLow = curHigh = -1.0;
            curIn = curOut = -1.0f;
            writePos = 0;
        }

        void reset()
        {
            for (auto& c : ch) { std::fill (c.ring.begin(), c.ring.end(), 0.0f); c.reset(); }
            writePos = 0;
        }

        int lookaheadSamples (float ms) const { return std::clamp ((int) std::lround (ms * 0.001 * sr), 0, maxLook); }
        int latencyFor (float lookaheadMs) const { return lookaheadSamples (lookaheadMs) + firHalf; }
        int getFirHalf() const { return firHalf; }

        // Most recent gain reduction per slot (dB, positive) — handy for tests.
        float currentGr (int slot) const { return ch[(size_t) slot].gr; }

        const std::vector<Frame>& getFrames() const { return frames; }

        void process (float* const* io, int numCh, int n, const Params& p)
        {
            frames.clear();
            numCh = std::clamp (numCh, 1, 2);
            updateFilters (p);

            const int   L        = lookaheadSamples (p.lookaheadMs);
            const bool  split    = p.processing == 1;
            const bool  needBand = split || p.audition;
            const bool  ms       = numCh == 2 && p.channels != 0;
            const bool  proc0    = ! ms || p.channels == 1;
            const bool  proc1    = numCh == 2 && (! ms || p.channels == 2);
            const bool  single   = p.mode == 0;
            const float thr      = p.thresholdDb;
            const float range    = std::max (0.0f, p.rangeDb);
            const float knee     = single ? 4.0f : 8.0f;
            const float link     = std::clamp (p.link, 0.0f, 1.0f);

            const float envAtt = coef (2.0);
            const float envRel = coef (12.0);
            const float balAtt = coef (12.0);   // symmetric = true average
            const float balRel = coef (12.0);
            const float gAtt   = coef (std::max (0.05, (double) L / sr * 1000.0 / 3.0));
            const float gRel   = coef (std::max (5.0f, p.releaseMs));

            const float tIn  = dbToGain (p.inputDb), tOut = dbToGain (p.outputDb);
            if (curIn  < 0) curIn  = tIn;
            if (curOut < 0) curOut = tOut;
            const float dIn  = (tIn  - curIn)  / (float) std::max (1, n);
            const float dOut = (tOut - curOut) / (float) std::max (1, n);

            const int offset = L + numTaps - 1;   // distance from write position back to first FIR tap

            for (int i = 0; i < n; ++i)
            {
                curIn += dIn; curOut += dOut;

                float a[2] = { io[0][i] * curIn, numCh > 1 ? io[1][i] * curIn : 0.0f };
                const float inPk = std::max (std::abs (a[0]), std::abs (a[1]));
                if (ms) { const float m = 0.5f * (a[0] + a[1]), s = 0.5f * (a[0] - a[1]); a[0] = m; a[1] = s; }

                float target[2] = { 0.0f, 0.0f };
                float detMax = -120.0f;
                const bool processed[2] = { proc0, proc1 };

                for (int c = 0; c < numCh; ++c)
                {
                    if (! processed[c]) continue;
                    auto& s = ch[(size_t) c];
                    float b = s.hp1.process (a[c]);
                    b = s.hp2.process (b);
                    if (lpActive) b = s.lp.process (b);

                    s.envBand = follow (s.envBand, b * b, envAtt, envRel);       // short-term RMS
                    float det = powToDb (s.envBand);
                    if (single)
                    {
                        // spectral balance on slower envelopes, so glottal pulses don't read as sibilance
                        s.balBand = follow (s.balBand, b * b, balAtt, balRel);
                        s.envFull = follow (s.envFull, a[c] * a[c], balAtt, balRel);
                        const float rel = powToDb (s.balBand) - powToDb (s.envFull);         // ≈ 0 when the band dominates (an "s")
                        det -= std::clamp (1.5f * (-rel - 8.0f), 0.0f, 40.0f); // vowels with bright overtones are ignored
                    }
                    detMax = std::max (detMax, det);

                    const float over = det - thr;
                    float gr = 0.0f;
                    if (over > knee * 0.5f)       gr = over;
                    else if (over > -knee * 0.5f) gr = (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
                    target[c] = std::min (gr, range);
                }

                if (numCh == 2 && proc0 && proc1 && link > 0.0f)
                {
                    const float mx = std::max (target[0], target[1]);
                    target[0] += link * (mx - target[0]);
                    target[1] += link * (mx - target[1]);
                }

                float y[2] = { 0.0f, 0.0f };
                float grMax = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    auto& s = ch[(size_t) c];
                    s.ring[(size_t) writePos] = a[c];
                    s.ring[(size_t) (writePos + ringSize)] = a[c];

                    const int p0 = (writePos - offset) & ringMask;
                    const float* x = s.ring.data() + p0;
                    const float d = x[firHalf];

                    float g = 1.0f;
                    if (processed[c])
                    {
                        s.gr = target[c] > s.gr ? target[c] + (s.gr - target[c]) * (1.0f - gAtt)
                                                : target[c] + (s.gr - target[c]) * (1.0f - gRel);
                        if (s.gr < 1.0e-6f) s.gr = 0.0f;
                        g = dbToGain (-s.gr);
                        grMax = std::max (grMax, s.gr);
                    }

                    if (needBand)
                    {
                        float bp = 0.0f;
                        const float* hh = h.data();
                        for (int k = 0; k < numTaps; ++k) bp += hh[k] * x[k];
                        if (p.audition)  y[c] = processed[c] ? bp : 0.0f;
                        else             y[c] = d + (g - 1.0f) * bp;
                    }
                    else
                    {
                        y[c] = d * g;
                    }
                }

                if (ms) { const float l = y[0] + y[1], r = y[0] - y[1]; y[0] = l; y[1] = r; }

                y[0] *= curOut; io[0][i] = y[0];
                if (numCh > 1) { y[1] *= curOut; io[1][i] = y[1]; }

                writePos = (writePos + 1) & ringMask;

                // metering
                fIn  = std::max (fIn, inPk);
                fOut = std::max (fOut, std::max (std::abs (y[0]), std::abs (y[1])));
                fGr  = std::max (fGr, grMax);
                fDet = std::max (fDet, detMax);
                if (++frameCount >= frameLen)
                {
                    frames.push_back ({ gainToDb (fIn), fDet, fGr, gainToDb (fOut) });
                    frameCount = 0; fIn = fOut = fGr = 0.0f; fDet = -120.0f;
                }
            }

            curIn = tIn; curOut = tOut;
        }

    private:
        struct Channel
        {
            std::vector<float> ring;
            Biquad hp1, hp2, lp;
            float envBand = 0, balBand = 0, envFull = 0, gr = 0;
            void reset() { hp1.reset(); hp2.reset(); lp.reset(); envBand = balBand = envFull = gr = 0; }
        };

        static double bessel0 (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
            return sum;
        }
        static inline float dbToGain (float db) noexcept { return std::exp (db * 0.11512925465f); }
        static inline float gainToDb (float g) noexcept  { return 8.68588963807f * std::log (std::max (g, 1.0e-6f)); }
        static inline float powToDb (float p) noexcept   { return 4.34294481903f * std::log (std::max (p, 1.0e-12f)); }
        static inline float follow (float env, float x, float att, float rel) noexcept
        {
            return env + (x - env) * (x > env ? att : rel);
        }
        float coef (double ms) const { return (float) (1.0 - std::exp (-1.0 / (std::max (0.001, ms) * 0.001 * sr))); }

        void updateFilters (const Params& p)
        {
            double lo = std::clamp ((double) p.lowHz, 500.0, sr * 0.42);
            double hi = std::clamp ((double) p.highHz, lo * 1.25, sr * 0.5);

            // glide frequencies per block so dragging the band never clicks
            if (curLow < 0) { curLow = lo; curHigh = hi; designFir (lo, hi); setIir (lo, hi); return; }
            const double k = 0.35;
            const double nl = curLow  * std::pow (lo / curLow,  k);
            const double nh = curHigh * std::pow (hi / curHigh, k);
            const bool changed = std::abs (nl / curLow - 1.0) > 1.0e-4 || std::abs (nh / curHigh - 1.0) > 1.0e-4;
            curLow  = std::abs (lo / nl - 1.0) < 1.0e-4 ? lo : nl;
            curHigh = std::abs (hi / nh - 1.0) < 1.0e-4 ? hi : nh;
            if (changed) { designFir (curLow, curHigh); setIir (curLow, curHigh); }
        }

        void setIir (double lo, double hi)
        {
            lpActive = hi < sr * 0.45 && hi < 19500.0;
            for (auto& c : ch)
            {
                c.hp1.set (lo, 0.54119610, sr, true);
                c.hp2.set (lo, 1.30656296, sr, true);
                if (lpActive) c.lp.set (hi, 0.70710678, sr, false);
            }
        }

        void designFir (double lo, double hi)
        {
            const bool lp = hi < sr * 0.45 && hi < 19500.0;
            const double fl = lo / sr, fh = hi / sr, pi = 3.14159265358979323846;
            auto sinc = [pi] (double fc, int m) { return m == 0 ? 2.0 * fc : std::sin (2.0 * pi * fc * m) / (pi * m); };
            for (int n = 0; n < numTaps; ++n)
            {
                const int m = n - firHalf;
                const double v = (lp ? sinc (fh, m) : (m == 0 ? 1.0 : 0.0)) - sinc (fl, m);
                h[(size_t) n] = (float) (v * kaiser[(size_t) n]);
            }
        }

        double sr = 48000.0;
        int numTaps = 255, firHalf = 127, maxLook = 720, ringSize = 2048, ringMask = 2047, writePos = 0;
        std::vector<float> h, kaiser;
        Channel ch[2];
        bool lpActive = false;
        double curLow = -1, curHigh = -1;
        float curIn = -1, curOut = -1;

        int frameLen = 240, frameCount = 0;
        float fIn = 0, fOut = 0, fGr = 0, fDet = -120;
        std::vector<Frame> frames;
    };
}
