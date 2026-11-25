#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class GodAtomProcessor : public juce::AudioProcessor {
public:
    GodAtomProcessor();
    ~GodAtomProcessor() override = default;

    static BusesProperties buses();
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "GOD ATOM"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return ga::kNumPresets; }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int i) override;
    const juce::String getProgramName(int i) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    ga::Params readParams() const;
    void applyParams(const ga::Params& p);
    void applyPreset(int index);

    static juce::File userPresetDirectory();
    static juce::String sanitizePresetName(const juce::String& name);
    static juce::StringArray listUserPresets();
    bool saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::String& name);
    juce::String currentUserPreset() const { return userPresetName; }

    ga::Meters currentMeters() const { return meters; }
    void resetMeters() { engine.resetPeaks(); meters = {}; }
    int reportedLatency() const { return getLatencySamples(); }
    float autoGainDb() const { return autoGain.load(std::memory_order_relaxed); }

    // A/B
    void storeSlot(int slot);
    void recallSlot(int slot);
    int activeSlot() const { return abSlot; }
    void copyAtoB();

    std::atomic<bool> showSettings { false };

private:
    void syncLatency();
    ga::Engine engine;
    ga::Meters meters;
    int program = 0;
    int lastOs = 1;
    int lastLatency = 0;
    double sr = 48000;
    int block = 512;
    juce::MemoryBlock slotA, slotB;
    int abSlot = 0;
    std::atomic<float> autoGain { 0 };
    juce::String userPresetName;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GodAtomProcessor)
};
