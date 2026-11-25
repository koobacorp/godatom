#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioParameterFloat* pf(const juce::String& id, const juce::String& name,
                                     float min, float max, float def, const juce::String& su = {}) {
    juce::NormalisableRange<float> r(min, max);
    return new juce::AudioParameterFloat(juce::ParameterID{id, 1}, name, r, def, su);
}
static juce::AudioParameterBool* pb(const juce::String& id, const juce::String& name, bool def) {
    return new juce::AudioParameterBool(juce::ParameterID{id, 1}, name, def);
}
static juce::AudioParameterChoice* pc(const juce::String& id, const juce::String& name,
                                      const juce::StringArray& choices, int def) {
    return new juce::AudioParameterChoice(juce::ParameterID{id, 1}, name, choices, def);
}

juce::AudioProcessor::BusesProperties GodAtomProcessor::buses() {
    return BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false);
}

juce::AudioProcessorValueTreeState::ParameterLayout GodAtomProcessor::layout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    v.emplace_back(pf("input", "Input", -12.f, 12.f, 0.f, "dB"));
    v.emplace_back(pf("output", "Output", -12.f, 12.f, 0.f, "dB"));
    v.emplace_back(pf("eqHigh", "High", -6.f, 6.f, 0.f, "dB"));
    v.emplace_back(pf("eqMid", "Mid", -6.f, 6.f, 0.f, "dB"));
    v.emplace_back(pf("eqLow", "Low", -6.f, 6.f, 0.f, "dB"));
    v.emplace_back(pf("amount", "Amount", 0.f, 200.f, 100.f, "%"));
    v.emplace_back(pf("limiter", "Limiter", 0.f, 12.f, 5.f, "dB"));
    v.emplace_back(pb("eqOn", "EQ On", true));
    v.emplace_back(pb("characterOn", "Amount On", true));
    v.emplace_back(pb("limiterOn", "Limiter On", true));
    v.emplace_back(pb("extSC", "Sidechain Ext", false));
    v.emplace_back(pc("oversample", "Oversample", {"1x", "2x", "4x", "8x"}, 0));
    v.emplace_back(pc("meterMode", "Meter", {"RMS + Peak", "Short-term LUFS + Peak"}, 0));
    v.emplace_back(pb("animation", "Animation", true));
    v.emplace_back(pb("autoGain", "Auto-gain compare", true));
    v.emplace_back(pc("uiScale", "UI Scale", {"75%", "100%", "125%", "150%", "200%"}, 1));
    return {v.begin(), v.end()};
}

GodAtomProcessor::GodAtomProcessor()
    : AudioProcessor(buses()),
      apvts(*this, nullptr, "GODATOM", layout()) {}

bool GodAtomProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto mainIn = l.getChannelSet(true, 0);
    auto mainOut = l.getChannelSet(false, 0);
    if (mainIn != juce::AudioChannelSet::stereo()) return false;
    if (mainOut != juce::AudioChannelSet::stereo()) return false;
    if (l.inputBuses.size() > 1) {
        auto sc = l.getChannelSet(true, 1);
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void GodAtomProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sr = sampleRate;
    block = samplesPerBlock;
    auto p = readParams();
    lastOs = p.oversample;
    engine.prepare((float)sr, std::max(16, block), lastOs);
    lastLatency = engine.latencySamples();
    setLatencySamples(lastLatency);
}

void GodAtomProcessor::reset() { engine.reset(); meters = {}; }

void GodAtomProcessor::syncLatency() {
    auto p = readParams();
    if (p.oversample != lastOs) {
        lastOs = p.oversample;
        engine.prepare((float)sr, std::max(16, block), lastOs);
    }
    int lat = engine.latencySamples();
    if (lat != lastLatency) {
        lastLatency = lat;
        setLatencySamples(lat);
    }
}

ga::Params GodAtomProcessor::readParams() const {
    auto v = [&](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    ga::Params p;
    p.inputDb = v("input");
    p.outputDb = v("output");
    p.eqHighDb = v("eqHigh");
    p.eqMidDb = v("eqMid");
    p.eqLowDb = v("eqLow");
    p.amount = v("amount");
    p.limiterDb = v("limiter");
    p.eqOn = v("eqOn") >= 0.5f;
    p.characterOn = v("characterOn") >= 0.5f;
    p.limiterOn = v("limiterOn") >= 0.5f;
    p.extSidechain = v("extSC") >= 0.5f;
    int osIdx = (int)std::lround(v("oversample"));
    p.oversample = (osIdx == 1 ? 2 : osIdx == 2 ? 4 : osIdx == 3 ? 8 : 1);
    p.meterMode = (int)std::lround(v("meterMode"));
    p.animation = v("animation") >= 0.5f;
    p.autoGain = v("autoGain") >= 0.5f;
    return p;
}

void GodAtomProcessor::applyParams(const ga::Params& p) {
    auto setF = [&](const char* id, float x) {
        if (auto* par = apvts.getParameter(id)) {
            par->beginChangeGesture();
            par->setValueNotifyingHost(par->convertTo0to1(x));
            par->endChangeGesture();
        }
    };
    auto setB = [&](const char* id, bool x) {
        if (auto* par = apvts.getParameter(id)) {
            par->beginChangeGesture();
            par->setValueNotifyingHost(x ? 1.f : 0.f);
            par->endChangeGesture();
        }
    };
    auto setC = [&](const char* id, int idx) {
        if (auto* par = apvts.getParameter(id)) {
            int n = par->getNumSteps();
            float v01 = (n > 1) ? (float)idx / (float)(n) : 0.f;
            // AudioParameterChoice uses equally spaced 0..1
            par->beginChangeGesture();
            par->setValueNotifyingHost(par->convertTo0to1((float)idx));
            par->endChangeGesture();
        }
    };
    setF("input", p.inputDb);
    setF("output", p.outputDb);
    setF("eqHigh", p.eqHighDb);
    setF("eqMid", p.eqMidDb);
    setF("eqLow", p.eqLowDb);
    setF("amount", p.amount);
    setF("limiter", p.limiterDb);
    setB("eqOn", p.eqOn);
    setB("characterOn", p.characterOn);
    setB("limiterOn", p.limiterOn);
    setB("extSC", p.extSidechain);
    int osIdx = (p.oversample == 2 ? 1 : p.oversample == 4 ? 2 : p.oversample == 8 ? 3 : 0);
    setC("oversample", osIdx);
    setC("meterMode", p.meterMode);
    setB("animation", p.animation);
    setB("autoGain", p.autoGain);
}

void GodAtomProcessor::applyPreset(int index) {
    index = juce::jlimit(0, ga::kNumPresets - 1, index);
    program = index;
    userPresetName.clear();
    applyParams(ga::presetByName(ga::kPresetNames[index]));
}

void GodAtomProcessor::setCurrentProgram(int i) { applyPreset(i); }
const juce::String GodAtomProcessor::getProgramName(int i) {
    if (i >= 0 && i < ga::kNumPresets) return ga::kPresetNames[i];
    return {};
}

juce::File GodAtomProcessor::userPresetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library")
        .getChildFile("Audio")
        .getChildFile("Presets")
        .getChildFile("GOD ATOM");
}

juce::String GodAtomProcessor::sanitizePresetName(const juce::String& name) {
    auto s = name.trim();
    juce::String out;
    for (int i = 0; i < s.length() && out.length() < 64; ++i) {
        auto c = s[i];
        if (juce::CharacterFunctions::isLetterOrDigit(c) || c == ' ' || c == '-' || c == '_' || c == '.')
            out += c;
    }
    return out.trim();
}

juce::StringArray GodAtomProcessor::listUserPresets() {
    juce::StringArray names;
    auto dir = userPresetDirectory();
    if (!dir.isDirectory()) return names;
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        names.add(f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool GodAtomProcessor::saveUserPreset(const juce::String& name) {
    auto safe = sanitizePresetName(name);
    if (safe.isEmpty()) return false;
    auto dir = userPresetDirectory();
    if (!dir.createDirectory() && !dir.isDirectory()) return false;
    auto xml = apvts.copyState().createXml();
    if (!xml) return false;
    if (!xml->writeTo(dir.getChildFile(safe + ".xml"))) return false;
    userPresetName = safe;
    return true;
}

bool GodAtomProcessor::loadUserPreset(const juce::String& name) {
    auto safe = sanitizePresetName(name);
    if (safe.isEmpty()) return false;
    auto f = userPresetDirectory().getChildFile(safe + ".xml");
    if (!f.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse(f);
    if (!xml) return false;
    if (!xml->hasTagName(apvts.state.getType())) return false;
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    userPresetName = safe;
    return true;
}

void GodAtomProcessor::storeSlot(int slot) {
    juce::MemoryBlock mb;
    getStateInformation(mb);
    if (slot == 0) slotA = mb; else slotB = mb;
    abSlot = slot;
}
void GodAtomProcessor::recallSlot(int slot) {
    const auto& mb = (slot == 0 ? slotA : slotB);
    if (mb.getSize() > 0) setStateInformation(mb.getData(), (int)mb.getSize());
    abSlot = slot;
}
void GodAtomProcessor::copyAtoB() {
    if (abSlot == 0) storeSlot(0);
    slotB = slotA;
}

void GodAtomProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    syncLatency();
    auto p = readParams();
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n <= 0 || nCh < 2) return;

    // Clear extra outs
    for (int c = 2; c < nCh; ++c) buffer.clear(c, 0, n);

    const float* scL = nullptr;
    const float* scR = nullptr;
    if (p.extSidechain && getBusCount(true) > 1) {
        auto sc = getBusBuffer(buffer, true, 1);
        if (sc.getNumChannels() >= 2 && sc.getNumSamples() >= n) {
            scL = sc.getReadPointer(0);
            scR = sc.getReadPointer(1);
        }
    }
    // Main bus is bus 0. JUCE packs all buses into `buffer`. Use getBusBuffer for main.
    auto main = getBusBuffer(buffer, false, 0);
    // Input is also on the same channels for a standard layout
    auto inBuf = getBusBuffer(buffer, true, 0);
    // in-place: main out shares storage with main in
    float* L = main.getWritePointer(0);
    float* R = main.getWritePointer(1);
    if (inBuf.getNumChannels() >= 2 && inBuf.getWritePointer(0) != L) {
        // Copy in to out if not in-place (rare)
        juce::FloatVectorOperations::copy(L, inBuf.getReadPointer(0), n);
        juce::FloatVectorOperations::copy(R, inBuf.getReadPointer(1), n);
    }
    engine.process(L, R, scL, scR, n, p);
    meters = engine.meters();

    if (p.autoGain) {
        float inL = meters.inLufs, outL = meters.outLufs;
        if (inL > -80.f && outL > -80.f)
            autoGain.store(inL - outL, std::memory_order_relaxed);
    } else {
        autoGain.store(0.f, std::memory_order_relaxed);
    }
}

void GodAtomProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    syncLatency();
    auto p = readParams();
    p.bypass = true;
    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() < 2) return;
    auto main = getBusBuffer(buffer, false, 0);
    engine.process(main.getWritePointer(0), main.getWritePointer(1), nullptr, nullptr, n, p);
    meters = engine.meters();
}

void GodAtomProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}
void GodAtomProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* GodAtomProcessor::createEditor() { return new GodAtomEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GodAtomProcessor(); }
