#include "Engine.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static int passed = 0, failed = 0;
#define CHECK(x) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s | ", __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::vector<float> sine(int n, float sr, float hz, float amp, float phase = 0) {
    std::vector<float> y((size_t)n);
    double ph = phase;
    double inc = 2.0 * M_PI * (double)hz / (double)sr;
    for (int i = 0; i < n; ++i) { y[(size_t)i] = amp * (float)std::sin(ph); ph += inc; }
    return y;
}
static float peakOf(const std::vector<float>& a, const std::vector<float>& b) {
    float p = 0;
    for (float v : a) p = std::max(p, std::abs(v));
    for (float v : b) p = std::max(p, std::abs(v));
    return p;
}
static float rmsOf(const std::vector<float>& a, int start = 0) {
    double acc = 0; int n = (int)a.size() - start;
    if (n <= 0) return 0;
    for (int i = start; i < (int)a.size(); ++i) acc += (double)a[(size_t)i] * a[(size_t)i];
    return (float)std::sqrt(acc / n);
}
static bool allFinite(const std::vector<float>& a) {
    for (float v : a) if (!std::isfinite(v)) return false;
    return true;
}
static int maxAbsOffset(const std::vector<float>& in, const std::vector<float>& out, float thresh = 0.1f) {
    // estimate delay by locating first significant energy
    auto first = [&](const std::vector<float>& x) {
        for (int i = 0; i < (int)x.size(); ++i) if (std::abs(x[(size_t)i]) > thresh) return i;
        return -1;
    };
    int a = first(in), b = first(out);
    if (a < 0 || b < 0) return -9999;
    return b - a;
}

struct Run {
    ga::Engine e;
    std::vector<float> L, R;
    Run(float sr = 48000, int block = 512, int os = 1) {
        e.prepare(sr, block, os);
    }
    void go(const std::vector<float>& inL, const std::vector<float>& inR,
            const ga::Params& p, const std::vector<float>* scL = nullptr, const std::vector<float>* scR = nullptr,
            int block = 512) {
        L = inL; R = inR;
        int n = (int)L.size();
        int pos = 0;
        while (pos < n) {
            int m = std::min(block, n - pos);
            const float* kL = scL ? scL->data() + pos : nullptr;
            const float* kR = scR ? scR->data() + pos : nullptr;
            e.process(L.data() + pos, R.data() + pos, kL, kR, m, p);
            pos += m;
        }
    }
};

static void test_soft_knee_math() {
    // Hard-knee limit of the formula: above thresh+half, GR = (1/ratio - 1)*(x-thresh)
    float t = -20.f, ratio = 2.5f, knee = 18.f;
    CHECK(std::abs(ga::softKneeGrDb(-40.f, t, ratio, knee)) < 1e-6f);
    float x = 0.f; // well above knee
    float y = t + (x - t) / ratio;
    CHECKM(std::abs(ga::softKneeGrDb(x, t, ratio, knee) - (y - x)) < 1e-5f, "got %f expected %f",
           ga::softKneeGrDb(x, t, ratio, knee), y - x);
    // Mid-knee is between 0 and the hard-knee value
    float midX = t;
    float gr = ga::softKneeGrDb(midX, t, ratio, knee);
    float hard = (t + (midX - t) / ratio) - midX;
    CHECK(gr <= 0);
    CHECK(gr < 0.0f); // at thresh, the soft knee is already reducing
    (void)hard;
}

static void test_eq_textbook() {
    const float sr = 48000;
    ga::Biquad lo, mid, hi;
    lo.lowShelf(sr, 200.f, 6.f);
    mid.peaking(sr, 640.f, 6.f, 0.5f);
    hi.highShelf(sr, 10000.f, 6.f);
    // Low shelf well below hinge should be close to +6
    float loDeep = ga::biquadMagDb(lo, sr, 40.f);
    CHECKM(std::abs(loDeep - 6.f) < 0.35f, "low shelf 40 Hz = %f", loDeep);
    // High shelf well above hinge
    float hiAir = ga::biquadMagDb(hi, sr, 18000.f);
    CHECKM(std::abs(hiAir - 6.f) < 0.6f, "high shelf 18k = %f", hiAir);
    // Peak at 640
    float midPk = ga::biquadMagDb(mid, sr, 640.f);
    CHECKM(std::abs(midPk - 6.f) < 0.15f, "mid peak = %f", midPk);
    // 0 dB is identity
    ga::Biquad z; z.lowShelf(sr, 200.f, 0.f);
    CHECK(z.b0 == 1.f && z.b1 == 0.f && z.a1 == 0.f);
}

static void test_null_and_bypass() {
    const float sr = 48000;
    const int n = 48000;
    auto inL = sine(n, sr, 440.f, 0.2f);
    auto inR = sine(n, sr, 440.f, 0.2f, 0.3f);
    // also sprinkle some noise
    uint32_t rng = 1;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        inL[(size_t)i] += 0.01f * ((float)((rng >> 9) & 0xffff) / 32768.f - 1.f);
        inR[(size_t)i] += 0.01f * ((float)((rng >> 17) & 0xffff) / 32768.f - 1.f);
    }

    ga::Params unity; // all defaults then neutralize
    unity.limiterOn = false;
    unity.limiterDb = 0;
    unity.amount = 0;
    unity.characterOn = false;
    unity.eqOn = false;
    unity.inputDb = 0;
    unity.outputDb = 0;

    Run a(sr, 512, 1);
    a.go(inL, inR, unity);
    // Compare delayed input
    int lat = a.e.latencySamples();
    CHECKM(lat == ga::kLookaheadHost, "lat=%d", lat);
    bool exact = true;
    int mismatches = 0;
    for (int i = 0; i < n; ++i) {
        float expL = (i >= lat) ? inL[(size_t)(i - lat)] : 0.f;
        float expR = (i >= lat) ? inR[(size_t)(i - lat)] : 0.f;
        if (a.L[(size_t)i] != expL || a.R[(size_t)i] != expR) { exact = false; ++mismatches; }
    }
    CHECKM(exact, "unity mismatches=%d / %d lat=%d", mismatches, n, lat);

    ga::Params byp = unity; byp.bypass = true;
    Run b(sr, 512, 1);
    b.go(inL, inR, byp);
    bool same = (b.L == a.L && b.R == a.R);
    CHECKM(same, "bypass vs unity not bit-exact");

    // Amount 0 with character "on" but amount 0, limiter off, eq 0 — also unity
    ga::Params amt0; amt0.amount = 0; amt0.limiterOn = false; amt0.limiterDb = 0;
    amt0.eqHighDb = amt0.eqMidDb = amt0.eqLowDb = 0;
    Run c(sr, 512, 1);
    c.go(inL, inR, amt0);
    CHECKM(c.L == a.L && c.R == a.R, "amount0 path not bit-exact vs unity");
}

static void test_nan() {
    const float sr = 48000;
    const int n = 4096;
    std::vector<float> L(n), R(n);
    uint32_t rng = 42;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        L[(size_t)i] = 4.0f * ((float)((rng >> 9) & 0xffff) / 32768.f - 1.f);
        R[(size_t)i] = 4.0f * ((float)((rng >> 17) & 0xffff) / 32768.f - 1.f);
    }
    L[0] = 0; L[1] = 1.e-40f; L[2] = -1.e-40f; L[3] = 1.f; L[4] = -1.f;
    ga::Params p; // default, all on, limiter +5
    Run r(sr, 64, 1);
    r.go(L, R, p, nullptr, nullptr, 64);
    CHECK(allFinite(r.L) && allFinite(r.R));
    CHECK(peakOf(r.L, r.R) <= ga::ceilingLin() + 1e-5f);
}

static void test_latency() {
    const float sr = 48000;
    // Impulse
    auto impulseTest = [&](int os, bool limOn) {
        const int n = 8000;
        std::vector<float> inL(n, 0), inR(n, 0);
        inL[100] = 0.01f; inR[100] = 0.01f; // low-level, should not limit
        ga::Params p;
        p.limiterOn = limOn;
        p.limiterDb = 0;
        p.amount = 0;
        p.characterOn = false;
        p.eqOn = false;
        p.inputDb = 0;
        p.outputDb = 0;
        Run r(sr, 128, os);
        r.go(inL, inR, p, nullptr, nullptr, 128);
        int declared = r.e.latencySamples();
        int measured = -1;
        float best = 0;
        for (int i = 0; i < n; ++i) {
            float v = std::abs(r.L[(size_t)i]);
            if (v > best) { best = v; measured = i - 100; }
        }
        CHECKM(measured == declared, "os=%d lim=%d declared=%d measured=%d peak=%g",
               os, (int)limOn, declared, measured, best);
        return declared;
    };
    int l1 = impulseTest(1, true);
    int l1b = impulseTest(1, false);
    CHECKM(l1 == l1b, "limiter on/off changed latency %d vs %d", l1, l1b);
    impulseTest(2, true);
    impulseTest(4, true);
    impulseTest(8, false);
}

static void test_atom_curve() {
    const float sr = 48000;
    // Low-level sines so the compressor stays out, house+tilt only (sat lightly at low level).
    auto measure = [&](float hz, float amount, bool midNotSide) {
        const int n = (int)sr * 2;
        auto s = sine(n, sr, hz, 0.003f);
        std::vector<float> L, R;
        if (midNotSide) { L = s; R = s; }
        else { L = s; R.resize(s.size()); for (int i = 0; i < n; ++i) R[(size_t)i] = -s[(size_t)i]; }
        ga::Params p;
        p.amount = amount;
        p.compOn = false;
        p.satOn = false;
        p.limiterOn = false;
        p.limiterDb = 0;
        p.eqOn = false;
        p.houseOn = true;
        p.tiltOn = (amount > 0);
        Run r(sr, 512, 1);
        r.go(L, R, p);
        int lat = r.e.latencySamples();
        // Reconstruct mid or side and compare RMS after settling
        double accIn = 0, accOut = 0;
        int start = lat + 4000;
        for (int i = start; i < n; ++i) {
            float inM = midNotSide ? L[(size_t)(i - lat)] : 0.5f * (L[(size_t)(i - lat)] - R[(size_t)(i - lat)]);
            // wait, L and R were overwritten. Use original s delayed.
            float in = s[(size_t)(i - lat)];
            float out = midNotSide ? 0.5f * (r.L[(size_t)i] + r.R[(size_t)i])
                                   : 0.5f * (r.L[(size_t)i] - r.R[(size_t)i]);
            accIn += in * in;
            accOut += out * out;
        }
        int nn = n - start;
        float ratio = (float)std::sqrt((accOut / nn) / (accIn / nn + 1e-30));
        return ga::linToDb(ratio);
    };
    float loM = measure(60.f, 100.f, true);
    float midM = measure(800.f, 100.f, true);
    float hiM = measure(12000.f, 100.f, true);
    float hiS = measure(12000.f, 100.f, false);
    CHECKM(std::abs(loM - 3.0f) < 0.8f, "low mid 100%% = %f (want ~3)", loM);
    CHECKM(std::abs(midM - 3.5f) < 0.8f, "mid mid 100%% = %f (want ~3.5)", midM);
    CHECKM(std::abs(hiM - 1.0f) < 1.2f, "high mid 100%% = %f (want ~1, plus maybe tilt 0 on mid)", hiM);
    CHECKM(hiS > 3.5f && hiS < 7.0f, "high side 100%% = %f (want ~4 + tilt ~1.5)", hiS);

    float loM2 = measure(60.f, 200.f, true);
    CHECKM(loM2 > loM, "200%% low should exceed 100%% (%f vs %f)", loM2, loM);
}

static void test_compressor_curve() {
    const float sr = 48000;
    // Mid-band 1 kHz, mid-only, house/sat/tilt/eq/lim off, amount 100, comp on.
    auto settledGainDb = [&](float amp) {
        const int n = (int)(sr * 2.5f);
        auto s = sine(n, sr, 1000.f, amp);
        ga::Params p;
        p.amount = 100;
        p.houseOn = false;
        p.satOn = false;
        p.tiltOn = false;
        p.limiterOn = false;
        p.limiterDb = 0;
        p.eqOn = false;
        p.compOn = true;
        Run r(sr, 512, 1);
        r.go(s, s, p);
        int lat = r.e.latencySamples();
        int start = lat + (int)(0.8 * sr);
        float inRms = 0, outRms = 0;
        int c = 0;
        for (int i = start; i < n; ++i) {
            float in = s[(size_t)(i - lat)];
            inRms += in * in;
            outRms += r.L[(size_t)i] * r.L[(size_t)i];
            ++c;
        }
        inRms = std::sqrt(inRms / c);
        outRms = std::sqrt(outRms / c);
        return ga::linToDb(outRms / (inRms + 1e-20f));
    };
    // Well below threshold: only makeup (~+2.5 dB)
    float gQuiet = settledGainDb(0.003f); // ~ -50 dBFS peak
    CHECKM(std::abs(gQuiet - 2.5f) < 0.7f, "quiet gain %f (want makeup ~2.5)", gQuiet);

    // Textbook GR at a known level, plus makeup
    // Envelope of a sine of amplitude A is ~A (peak follower). xDb = linToDb(A).
    float A = 0.25f; // -12 dBFS peak, above thresh -22
    float xDb = ga::linToDb(A);
    float gr = ga::softKneeGrDb(xDb, -22.f, 2.5f, 18.f);
    float want = gr + 2.5f;
    float gLoud = settledGainDb(A);
    CHECKM(std::abs(gLoud - want) < 1.5f, "loud gain %f want %f (xDb=%f gr=%f)", gLoud, want, xDb, gr);
    CHECK(gLoud < gQuiet); // more reduction on louder signal
}

static void test_mid_only_detect() {
    const float sr = 48000;
    const int n = (int)sr * 2;
    auto s = sine(n, sr, 1000.f, 0.4f);
    std::vector<float> z(n, 0);
    // Side-only: L = s, R = -s
    ga::Params p;
    p.amount = 100;
    p.houseOn = false;
    p.satOn = false;
    p.tiltOn = false;
    p.limiterOn = false;
    p.limiterDb = 0;
    p.eqOn = false;
    p.compOn = true;
    Run side(sr, 512, 1);
    std::vector<float> Rm(n);
    for (int i = 0; i < n; ++i) Rm[(size_t)i] = -s[(size_t)i];
    side.go(s, Rm, p);
    auto mSide = side.e.meters();
    // Mid-only
    Run mid(sr, 512, 1);
    mid.go(s, s, p);
    auto mMid = mid.e.meters();
    CHECKM(std::abs(mSide.grMid) < 0.35f, "side-only mid GR = %f (want ~0)", mSide.grMid);
    CHECKM(mMid.grMid < -0.5f, "mid-only mid GR = %f (want noticeable)", mMid.grMid);
}

static void test_ceiling() {
    const float sr = 48000;
    const int n = (int)sr;
    std::vector<float> L(n), R(n);
    uint32_t rng = 99;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        // Pathological transients
        float nse = ((float)((rng >> 9) & 0xffff) / 32768.f - 1.f);
        L[(size_t)i] = (i % 113 == 0) ? 1.5f * nse : 0.4f * nse;
        R[(size_t)i] = (i % 91 == 0) ? 1.5f * nse : 0.4f * nse;
    }
    ga::Params p; // default limiter +5, amount 100
    p.outputDb = 6.f; // try to break the ceiling
    Run r(sr, 256, 1);
    r.go(L, R, p, nullptr, nullptr, 256);
    float pk = peakOf(r.L, r.R);
    CHECKM(pk <= ga::ceilingLin() + 1e-4f, "peak %f ceiling %f", pk, ga::ceilingLin());
    CHECK(allFinite(r.L) && allFinite(r.R));

    // True-peak-ish: interpolator on the output
    float tp = pk;
    for (int i = 1; i < n; ++i) {
        tp = std::max(tp, ga::LookaheadLim::truePeak4(r.L[(size_t)(i - 1)], r.L[(size_t)i]));
        tp = std::max(tp, ga::LookaheadLim::truePeak4(r.R[(size_t)(i - 1)], r.R[(size_t)i]));
    }
    CHECKM(tp <= ga::dbToLin(-0.05f), "true-peak %f (%.3f dBTP) over allowed", tp, ga::linToDb(tp));
}

static void test_aliasing() {
    const float sr = 48000;
    const int n = (int)sr;
    auto s = sine(n, sr, 18000.f, 0.7f);
    auto aliasEnergy = [&](int os) {
        ga::Params p;
        p.amount = 200;
        p.houseOn = false;
        p.compOn = false;
        p.tiltOn = false;
        p.limiterOn = false;
        p.limiterDb = 0;
        p.eqOn = false;
        p.satOn = true;
        p.characterOn = true;
        Run r(sr, 512, os);
        r.go(s, s, p);
        // DFT-ish energy below 10 kHz via Goertzel / bin averages (simple block cosine)
        // Correlate against a bank of sines 200 Hz .. 10 kHz
        int lat = r.e.latencySamples();
        int start = lat + 2048;
        double low = 0, high = 0;
        for (int b = 1; b <= 80; ++b) {
            float hz = 200.f * b; // 200..16000
            if (hz > 20000) break;
            double cr = 0, ci = 0;
            int N = n - start;
            double w = 2.0 * M_PI * hz / sr;
            for (int i = start; i < n; ++i) {
                double t = i - start;
                cr += r.L[(size_t)i] * std::cos(w * t);
                ci += r.L[(size_t)i] * std::sin(w * t);
            }
            double e = (cr * cr + ci * ci) / (double)N;
            if (hz < 10000.f) low += e;
            else if (hz > 16000.f) high += e;
        }
        return low;
    };
    double e1 = aliasEnergy(1);
    double e2 = aliasEnergy(2);
    double e4 = aliasEnergy(4);
    CHECKM(e2 < e1 * 0.85, "2x alias %g not below 1x %g", e2, e1);
    CHECKM(e4 < e2 * 1.05, "4x alias %g should not exceed 2x %g much", e4, e2);
    std::printf("  alias energy 1x=%g 2x=%g 4x=%g\n", e1, e2, e4);
}

static void test_lufs() {
    const float sr = 48000;
    const int n = (int)sr * 4;
    // Independent ITU 48 kHz K-weight on a 997 Hz stereo sine
    ga::KWeight kL, kR;
    kL.prepare(sr); kR.prepare(sr);
    auto s = sine(n, sr, 997.f, 0.1f); // -20 dBFS peak
    double acc = 0;
    for (int i = 0; i < n; ++i) {
        float z1 = kL.process(s[(size_t)i]);
        float z2 = kR.process(s[(size_t)i]);
        acc += (double)z1 * z1 + (double)z2 * z2;
    }
    double mean = acc / n;
    float ref = (float)(-0.691 + 10.0 * std::log10(mean));

    ga::Params p;
    p.amount = 0; p.characterOn = false; p.eqOn = false;
    p.limiterOn = false; p.limiterDb = 0;
    Run r(sr, 512, 1);
    r.go(s, s, p);
    float got = r.e.meters().inLufs;
    CHECKM(std::abs(got - ref) < 0.35f, "LUFS got %f ref %f", got, ref);

    // +6 dB
    auto s2 = sine(n, sr, 997.f, 0.2f);
    Run r2(sr, 512, 1);
    r2.go(s2, s2, p);
    float got2 = r2.e.meters().inLufs;
    CHECKM(std::abs((got2 - got) - 6.0f) < 0.25f, "+6 LU: %f -> %f (delta %f)", got, got2, got2 - got);
}

static void test_every_control() {
    const float sr = 48000;
    const int n = (int)(sr * 1.5f);
    // Mixed signal
    std::vector<float> L(n, 0), R(n, 0);
    uint32_t rng = 7;
    for (int i = 0; i < n; ++i) {
        float t = (float)i / sr;
        float kick = (std::fmod(t, 0.5f) < 0.05f) ? 0.7f * std::sin(2 * (float)M_PI * 60 * t) * std::exp(-std::fmod(t, 0.5f) / 0.04f) : 0;
        rng = rng * 1664525u + 1013904223u;
        float hat = ((float)((rng >> 9) & 0xffff) / 32768.f - 1.f) * 0.08f * ((std::fmod(t, 0.25f) < 0.02f) ? 1.f : 0.15f);
        float padL = 0.12f * std::sin(2 * (float)M_PI * 220 * t);
        float padR = 0.12f * std::sin(2 * (float)M_PI * 224 * t);
        L[(size_t)i] = kick + hat + padL;
        R[(size_t)i] = kick + hat * 0.7f + padR;
    }
    auto runP = [&](ga::Params p) {
        Run r(sr, 512, 1);
        r.go(L, R, p);
        return r;
    };
    ga::Params d;
    auto base = runP(d);
    auto diff = [&](const Run& a, const Run& b) {
        double acc = 0; int lat = std::max(a.e.latencySamples(), b.e.latencySamples());
        for (int i = lat; i < n; ++i) {
            float x = a.L[(size_t)i] - b.L[(size_t)i];
            float y = a.R[(size_t)i] - b.R[(size_t)i];
            acc += x * x + y * y;
        }
        return std::sqrt(acc / (2.0 * (n - lat)));
    };
    const float eps = 1e-4f;
    auto checkParam = [&](const char* name, auto setter) {
        ga::Params p = d;
        setter(p);
        auto r = runP(p);
        float dlt = (float)diff(base, r);
        CHECKM(dlt > eps, "%s produced only %g change", name, dlt);
    };
    checkParam("inputDb", [](ga::Params& p){ p.inputDb = 4.f; });
    checkParam("outputDb", [](ga::Params& p){ p.outputDb = -3.f; });
    checkParam("eqHigh", [](ga::Params& p){ p.eqHighDb = 6.f; });
    checkParam("eqMid", [](ga::Params& p){ p.eqMidDb = -6.f; });
    checkParam("eqLow", [](ga::Params& p){ p.eqLowDb = 6.f; });
    
    // eqOff vs default (all 0) may be null — that's correct. Check eqOn toggle with boost:
    {
        ga::Params on = d; on.eqHighDb = 5.f; on.eqOn = true;
        ga::Params off = on; off.eqOn = false;
        auto a = runP(on), b = runP(off);
        CHECKM(diff(a, b) > eps, "eqOn toggle no change");
    }
    checkParam("amount 0", [](ga::Params& p){ p.amount = 0; });
    checkParam("amount 200", [](ga::Params& p){ p.amount = 200; });
    checkParam("characterOff", [](ga::Params& p){ p.characterOn = false; });
    checkParam("limiterDb", [](ga::Params& p){ p.limiterDb = 10.f; });
    checkParam("limiterOff", [](ga::Params& p){ p.limiterOn = false; });
}

static void test_presets_distinct() {
    const float sr = 48000;
    const int n = (int)(sr * 1.5f);
    std::vector<float> L(n, 0), R(n, 0);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / sr;
        float kick = 0.6f * std::sin(2 * (float)M_PI * 55 * t) * std::exp(-std::fmod(t, 0.5f) / 0.05f);
        float mid = 0.15f * std::sin(2 * (float)M_PI * 330 * t);
        float wide = 0.1f * std::sin(2 * (float)M_PI * 1200 * t);
        L[(size_t)i] = kick + mid + wide;
        R[(size_t)i] = kick + mid - 0.6f * wide;
    }
    auto fingerprint = [&](const char* name) {
        Run r(sr, 512, 1);
        r.go(L, R, ga::presetByName(name));
        int lat = r.e.latencySamples();
        double acc = 0, accH = 0;
        for (int i = lat; i < n; ++i) acc += r.L[(size_t)i] * r.L[(size_t)i] + r.R[(size_t)i] * r.R[(size_t)i];
        // crude high-band energy
        float hp = 0, a = 1.f - std::exp(-2.f * (float)M_PI * 6000.f / sr);
        for (int i = lat; i < n; ++i) {
            float x = 0.5f * (r.L[(size_t)i] + r.R[(size_t)i]);
            hp += a * (x - hp);
            float h = x - hp;
            accH += h * h;
        }
        return std::pair<double,double>{acc / (n - lat), accH / (n - lat)};
    };
    auto def = fingerprint("Default");
    int distinct = 0;
    for (int i = 1; i < ga::kNumPresets; ++i) {
        auto f = fingerprint(ga::kPresetNames[i]);
        double rel = std::abs(f.first - def.first) / (def.first + 1e-12);
        double relH = std::abs(f.second - def.second) / (def.second + 1e-12);
        bool ok = (rel > 0.01) || (relH > 0.02);
        CHECKM(ok, "preset %s too close to Default (rms rel %g high rel %g)", ga::kPresetNames[i], rel, relH);
        if (ok) ++distinct;
    }
    CHECK(distinct == ga::kNumPresets - 1);
}

static void test_cpu() {
    const float sr = 48000;
    const int n = (int)sr * 10;
    std::vector<float> L(n), R(n);
    uint32_t rng = 123;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        L[(size_t)i] = 0.3f * ((float)((rng >> 9) & 0xffff) / 32768.f - 1.f);
        R[(size_t)i] = 0.3f * ((float)((rng >> 17) & 0xffff) / 32768.f - 1.f);
    }
    ga::Params p;
    Run r(sr, 512, 1);
    auto t0 = std::chrono::steady_clock::now();
    r.go(L, R, p, nullptr, nullptr, 512);
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    double xrt = 10.0 / sec;
    std::printf("  CPU 10s @ 48 kHz 1x: %.3f s (%.1fx realtime)\n", sec, xrt);
    CHECKM(xrt > 8.0, "too slow: %.2fx realtime", xrt);
    CHECK(allFinite(r.L) && peakOf(r.L, r.R) <= ga::ceilingLin() + 1e-4f);
}

int main() {
    std::printf("GOD ATOM engine tests\n");
    test_soft_knee_math();
    test_eq_textbook();
    test_null_and_bypass();
    test_nan();
    test_latency();
    test_atom_curve();
    test_compressor_curve();
    test_mid_only_detect();
    test_ceiling();
    test_aliasing();
    test_lufs();
    test_every_control();
    test_presets_distinct();
    test_cpu();
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
