#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginEditor.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

static int passed = 0, failed = 0;
#define CHECK(x) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++passed; else { ++failed; std::printf("FAIL line %d: %s | ", __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void writeWav(const juce::File& file, const std::vector<float>& L, const std::vector<float>& R, double sr) {
    juce::WavAudioFormat fmt;
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    auto out = file.createOutputStream();
    if (!out) return;
    int n = (int)L.size();
    juce::AudioBuffer<float> buf(2, n);
    if (n > 0) {
        buf.copyFrom(0, 0, L.data(), n);
        buf.copyFrom(1, 0, R.data(), n);
    }
    if (auto w = fmt.createWriterFor(out.get(), sr, 2, 24, {}, 0)) {
        out.release();
        w->writeFromAudioSampleBuffer(buf, 0, n);
        delete w;
    }
}

static void makeMix(std::vector<float>& L, std::vector<float>& R, int n, double sr) {
    L.assign((size_t)n, 0); R.assign((size_t)n, 0);
    uint32_t rng = 0x51C0u;
    for (int i = 0; i < n; ++i) {
        float t = (float)(i / sr);
        float kick = 0.7f * std::sin(2.f * (float)M_PI * 58.f * t) * std::exp(-std::fmod(t, 0.5f) / 0.05f);
        rng = rng * 1664525u + 1013904223u;
        float hat = ((float)((rng >> 9) & 0xffff) / 32768.f - 1.f) * 0.09f
                    * (std::fmod(t, 0.25f) < 0.03f ? 1.f : 0.12f);
        float mid = 0.14f * std::sin(2.f * (float)M_PI * 329.6f * t);
        float wide = 0.10f * std::sin(2.f * (float)M_PI * 1174.f * t);
        L[(size_t)i] = juce::jlimit(-1.f, 1.f, kick + hat + mid + wide);
        R[(size_t)i] = juce::jlimit(-1.f, 1.f, kick + hat * 0.7f + mid - 0.7f * wide);
    }
}

struct Rig {
    GodAtomProcessor p;
    double sr; int block;
    Rig(double rate = 48000, int bs = 256) : sr(rate), block(bs) {
        p.setRateAndBufferSizeDetails(sr, block);
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        p.setBusesLayout(layout);
        p.prepareToPlay(sr, block);
    }
    double run(const std::vector<float>& inL, const std::vector<float>& inR,
               std::vector<float>& outL, std::vector<float>& outR) {
        int n = (int)inL.size(), pos = 0, nCh = std::max(2, p.getTotalNumOutputChannels());
        outL.clear(); outR.clear();
        juce::AudioBuffer<float> buf(nCh, block);
        juce::MidiBuffer midi;
        double t = 0;
        while (pos < n) {
            int m = std::min(block, n - pos);
            buf.setSize(nCh, m, false, false, true);
            buf.clear();
            for (int i = 0; i < m; ++i) {
                buf.setSample(0, i, inL[(size_t)(pos + i)]);
                buf.setSample(1, i, inR[(size_t)(pos + i)]);
            }
            auto t0 = std::chrono::steady_clock::now();
            p.processBlock(buf, midi);
            t += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            outL.insert(outL.end(), buf.getReadPointer(0), buf.getReadPointer(0) + m);
            outR.insert(outR.end(), buf.getReadPointer(1), buf.getReadPointer(1) + m);
            pos += m;
        }
        return t;
    }
};

static bool shot(juce::Component& c, const juce::File& f) {
    c.addToDesktop(0);
    c.setVisible(true);
    c.repaint();
    juce::Thread::sleep(60);
    auto img = c.createComponentSnapshot(c.getLocalBounds(), true, 1.0f);
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    auto s = f.createOutputStream();
    bool ok = s && s->openedOk() && juce::PNGImageFormat().writeImageToStream(img, *s);
    c.removeFromDesktop();
    return ok && img.getWidth() > 10 && img.getHeight() > 10;
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf("GOD ATOM plugin tests\n");

    juce::File diag = juce::File("/workspace/mastering/diag");
    juce::File shotsDir = juce::File::getCurrentWorkingDirectory();

    {
        Rig r;
        CHECK(r.p.reportedLatency() == ga::kLookaheadHost);
        CHECK(r.p.getLatencySamples() == r.p.reportedLatency());
        CHECK(r.p.getName() == "GOD ATOM");
    }

    const double sr = 48000;
    const int n = (int)(sr * 4);
    std::vector<float> inL, inR, outL, outR;
    makeMix(inL, inR, n, sr);

    // Default process + ceiling
    {
        Rig r(sr, 256);
        r.p.applyPreset(0);
        r.run(inL, inR, outL, outR);
        float pk = 0;
        bool finite = true;
        for (int i = 0; i < n; ++i) {
            pk = std::max(pk, std::max(std::abs(outL[(size_t)i]), std::abs(outR[(size_t)i])));
            if (!std::isfinite(outL[(size_t)i]) || !std::isfinite(outR[(size_t)i])) finite = false;
        }
        CHECK(finite);
        CHECKM(pk <= ga::ceilingLin() + 1e-4f, "plugin peak %f", pk);
        CHECK(r.p.getLatencySamples() == ga::kLookaheadHost);
    }

    // Bypass null vs amount 0 / limiter off
    {
        Rig a(sr, 256), b(sr, 256);
        ga::Params u; u.amount = 0; u.characterOn = false; u.eqOn = false;
        u.limiterOn = false; u.limiterDb = 0;
        a.p.applyParams(u);
        b.p.applyParams(u);
        std::vector<float> A, Ar, B, Br;
        a.run(inL, inR, A, Ar);
        // host bypass
        juce::AudioBuffer<float> buf(2, 256);
        juce::MidiBuffer midi;
        B = inL; Br = inR;
        for (int pos = 0; pos < n; ) {
            int m = std::min(256, n - pos);
            buf.setSize(2, m, false, false, true);
            for (int i = 0; i < m; ++i) {
                buf.setSample(0, i, inL[(size_t)(pos + i)]);
                buf.setSample(1, i, inR[(size_t)(pos + i)]);
            }
            b.p.processBlockBypassed(buf, midi);
            for (int i = 0; i < m; ++i) {
                B[(size_t)(pos + i)] = buf.getSample(0, i);
                Br[(size_t)(pos + i)] = buf.getSample(1, i);
            }
            pos += m;
        }
        int mis = 0;
        for (int i = 0; i < n; ++i)
            if (A[(size_t)i] != B[(size_t)i] || Ar[(size_t)i] != Br[(size_t)i]) ++mis;
        CHECKM(mis == 0, "bypass vs unity mismatches %d", mis);
    }

    // Every major control changes the output through the processor
    {
        auto runP = [&](auto setter) {
            Rig r(sr, 256);
            ga::Params p; setter(p);
            r.p.applyParams(p);
            std::vector<float> L, R;
            r.run(inL, inR, L, R);
            return L;
        };
        auto d = runP([](ga::Params&){});
        auto chk = [&](const char* name, auto s) {
            auto x = runP(s);
            double acc = 0;
            int lat = ga::kLookaheadHost;
            for (int i = lat; i < n; ++i) { float v = x[(size_t)i] - d[(size_t)i]; acc += v * v; }
            float rms = (float)std::sqrt(acc / (n - lat));
            CHECKM(rms > 1e-4f, "%s change %g", name, rms);
        };
        chk("amount 200", [](ga::Params& p){ p.amount = 200; });
        chk("eq high", [](ga::Params& p){ p.eqHighDb = 6; });
        chk("limiter 10", [](ga::Params& p){ p.limiterDb = 10; });
        chk("input +4", [](ga::Params& p){ p.inputDb = 4; });
    }

    // State round-trip
    {
        Rig r(sr, 256);
        r.p.applyPreset(3); // Late Night
        juce::MemoryBlock mb;
        r.p.getStateInformation(mb);
        r.p.applyPreset(0);
        r.p.setStateInformation(mb.getData(), (int)mb.getSize());
        auto p = r.p.readParams();
        CHECKM(std::abs(p.amount - 130.f) < 0.5f, "state amount %f", p.amount);
    }

    // User presets on disk
    {
        Rig r(sr, 256);
        const juce::String pname { "TestAtomUser" };
        auto file = GodAtomProcessor::userPresetDirectory().getChildFile(pname + ".xml");
        file.deleteFile();
        CHECK(GodAtomProcessor::sanitizePresetName(" My Cool-Preset_1 ").compare("My Cool-Preset_1") == 0);
        CHECK(GodAtomProcessor::sanitizePresetName("foo/../bar").compare("foo..bar") == 0);
        CHECK(!r.p.saveUserPreset(""));
        CHECK(!r.p.saveUserPreset("   "));
        r.p.applyParams([]{ ga::Params p; p.amount = 173; p.eqLowDb = 4.5f; p.limiterDb = 7; return p; }());
        CHECK(r.p.saveUserPreset(pname));
        CHECK(file.existsAsFile());
        CHECK(GodAtomProcessor::listUserPresets().contains(pname));
        r.p.applyPreset(0);
        CHECK(r.p.currentUserPreset().isEmpty());
        CHECKM(std::abs(r.p.readParams().amount - 100.f) < 0.5f, "reset amount %f", r.p.readParams().amount);
        CHECK(r.p.loadUserPreset(pname));
        CHECK(r.p.currentUserPreset() == pname);
        auto loaded = r.p.readParams();
        CHECKM(std::abs(loaded.amount - 173.f) < 0.5f, "loaded amount %f", loaded.amount);
        CHECKM(std::abs(loaded.eqLowDb - 4.5f) < 0.1f, "loaded eqLow %f", loaded.eqLowDb);
        CHECKM(std::abs(loaded.limiterDb - 7.f) < 0.1f, "loaded limiter %f", loaded.limiterDb);
        auto* ed = dynamic_cast<GodAtomEditor*>(r.p.createEditor());
        CHECK(ed != nullptr);
        if (ed) {
            ed->refreshPresetMenu();
            CHECK(ed->presetMenuContains(pname));
            delete ed;
        }
        CHECK(file.deleteFile());
    }

    // Screenshots
    {
        Rig r(sr, 256);
        // pump some audio so meters move
        std::vector<float> L, R;
        r.run(inL, inR, L, R);
        auto* ed = dynamic_cast<GodAtomEditor*>(r.p.createEditor());
        CHECK(ed != nullptr);
        if (ed) {
            auto setScale = [&](int idx) {
                if (auto* par = r.p.apvts.getParameter("uiScale"))
                    par->setValueNotifyingHost(par->convertTo0to1((float)idx));
                ed->applyScaleFromParam();
            };
            r.p.applyPreset(0);
            juce::Thread::sleep(60);
            CHECK(shot(*ed, shotsDir.getChildFile("default.png")));
            CHECK(shot(*ed, diag.getChildFile("ui-default.png")));

            setScale(0); // 75%
            juce::Thread::sleep(60);
            CHECK(shot(*ed, diag.getChildFile("ui-default-75.png")));
            setScale(4); // 200%
            juce::Thread::sleep(60);
            CHECK(shot(*ed, diag.getChildFile("ui-default-200.png")));
            setScale(1); // back to 100%

            r.p.applyParams([]{ ga::Params p; p.amount = 0; return p; }());
            juce::Thread::sleep(60);
            CHECK(shot(*ed, diag.getChildFile("ui-amount-0.png")));

            r.p.applyParams([]{ ga::Params p; p.amount = 200; p.limiterDb = 8; return p; }());
            juce::Thread::sleep(60);
            CHECK(shot(*ed, diag.getChildFile("ui-amount-200.png")));

            r.p.applyPreset(2); // Ambient
            juce::Thread::sleep(60);
            CHECK(shot(*ed, diag.getChildFile("ui-ambient.png")));

            r.p.applyPreset(3);
            juce::Thread::sleep(60);
            CHECK(shot(*ed, diag.getChildFile("ui-late-night.png")));

            r.p.showSettings = true;
            ed->resized();
            juce::Thread::sleep(60);
            CHECK(shot(*ed, diag.getChildFile("ui-settings.png")));
            r.p.showSettings = false;

            delete ed;
        }
    }

    // Level-matched audio demos
    {
        Rig dryR(sr, 256), wetR(sr, 256);
        ga::Params dryP; dryP.amount = 0; dryP.characterOn = false; dryP.eqOn = false;
        dryP.limiterOn = false; dryP.limiterDb = 0;
        dryR.p.applyParams(dryP);
        wetR.p.applyPreset(0);
        std::vector<float> dryL, dryRr, wetL, wetRr;
        dryR.run(inL, inR, dryL, dryRr);
        wetR.run(inL, inR, wetL, wetRr);

        writeWav(diag.getChildFile("demo-dry.wav"), dryL, dryRr, sr);
        writeWav(diag.getChildFile("demo-default.wav"), wetL, wetRr, sr);

        // LUFS match wet to dry
        ga::LufsMeter ld, lw;
        ld.prepare((float)sr, 3.f); lw.prepare((float)sr, 3.f);
        int lat = ga::kLookaheadHost;
        for (int i = lat; i < n; ++i) {
            ld.push(dryL[(size_t)i], dryRr[(size_t)i]);
            lw.push(wetL[(size_t)i], wetRr[(size_t)i]);
        }
        float g = ga::dbToLin(ld.shortTerm() - lw.shortTerm());
        auto mL = wetL, mR = wetRr;
        for (int i = 0; i < n; ++i) { mL[(size_t)i] *= g; mR[(size_t)i] *= g; }
        writeWav(diag.getChildFile("demo-default-matched.wav"), mL, mR, sr);

        // Ambient matched
        Rig amb(sr, 256);
        amb.p.applyPreset(2);
        std::vector<float> aL, aR;
        amb.run(inL, inR, aL, aR);
        writeWav(diag.getChildFile("demo-ambient.wav"), aL, aR, sr);
        ga::LufsMeter la; la.prepare((float)sr, 3.f);
        for (int i = lat; i < n; ++i) la.push(aL[(size_t)i], aR[(size_t)i]);
        float ga2 = ga::dbToLin(ld.shortTerm() - la.shortTerm());
        for (int i = 0; i < n; ++i) { aL[(size_t)i] *= ga2; aR[(size_t)i] *= ga2; }
        writeWav(diag.getChildFile("demo-ambient-matched.wav"), aL, aR, sr);

        std::printf("  demos written, match gain default=%.2f dB ambient=%.2f dB\n",
                    ld.shortTerm() - lw.shortTerm(), ld.shortTerm() - la.shortTerm());
        CHECK(diag.getChildFile("demo-dry.wav").getSize() > 1000);
        CHECK(diag.getChildFile("demo-default.wav").getSize() > 1000);
        CHECK(diag.getChildFile("demo-default-matched.wav").getSize() > 1000);
    }

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
