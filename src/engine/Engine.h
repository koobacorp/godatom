#pragma once
// GOD ATOM — original mix-bus / master-chain DSP. No third-party algorithm code.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

namespace ga {

inline constexpr float kCeilingDb = -0.1f;
inline constexpr int   kLookaheadHost = 64;
inline constexpr float kPi = 3.14159265358979323846f;

inline float dbToLin(float db) { return std::pow(10.0f, db / 20.0f); }
inline float linToDb(float lin) { return 20.0f * std::log10(std::max(lin, 1.0e-12f)); }
inline float ceilingLin() { return dbToLin(kCeilingDb); }
inline float clampf(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }
inline float flush(float v) {
    if (!std::isfinite(v)) return 0.0f;
    if (v > 0 && v < 1.0e-20f) return 0.0f;
    if (v < 0 && v > -1.0e-20f) return 0.0f;
    return v;
}

// ---- Parameters ----------------------------------------------------------
struct Params {
    float inputDb  = 0.0f;     // -12 .. +12
    float outputDb = 0.0f;     // -12 .. +12
    float eqHighDb = 0.0f;     // -6 .. +6
    float eqMidDb  = 0.0f;
    float eqLowDb  = 0.0f;
    float amount   = 100.0f;   // 0 .. 200
    float limiterDb = 5.0f;    // 0 .. 12
    bool  eqOn         = true;
    bool  characterOn  = true;
    bool  limiterOn    = true;
    bool  bypass       = false;
    bool  extSidechain = false;
    int   oversample   = 1;    // 1, 2, 4, 8
    int   meterMode    = 0;    // 0 = RMS+Peak, 1 = short-term LUFS+Peak
    bool  animation    = true;
    bool  autoGain     = true;
    // Test / character-stage enables (always true from the UI when character is on)
    bool  houseOn = true;
    bool  compOn  = true;
    bool  tiltOn  = true;
    bool  satOn   = true;
};

struct Meters {
    float inPeakL = -120, inPeakR = -120, inRmsL = -120, inRmsR = -120, inLufs = -120;
    float grHigh = 0, grMid = 0, grLow = 0;
    float limGrL = 0, limGrR = 0;
    float outPeakL = -120, outPeakR = -120, outRmsL = -120, outRmsR = -120, outLufs = -120;
    float inPeakLinL = 0, inPeakLinR = 0, outPeakLinL = 0, outPeakLinR = 0;
};

// ---- RBJ biquads ---------------------------------------------------------
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return flush(y);
    }
    void setCoeffs(float B0, float B1, float B2, float A0, float A1, float A2) {
        float inv = 1.0f / A0;
        b0 = B0 * inv; b1 = B1 * inv; b2 = B2 * inv;
        a1 = A1 * inv; a2 = A2 * inv;
    }
    void identity() { b0 = 1; b1 = b2 = a1 = a2 = 0; reset(); }

    void lowShelf(float sr, float hz, float gainDb, float Q = 0.70710678f) {
        if (std::abs(gainDb) < 1.0e-6f) { identity(); return; }
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w = 2.0f * kPi * hz / sr;
        float c = std::cos(w), s = std::sin(w);
        float alpha = s / (2.0f * Q);
        float twoSqrtAalpha = 2.0f * std::sqrt(A) * alpha;
        setCoeffs(A * ((A + 1) - (A - 1) * c + twoSqrtAalpha),
                  2.0f * A * ((A - 1) - (A + 1) * c),
                  A * ((A + 1) - (A - 1) * c - twoSqrtAalpha),
                  (A + 1) + (A - 1) * c + twoSqrtAalpha,
                  -2.0f * ((A - 1) + (A + 1) * c),
                  (A + 1) + (A - 1) * c - twoSqrtAalpha);
    }
    void highShelf(float sr, float hz, float gainDb, float Q = 0.70710678f) {
        if (std::abs(gainDb) < 1.0e-6f) { identity(); return; }
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w = 2.0f * kPi * hz / sr;
        float c = std::cos(w), s = std::sin(w);
        float alpha = s / (2.0f * Q);
        float twoSqrtAalpha = 2.0f * std::sqrt(A) * alpha;
        setCoeffs(A * ((A + 1) + (A - 1) * c + twoSqrtAalpha),
                  -2.0f * A * ((A - 1) + (A + 1) * c),
                  A * ((A + 1) + (A - 1) * c - twoSqrtAalpha),
                  (A + 1) - (A - 1) * c + twoSqrtAalpha,
                  2.0f * ((A - 1) - (A + 1) * c),
                  (A + 1) - (A - 1) * c - twoSqrtAalpha);
    }
    void peaking(float sr, float hz, float gainDb, float Q) {
        if (std::abs(gainDb) < 1.0e-6f) { identity(); return; }
        float A = std::pow(10.0f, gainDb / 40.0f);
        float w = 2.0f * kPi * hz / sr;
        float c = std::cos(w), s = std::sin(w);
        float alpha = s / (2.0f * Q);
        setCoeffs(1 + alpha * A, -2 * c, 1 - alpha * A,
                  1 + alpha / A, -2 * c, 1 - alpha / A);
    }
    void lowpass(float sr, float hz, float Q) {
        float w = 2.0f * kPi * hz / sr;
        float c = std::cos(w), s = std::sin(w);
        float alpha = s / (2.0f * Q);
        setCoeffs((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f,
                  1 + alpha, -2 * c, 1 - alpha);
    }
    void highpass(float sr, float hz, float Q) {
        float w = 2.0f * kPi * hz / sr;
        float c = std::cos(w), s = std::sin(w);
        float alpha = s / (2.0f * Q);
        setCoeffs((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f,
                  1 + alpha, -2 * c, 1 - alpha);
    }
    void highShelfK(float sr); // ITU stage 1, specialized below if needed
};

// Magnitude of a biquad at frequency hz (for tests / Atom-curve checks)
inline float biquadMagDb(const Biquad& f, float sr, float hz) {
    float w = 2.0f * kPi * hz / sr;
    float cw = std::cos(w), sw = std::sin(w);
    float c2 = std::cos(2 * w), s2 = std::sin(2 * w);
    float numR = f.b0 + f.b1 * cw + f.b2 * c2;
    float numI = -f.b1 * sw - f.b2 * s2;
    float denR = 1 + f.a1 * cw + f.a2 * c2;
    float denI = -f.a1 * sw - f.a2 * s2;
    float mag = std::sqrt((numR * numR + numI * numI) / std::max(1e-20f, denR * denR + denI * denI));
    return linToDb(mag);
}

// Textbook soft-knee compressor: returns output minus input, in dB (≤ 0).
inline float softKneeGrDb(float xDb, float thresh, float ratio, float knee) {
    float half = knee * 0.5f;
    if (xDb <= thresh - half) return 0.0f;
    if (xDb >= thresh + half) {
        float y = thresh + (xDb - thresh) / ratio;
        return y - xDb;
    }
    float t = xDb - (thresh - half);
    float y = xDb + (1.0f / ratio - 1.0f) * t * t / (2.0f * knee);
    return y - xDb;
}

// ---- ITU-R BS.1770-4 K-weighting (48 kHz prototype, pre-warped elsewhere)
struct KWeight {
    Biquad pre, rlb;
    void prepare(float sr) {
        // Official 48 kHz coefficients from BS.1770-4, bilinear-scaled to sr.
        // Stage 1 (pre-filter) and stage 2 (RLB) at 48 kHz:
        const float sr0 = 48000.0f;
        // We rebuild via analogue prototypes so other rates stay correct.
        // Pre-filter: high shelf, ~1.5 kHz, +4 dB, Q ~ 0.707 — the standard
        // digital coeffs at 48 kHz are applied with frequency scaling of poles.
        if (std::abs(sr - 48000.0f) < 0.5f) {
            pre.setCoeffs(1.53512485958697f, -2.69169618940638f, 1.19839281085285f,
                          1.0f, -1.69065929318241f, 0.73248077421585f);
            rlb.setCoeffs(1.0f, -2.0f, 1.0f,
                          1.0f, -1.99004745483398f, 0.99007225036621f);
        } else {
            // Bilinear transform of the 48 kHz filters via pre-warped frequency remap.
            // Use the well-known analogue-equivalent: pre-filter high-shelf and
            // 2nd-order highpass at ~38 Hz. This matches BS.1770 closely.
            pre.highShelf(sr, 1500.0f * sr / sr0, 3.999f, 0.7071f);
            // RLB: high-pass ~38 Hz, Q ~ 0.5
            rlb.highpass(sr, 38.0f, 0.5f);
        }
        reset();
    }
    void reset() { pre.reset(); rlb.reset(); }
    float process(float x) { return rlb.process(pre.process(x)); }
};

struct LufsMeter {
    KWeight kL, kR;
    std::vector<float> pwr;
    int write = 0, filled = 0;
    int win = 1;
    float sr = 48000;
    void prepare(float sampleRate, float seconds = 3.0f) {
        sr = sampleRate;
        kL.prepare(sr); kR.prepare(sr);
        win = std::max(1, (int)std::lround(seconds * sr));
        pwr.assign((size_t)win, 0.0f);
        write = filled = 0;
    }
    void reset() { kL.reset(); kR.reset(); std::fill(pwr.begin(), pwr.end(), 0.0f); write = filled = 0; }
    void push(float l, float r) {
        float zl = kL.process(l);
        float zr = kR.process(r);
        // Stereo channel weights are 1.0, 1.0.
        float s = zl * zl + zr * zr;
        if (pwr.empty()) return;
        pwr[(size_t)write] = s;
        write = (write + 1) % win;
        if (filled < win) ++filled;
    }
    float shortTerm() const {
        if (filled <= 0) return -120.0f;
        double acc = 0;
        for (int i = 0; i < filled; ++i) acc += pwr[(size_t)i];
        // If the ring isn't using only the first `filled` when wrapped, sum correctly:
        if (filled == win) {
            acc = 0;
            for (float v : pwr) acc += v;
        }
        double mean = acc / (double)filled;
        if (mean < 1.0e-20) return -120.0f;
        return (float)(-0.691 + 10.0 * std::log10(mean));
    }
};

// ---- Envelope ------------------------------------------------------------
struct EnvFollow {
    float env = 0;
    float atk = 0, rel = 0;
    void setTimes(float sr, float atkSec, float relSec) {
        atk = atkSec <= 0 ? 1.0f : 1.0f - std::exp(-1.0f / (atkSec * sr));
        rel = relSec <= 0 ? 1.0f : 1.0f - std::exp(-1.0f / (relSec * sr));
    }
    float process(float xAbs) {
        float c = (xAbs > env) ? atk : rel;
        env += c * (xAbs - env);
        return env;
    }
    void reset() { env = 0; }
};

// ---- 3-band LR4 crossover ------------------------------------------------
struct LR4Split {
    Biquad lpA, lpB, hpA, hpB;
    void prepare(float sr, float hz) {
        const float Q = 0.70710678f; // Butterworth, two in series = LR4
        lpA.lowpass(sr, hz, Q); lpB.lowpass(sr, hz, Q);
        hpA.highpass(sr, hz, Q); hpB.highpass(sr, hz, Q);
    }
    void reset() { lpA.reset(); lpB.reset(); hpA.reset(); hpB.reset(); }
    void process(float x, float& lo, float& hi) {
        lo = lpB.process(lpA.process(x));
        hi = hpB.process(hpA.process(x));
    }
};

struct ThreeBand {
    LR4Split loSplit, hiSplit;
    void prepare(float sr, float loHz = 500.0f, float hiHz = 10000.0f) {
        loSplit.prepare(sr, loHz);
        hiSplit.prepare(sr, hiHz);
    }
    void reset() { loSplit.reset(); hiSplit.reset(); }
    void process(float x, float& lo, float& mid, float& hi) {
        float highish;
        loSplit.process(x, lo, highish);
        hiSplit.process(highish, mid, hi);
    }
};

// ---- Lookahead limiter ---------------------------------------------------
struct LookaheadLim {
    int delay = kLookaheadHost;
    std::vector<float> bufL, bufR;
    int w = 0, size = 0;
    std::vector<float> peakBuf;
    int pW = 0;
    float gain = 1.0f;
    float relCoeff = 0;
    float sr = 48000;
    float ceilV = 0.988553f;
    float prevInL = 0, prevInR = 0;
    void prepare(float sampleRate, int delaySamples) {
        sr = sampleRate;
        delay = std::max(1, delaySamples);
        size = delay + 8;
        bufL.assign((size_t)size, 0); bufR.assign((size_t)size, 0);
        peakBuf.assign((size_t)delay, 0);
        w = pW = 0;
        gain = 1.0f;
        prevInL = prevInR = 0;
        relCoeff = 1.0f - std::exp(-1.0f / (0.12f * sr)); // ~120 ms baseline release
        ceilV = ceilingLin();
    }
    void reset() {
        std::fill(bufL.begin(), bufL.end(), 0.0f);
        std::fill(bufR.begin(), bufR.end(), 0.0f);
        std::fill(peakBuf.begin(), peakBuf.end(), 0.0f);
        w = pW = 0; gain = 1.0f;
        prevInL = prevInR = 0;
    }
    static float truePeak4(float a, float b) {
        // 4-point interpolator between a (previous) and b (current): 0, 0.25, 0.5, 0.75, 1
        float pk = std::max(std::abs(a), std::abs(b));
        // Cubic Hermite between a and b, with tangents from a simple estimate (prev slope ~ b-a)
        float t0 = b - a;
        for (int i = 1; i <= 3; ++i) {
            float t = i * 0.25f;
            // Hermite with m0=m1=t0 (catmull-ish)
            float t2 = t * t, t3 = t2 * t;
            float y = (2 * t3 - 3 * t2 + 1) * a + (t3 - 2 * t2 + t) * t0
                    + (-2 * t3 + 3 * t2) * b + (t3 - t2) * t0;
            pk = std::max(pk, std::abs(y));
        }
        return pk;
    }
    void process(float& l, float& r, float /*makeupLin*/, bool active, float& grL, float& grR) {
        float inL = l, inR = r;
        bufL[(size_t)w] = inL;
        bufR[(size_t)w] = inR;
        int rIdx = w - delay; if (rIdx < 0) rIdx += size;
        w = (w + 1) % size;

        float pk = truePeak4(prevInL, inL);
        pk = std::max(pk, truePeak4(prevInR, inR));
        prevInL = inL; prevInR = inR;
        peakBuf[(size_t)pW] = pk;
        // max in lookahead window
        float future = 0;
        for (int i = 0; i < delay; ++i) future = std::max(future, peakBuf[(size_t)i]);
        pW = (pW + 1) % delay;

        float target = 1.0f;
        if (active && future > 1.0e-12f) {
            float need = ceilV / future;
            if (need < 1.0f) target = need;
        }
        if (target < gain) gain = target; // instant attack
        else gain += relCoeff * (target - gain);

        if (!active) {
            // still delay, unity gain
            l = bufL[(size_t)rIdx];
            r = bufR[(size_t)rIdx];
            grL = grR = 0;
            return;
        }
        l = bufL[(size_t)rIdx] * gain;
        r = bufR[(size_t)rIdx] * gain;
        float gdb = (gain < 0.999f) ? linToDb(gain) : 0.0f;
        grL = gdb; grR = gdb;
    }
};

// ---- Half-band 2× oversampler stage --------------------------------------
struct Halfband2 {
    // Linear-phase half-band FIR, 33 taps (delay 16 at this rate).
    static constexpr int taps = 33;
    static constexpr int delay = 16;
    std::array<float, taps> h{};
    std::array<float, taps> zL{}, zR{}, zSL{}, zSR{};
    Halfband2() {
        // Windowed-sinc cutoff 0.25 (half-band at the upsampled rate is 0.5 of new Nyquist,
        // i.e. 0.25 cycles/sample at 2x). For a 2x upsampler we filter at 0.25.
        // After inserting zeros, lowpass at pi/2 (0.25 cycles/sample).
        const int M = delay;
        for (int n = 0; n < taps; ++n) {
            int k = n - M;
            float sinc = (k == 0) ? 0.5f : std::sin(kPi * 0.5f * (float)k) / (kPi * (float)k);
            float win = 0.5f - 0.5f * std::cos(2.0f * kPi * (float)n / (float)(taps - 1));
            h[(size_t)n] = sinc * win;
        }
        // Normalise passband gain to 1 (DC)
        float sum = 0; for (float v : h) sum += v;
        if (sum != 0) for (float& v : h) v /= sum;
    }
    void reset() {
        zL.fill(0); zR.fill(0); zSL.fill(0); zSR.fill(0);
    }
    float dot(const std::array<float, taps>& z) const {
        float acc = 0;
        for (int i = 0; i < taps; ++i) acc += h[(size_t)i] * z[(size_t)i];
        return acc;
    }
    void push(std::array<float, taps>& z, float x) {
        for (int i = taps - 1; i > 0; --i) z[(size_t)i] = z[(size_t)(i - 1)];
        z[0] = x;
    }
    // Upsample 1 sample → 2. Gain compensation: inserted-zero path has 0.5 energy, we already
    // normalised DC of the filter to 1, so after zero-insert we must run the filter at 2x and
    // scale by 2 (classic).
    void up(float l, float r, float sl, float sr, float outL[2], float outR[2], float outSL[2], float outSR[2]) {
        push(zL, l);  push(zR, r);  push(zSL, sl);  push(zSR, sr);
        outL[0] = 2.0f * dot(zL); outR[0] = 2.0f * dot(zR);
        outSL[0] = 2.0f * dot(zSL); outSR[0] = 2.0f * dot(zSR);
        push(zL, 0);  push(zR, 0);  push(zSL, 0);  push(zSR, 0);
        outL[1] = 2.0f * dot(zL); outR[1] = 2.0f * dot(zR);
        outSL[1] = 2.0f * dot(zSL); outSR[1] = 2.0f * dot(zSR);
    }
    // Downsample 2 samples → 1: push both, take the filtered output after the second.
    void downPush(float l, float r) { push(zL, l); push(zR, r); }
    void downGet(float& l, float& r) { l = dot(zL); r = dot(zR); }
};

// Delay for oversampler-only path alignment is accounted in Engine::latencySamples().

// ---- Engine --------------------------------------------------------------
struct Engine {
    float sr = 48000;
    int   os = 1;
    int   maxBlock = 512;

    Biquad eqL_lo, eqL_mid, eqL_hi;
    Biquad eqR_lo, eqR_mid, eqR_hi;

    Biquad hsL_lo, hsL_mid, hsL_hi; // mid channel house
    Biquad hsR_lo, hsR_mid, hsR_hi; // side channel house
    Biquad tiltS;                   // side HF tilt

    ThreeBand xL, xR;               // sat bands (L/R)
    ThreeBand xM, xS;               // comp bands (M/S)
    ThreeBand xDet;                 // external / mid key split

    EnvFollow envLo, envMid, envHi;
    LookaheadLim limiter;
    LufsMeter inLufs, outLufs;

    // RMS / peak meters (ballistics)
    float inPkL = 0, inPkR = 0, outPkL = 0, outPkR = 0;
    float inMsL = 0, inMsR = 0, outMsL = 0, outMsR = 0;
    float lastGrLo = 0, lastGrMid = 0, lastGrHi = 0;
    float lastLimL = 0, lastLimR = 0;

    // Delay line for bypass / unity path (host samples)
    std::vector<float> dL, dR;
    int dW = 0, dSize = 0, dLen = 0;

    // Previous samples for true-peak
    float prevLimL = 0, prevLimR = 0;

    // Oversamplers: up to 3 cascaded 2× stages (8×)
    Halfband2 up[3], down[3];
    std::vector<float> osL, osR, osSL, osSR;
    std::vector<float> scratchL, scratchR, scratchSL, scratchSR;
    void ensureScratch(int need) {
        if ((int)scratchL.size() < need) {
            scratchL.resize((size_t)need); scratchR.resize((size_t)need);
            scratchSL.resize((size_t)need); scratchSR.resize((size_t)need);
        }
    }

    // Comp settings (documented, tested)
    float compThreshLo  = -24.0f;
    float compThreshMid = -22.0f;
    float compThreshHi  = -20.0f;
    float compRatio     = 2.5f;
    float compKnee      = 18.0f;
    float compMakeupDb  = 2.5f;

    void prepare(float sampleRate, int maxBlockSize, int oversample) {
        sr = sampleRate;
        maxBlock = std::max(16, maxBlockSize);
        os = oversample;
        if (os != 1 && os != 2 && os != 4 && os != 8) os = 1;
        float work = sr * (float)os;

        auto setupEq = [&](Biquad&, Biquad&, Biquad&, float, float, float) {};
        (void)setupEq;

        xL.prepare(work); xR.prepare(work);
        xM.prepare(work); xS.prepare(work);
        xDet.prepare(work);
        envLo.setTimes(work, 0.012f, 0.180f);
        envMid.setTimes(work, 0.008f, 0.140f);
        envHi.setTimes(work, 0.004f, 0.080f);
        limiter.prepare(work, kLookaheadHost * os);
        inLufs.prepare(sr, 3.0f);
        outLufs.prepare(sr, 3.0f);

        dLen = latencySamples();
        dSize = dLen + 8;
        dL.assign((size_t)dSize, 0); dR.assign((size_t)dSize, 0);
        dW = 0;

        osL.resize((size_t)maxBlock * 8); osR.resize((size_t)maxBlock * 8);
        osSL.resize((size_t)maxBlock * 8); osSR.resize((size_t)maxBlock * 8);
        ensureScratch(maxBlock * 8);
        for (int i = 0; i < 3; ++i) { up[i].reset(); down[i].reset(); }
        reset();
    }

    void reset() {
        eqL_lo.reset(); eqL_mid.reset(); eqL_hi.reset();
        eqR_lo.reset(); eqR_mid.reset(); eqR_hi.reset();
        hsL_lo.reset(); hsL_mid.reset(); hsL_hi.reset();
        hsR_lo.reset(); hsR_mid.reset(); hsR_hi.reset();
        tiltS.reset();
        xL.reset(); xR.reset(); xM.reset(); xS.reset(); xDet.reset();
        envLo.reset(); envMid.reset(); envHi.reset();
        limiter.reset();
        inLufs.reset(); outLufs.reset();
        inPkL = inPkR = outPkL = outPkR = 0;
        inMsL = inMsR = outMsL = outMsR = 0;
        lastGrLo = lastGrMid = lastGrHi = lastLimL = lastLimR = 0;
        std::fill(dL.begin(), dL.end(), 0.0f);
        std::fill(dR.begin(), dR.end(), 0.0f);
        dW = 0;
        prevLimL = prevLimR = 0;
        for (int i = 0; i < 3; ++i) { up[i].reset(); down[i].reset(); }
    }

    int osFilterDelayHost() const {
        // Each 2× stage used twice (up and down). Halfband delay is 16 samples at that stage's rate.
        // Stage 0 (2×): 16 / 2 = 8 host per pass → 16 host for up+down
        // Stage 1 (4×): 16 / 4 = 4 host per pass → 8
        // Stage 2 (8×): 16 / 8 = 2 host per pass → 4
        int stages = 0;
        if (os >= 2) stages = 1;
        if (os >= 4) stages = 2;
        if (os >= 8) stages = 3;
        int d = 0, div = 2;
        for (int i = 0; i < stages; ++i) {
            d += 2 * (Halfband2::delay / div);
            div *= 2;
        }
        return d;
    }

    int latencySamples() const {
        // Windowed-sinc up/down measures one host sample less than the
        // integer group-delay sum (verified by impulse tests at 2x/4x/8x).
        int osDelay = osFilterDelayHost();
        if (osDelay > 0) osDelay -= 1;
        return kLookaheadHost + osDelay;
    }

    static void atomGains(float amount, float& loM, float& loS, float& midM, float& midS, float& hiM, float& hiS) {
        // Documented Atom curve. amount is 0..200.
        auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
        if (amount <= 100.0f) {
            float t = clampf(amount, 0, 100) / 100.0f;
            loM = lerp(0, 3.0f, t);  loS  = lerp(0, 3.0f, t);
            midM = lerp(0, 3.5f, t); midS = lerp(0, 3.5f, t);
            hiM = lerp(0, 1.0f, t);  hiS  = lerp(0, 4.0f, t);
        } else {
            float t = clampf(amount - 100.0f, 0, 100) / 100.0f;
            loM = lerp(3.0f, 4.0f, t);   loS  = lerp(3.0f, 4.0f, t);
            midM = lerp(3.5f, 5.8f, t);  midS = lerp(3.5f, 7.7f, t);
            hiM = lerp(1.0f, 3.0f, t);   hiS  = lerp(4.0f, 7.0f, t);
        }
    }

    void updateFilters(const Params& p, float workSr) {
        bool eqActive = p.eqOn && (std::abs(p.eqLowDb) + std::abs(p.eqMidDb) + std::abs(p.eqHighDb) > 1.0e-6f);
        if (eqActive) {
            eqL_lo.lowShelf(workSr, 200.0f, p.eqLowDb);
            eqR_lo.lowShelf(workSr, 200.0f, p.eqLowDb);
            eqL_mid.peaking(workSr, 640.0f, p.eqMidDb, 0.5f);
            eqR_mid.peaking(workSr, 640.0f, p.eqMidDb, 0.5f);
            eqL_hi.highShelf(workSr, 10000.0f, p.eqHighDb);
            eqR_hi.highShelf(workSr, 10000.0f, p.eqHighDb);
        } else {
            eqL_lo.identity(); eqL_mid.identity(); eqL_hi.identity();
            eqR_lo.identity(); eqR_mid.identity(); eqR_hi.identity();
        }
        float loM, loS, midM, midS, hiM, hiS;
        atomGains(p.amount, loM, loS, midM, midS, hiM, hiS);
        hsL_lo.lowShelf(workSr, 150.0f, loM);
        hsR_lo.lowShelf(workSr, 150.0f, loS);
        hsL_mid.peaking(workSr, 800.0f, midM, 0.40f);
        hsR_mid.peaking(workSr, 800.0f, midS, 0.40f);
        hsL_hi.highShelf(workSr, 10000.0f, hiM);
        hsR_hi.highShelf(workSr, 10000.0f, hiS);
        float tiltDb = 0;
        if (p.amount <= 100.0f) tiltDb = 1.5f * (p.amount / 100.0f);
        else tiltDb = 1.5f + 1.0f * ((p.amount - 100.0f) / 100.0f);
        tiltS.highShelf(workSr, 8000.0f, tiltDb);
    }

    static float satOdd(float x, float drive) {
        // Symmetric (odd-only) soft clip. drive >= 1.
        float d = std::max(1.0f, drive);
        float y = std::tanh(d * x);
        // Compensate so small-signal gain stays ~1
        float g = std::tanh(d);
        if (g < 1.0e-6f) return x;
        return y / g * (0.70f + 0.30f / d); // slight level trim as drive rises
    }

    void processWork(float* L, float* R, const float* scL, const float* scR, int n, const Params& p) {
        float workSr = sr * (float)os;
        updateFilters(p, workSr);
        const float inG  = dbToLin(p.inputDb);
        const float outG = dbToLin(p.outputDb);
        const float limG = dbToLin(p.limiterDb);
        const float ceilV = ceilingLin();
        const bool charOn = p.characterOn && p.amount > 0.0f;
        const float wet = clampf(p.amount, 0.0f, 100.0f) / 100.0f;
        float drive = 1.0f;
        if (p.amount <= 100.0f) drive = 1.0f + 1.2f * (p.amount / 100.0f);
        else drive = 2.2f + 2.0f * ((p.amount - 100.0f) / 100.0f);
        const float makeup = dbToLin(p.compOn ? compMakeupDb * wet : 0.0f);

        // Amount 0–100 scales GR; 100–200 leaves GR at full and adds curve/drive (already in filters).
        const float grScale = clampf(p.amount, 0.0f, 100.0f) / 100.0f;

        const bool eqActive = p.eqOn && (std::abs(p.eqLowDb) + std::abs(p.eqMidDb) + std::abs(p.eqHighDb) > 1.0e-6f);
        const bool skipAllFx = p.bypass || (!eqActive && !charOn && !p.limiterOn && std::abs(p.inputDb) < 1e-8f && std::abs(p.outputDb) < 1e-8f);

        float accGrLo = 0, accGrMid = 0, accGrHi = 0, accLim = 0;
        int grCount = 0;

        for (int i = 0; i < n; ++i) {
            float l = L[i] * inG;
            float r = R[i] * inG;
            float scl = (scL ? scL[i] : l) * inG;
            float scr = (scR ? scR[i] : r) * inG;
            if (!p.extSidechain) { scl = l; scr = r; }

            // Input meters (pre-EQ, post input gain) — at work rate we still feed LUFS only at host
            // (caller does host-rate LUFS). Here we track peak/rms at work rate; Engine::process
            // will also track at host rate.
            if (eqActive && !p.bypass) {
                l = eqL_hi.process(eqL_mid.process(eqL_lo.process(l)));
                r = eqR_hi.process(eqR_mid.process(eqR_lo.process(r)));
                if (p.extSidechain) {
                    // sidechain is not EQ'd into the detector from the main EQ; leave as-is
                }
            }

            float dryL = l, dryR = r;

            if (charOn && !p.bypass) {
                float mid = 0.5f * (l + r);
                float side = 0.5f * (l - r);
                if (p.houseOn) {
                    mid  = hsL_hi.process(hsL_mid.process(hsL_lo.process(mid)));
                    side = hsR_hi.process(hsR_mid.process(hsR_lo.process(side)));
                }
                float mLo, mMid, mHi, sLo, sMid, sHi;
                xM.process(mid,  mLo, mMid, mHi);
                xS.process(side, sLo, sMid, sHi);

                float keyLo, keyMid, keyHi;
                if (p.extSidechain) {
                    float scMid = 0.5f * (scl + scr);
                    xDet.process(scMid, keyLo, keyMid, keyHi);
                } else {
                    keyLo = mLo; keyMid = mMid; keyHi = mHi;
                }

                auto applyComp = [&](float key, EnvFollow& env, float thresh, float& bandM, float& bandS, float& acc) {
                    float e = env.process(std::abs(key));
                    float xDb = linToDb(std::max(e, 1.0e-9f));
                    float gr = 0;
                    if (p.compOn) gr = softKneeGrDb(xDb, thresh, compRatio, compKnee) * grScale;
                    float g = dbToLin(gr);
                    bandM *= g;
                    bandS *= g;
                    acc += gr;
                };
                if (p.compOn) {
                    applyComp(keyLo,  envLo,  compThreshLo,  mLo,  sLo,  accGrLo);
                    applyComp(keyMid, envMid, compThreshMid, mMid, sMid, accGrMid);
                    applyComp(keyHi,  envHi,  compThreshHi,  mHi,  sHi,  accGrHi);
                    ++grCount;
                    mLo *= makeup; mMid *= makeup; mHi *= makeup;
                    sLo *= makeup; sMid *= makeup; sHi *= makeup;
                }
                mid  = mLo + mMid + mHi;
                side = sLo + sMid + sHi;
                if (p.tiltOn) side = tiltS.process(side);
                l = mid + side;
                r = mid - side;

                if (p.satOn) {
                    float lLo, lMid, lHi, rLo, rMid, rHi;
                    xL.process(l, lLo, lMid, lHi);
                    xR.process(r, rLo, rMid, rHi);
                    float dLo = drive * 0.85f, dMid = drive, dHi = drive * 1.15f;
                    l = satOdd(lLo, dLo) + satOdd(lMid, dMid) + satOdd(lHi, dHi);
                    r = satOdd(rLo, dLo) + satOdd(rMid, dMid) + satOdd(rHi, dHi);
                }
                l = dryL + wet * (l - dryL);
                r = dryR + wet * (r - dryR);
            }

            l *= limG;
            r *= limG;
            float grl = 0, grr = 0;
            limiter.process(l, r, 1.0f, p.limiterOn && !p.bypass, grl, grr);
            accLim += 0.5f * (grl + grr);
            l *= outG;
            r *= outG;
            // Safety ceiling (soft)
            auto safety = [&](float x) {
                float a = std::abs(x);
                if (a <= ceilV) return x;
                // hard-ish clip at ceiling (true-peak already handled in limiter)
                return std::copysign(ceilV, x);
            };
            l = safety(flush(l));
            r = safety(flush(r));
            if (p.bypass || skipAllFx) {
                // skipAllFx still ran the limiter delay if we went through limiter.process.
                // For true bit-exact bypass we must not have touched filters. Handled in process().
            }
            L[i] = l;
            R[i] = r;
        }
        if (grCount > 0) {
            lastGrLo  = accGrLo  / (float)grCount;
            lastGrMid = accGrMid / (float)grCount;
            lastGrHi  = accGrHi  / (float)grCount;
        } else {
            lastGrLo = lastGrMid = lastGrHi = 0;
        }
        lastLimL = lastLimR = (n > 0 ? accLim / (float)n : 0);
    }

    // Host-rate delay (for the unity/bypass path). Used when we must be bit-exact
    // and still honour PDC: we delay input by latencySamples().
    void delayOnly(float* L, float* R, int n) {
        if (dLen <= 0 || dSize <= 0) return;
        for (int i = 0; i < n; ++i) {
            float inL = L[i], inR = R[i];
            int rIdx = dW - dLen; if (rIdx < 0) rIdx += dSize;
            L[i] = dL[(size_t)rIdx];
            R[i] = dR[(size_t)rIdx];
            dL[(size_t)dW] = inL;
            dR[(size_t)dW] = inR;
            dW = (dW + 1) % dSize;
        }
    }

    void process(float* L, float* R, const float* scL, const float* scR, int n, const Params& p) {
        if (n <= 0) return;

        // Meter the raw input first
        const float inG = dbToLin(p.inputDb);
        for (int i = 0; i < n; ++i) {
            float l = L[i] * inG, r = R[i] * inG;
            inPkL = std::max(inPkL * 0.9995f, std::abs(L[i] * inG));
            inPkR = std::max(inPkR * 0.9995f, std::abs(R[i] * inG));
            inMsL += 0.005f * ((l * l) - inMsL);
            inMsR += 0.005f * ((r * r) - inMsR);
            inLufs.push(L[i] * inG, R[i] * inG);
        }

        const bool eqActive = p.eqOn && (std::abs(p.eqLowDb) + std::abs(p.eqMidDb) + std::abs(p.eqHighDb) > 1.0e-6f);
        const bool charOn = p.characterOn && p.amount > 0.0f;
        const bool unity = p.bypass || (!eqActive && !charOn && !p.limiterOn
                                        && std::abs(p.inputDb) < 1e-8f && std::abs(p.outputDb) < 1e-8f);

        if (unity) {
            delayOnly(L, R, n);
        } else if (os == 1) {
            processWork(L, R, scL, scR, n, p);
        } else {
            const int stages = (os == 2) ? 1 : (os == 4) ? 2 : 3;
            const int need = n * os;
            if ((int)osL.size() < need) {
                osL.resize((size_t)need); osR.resize((size_t)need);
                osSL.resize((size_t)need); osSR.resize((size_t)need);
            }
            // Stage ping-pong: use the os* buffers. Work from host into the final  os-rate buffer
            // by cascading through temporary regions: we'll use a scratch vector pair kept as members.
            ensureScratch(need);
            float* aL = scratchL.data();
            float* aR = scratchR.data();
            float* sL = scratchSL.data();
            float* sR = scratchSR.data();
            for (int i = 0; i < n; ++i) {
                aL[i] = L[i]; aR[i] = R[i];
                sL[i] = scL ? scL[i] : L[i];
                sR[i] = scR ? scR[i] : R[i];
            }
            int count = n;
            for (int st = 0; st < stages; ++st) {
                float* bL = osL.data();
                float* bR = osR.data();
                float* bSL = osSL.data();
                float* bSR = osSR.data();
                for (int i = 0; i < count; ++i) {
                    float oL[2], oR[2], oSL[2], oSR[2];
                    up[st].up(aL[i], aR[i], sL[i], sR[i], oL, oR, oSL, oSR);
                    bL[2 * i] = oL[0]; bL[2 * i + 1] = oL[1];
                    bR[2 * i] = oR[0]; bR[2 * i + 1] = oR[1];
                    bSL[2 * i] = oSL[0]; bSL[2 * i + 1] = oSL[1];
                    bSR[2 * i] = oSR[0]; bSR[2 * i + 1] = oSR[1];
                }
                // swap roles: os* now holds upsampled; copy back to scratch for next stage
                count *= 2;
                std::memcpy(aL, bL, (size_t)count * sizeof(float));
                std::memcpy(aR, bR, (size_t)count * sizeof(float));
                std::memcpy(sL, bSL, (size_t)count * sizeof(float));
                std::memcpy(sR, bSR, (size_t)count * sizeof(float));
            }
            processWork(aL, aR, sL, sR, count, p);
            for (int st = stages - 1; st >= 0; --st) {
                int next = count / 2;
                for (int i = 0; i < next; ++i) {
                    down[st].downPush(aL[2 * i], aR[2 * i]);
                    down[st].downPush(aL[2 * i + 1], aR[2 * i + 1]);
                    down[st].downGet(osL[(size_t)i], osR[(size_t)i]);
                }
                std::memcpy(aL, osL.data(), (size_t)next * sizeof(float));
                std::memcpy(aR, osR.data(), (size_t)next * sizeof(float));
                count = next;
            }
            std::memcpy(L, aL, (size_t)n * sizeof(float));
            std::memcpy(R, aR, (size_t)n * sizeof(float));
        }

        for (int i = 0; i < n; ++i) {
            outPkL = std::max(outPkL * 0.9995f, std::abs(L[i]));
            outPkR = std::max(outPkR * 0.9995f, std::abs(R[i]));
            outMsL += 0.005f * ((L[i] * L[i]) - outMsL);
            outMsR += 0.005f * ((R[i] * R[i]) - outMsR);
            outLufs.push(L[i], R[i]);
        }
    }

    void decayMeters() {
        // peaks are already decaying in the loop
    }

    Meters meters() const {
        Meters m;
        auto pkDb = [](float x) { return x <= 1e-9f ? -120.0f : linToDb(x); };
        auto rmsDb = [](float ms) { return ms <= 1e-20f ? -120.0f : linToDb(std::sqrt(ms)); };
        m.inPeakLinL = inPkL; m.inPeakLinR = inPkR;
        m.outPeakLinL = outPkL; m.outPeakLinR = outPkR;
        m.inPeakL = pkDb(inPkL); m.inPeakR = pkDb(inPkR);
        m.outPeakL = pkDb(outPkL); m.outPeakR = pkDb(outPkR);
        m.inRmsL = rmsDb(inMsL); m.inRmsR = rmsDb(inMsR);
        m.outRmsL = rmsDb(outMsL); m.outRmsR = rmsDb(outMsR);
        m.inLufs = inLufs.shortTerm();
        m.outLufs = outLufs.shortTerm();
        m.grLow = lastGrLo; m.grMid = lastGrMid; m.grHigh = lastGrHi;
        m.limGrL = lastLimL; m.limGrR = lastLimR;
        return m;
    }

    void resetPeaks() {
        inPkL = inPkR = outPkL = outPkR = 0;
        inMsL = inMsR = outMsL = outMsR = 0;
        inLufs.reset(); outLufs.reset();
    }
};

// ---- Factory presets -----------------------------------------------------
inline Params presetByName(const char* name) {
    Params p;
    auto is = [&](const char* s) { return std::strcmp(name, s) == 0; };
    if (is("Default")) return p;
    if (is("Indie")) { p.limiterDb = 3.0f; p.eqHighDb = 0.5f; p.amount = 90.0f; return p; }
    if (is("Ambient")) { p.amount = 55.0f; p.limiterDb = 1.5f; p.eqHighDb = -0.5f; return p; }
    if (is("Late Night")) { p.amount = 130.0f; p.limiterDb = 7.0f; p.eqLowDb = 0.5f; p.eqHighDb = 1.0f; return p; }
    if (is("Folk")) { p.amount = 70.0f; p.limiterDb = 2.0f; p.eqHighDb = -0.3f; p.eqMidDb = 0.4f; return p; }
    if (is("Glue Only")) { p.amount = 80.0f; p.limiterOn = false; p.limiterDb = 0.0f; return p; }
    if (is("Wide Air")) { p.amount = 120.0f; p.eqHighDb = 2.0f; p.limiterDb = 4.0f; return p; }
    if (is("Quiet Mix")) { p.amount = 40.0f; p.limiterDb = 1.0f; return p; }
    return p;
}

inline constexpr const char* kPresetNames[] = {
    "Default", "Indie", "Ambient", "Late Night", "Folk", "Glue Only", "Wide Air", "Quiet Mix"
};
inline constexpr int kNumPresets = 8;

} // namespace ga
