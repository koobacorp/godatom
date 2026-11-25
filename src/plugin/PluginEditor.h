#pragma once
#include "PluginProcessor.h"

class SilverLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

class GodAtomEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GodAtomEditor(GodAtomProcessor&);
    ~GodAtomEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void applyScaleFromParam();
    void refreshPresetMenu();
    bool presetMenuContains(const juce::String& name) const;

private:
    void timerCallback() override;
    void showSavePrompt();
    void hideSavePrompt();
    void commitSave();
    float S(float v) const { return v * scale; }
    juce::Rectangle<float> SR(float x, float y, float w, float h) const {
        return { S(x), S(y), S(w), S(h) };
    }
    void drawMeters(juce::Graphics&, juce::Rectangle<float> area, float pkL, float pkR,
                    float aL, float aR, bool inputSide, bool lufs);
    void drawGr(juce::Graphics&, juce::Rectangle<float> area, const ga::Meters&);
    void drawGalaxy(juce::Graphics&, juce::Rectangle<float> area, float amount, float level);
    void drawChassis(juce::Graphics&);
    void drawSettings(juce::Graphics&);

    GodAtomProcessor& proc;
    SilverLookAndFeel lnf;
    float scale = 1.0f;

    juce::Slider inKnob, outKnob, limKnob, eqHigh, eqMid, eqLow, amountKnob;
    juce::ToggleButton eqOn, charOn, limOn, scInt, scExt, btnA, btnB, animBtn, autoBtn;
    juce::ComboBox presetBox, scaleBox, osBox, meterBox;
    juce::TextButton settingsBtn { "Settings" }, saveBtn { "Save" }, saveOk { "OK" };
    juce::TextEditor saveName;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> aIn, aOut, aLim, aEh, aEm, aEl, aAmt;
    std::unique_ptr<BA> aEqOn, aCharOn, aLimOn, aExt, aAnim, aAuto;
    std::unique_ptr<CA> aScale, aOs, aMeter;

    struct Particle { float x, y, vx, vy, r, hue, a; };
    std::vector<Particle> parts;
    uint32_t rng = 0xC0FFEEu;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GodAtomEditor)
};
