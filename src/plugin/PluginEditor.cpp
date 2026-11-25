#include "PluginEditor.h"

namespace col {
const juce::Colour display { 0xff010001 };
const juce::Colour chassis { 0xff433d3e };
const juce::Colour chassisHi { 0xff524e4f };
const juce::Colour label { 0xffc8c4c5 };
const juce::Colour mint { 0xff8cd4be };
const juce::Colour meter { 0xff1a7244 };
const juce::Colour meterTrack { 0xff07130d };
const juce::Colour knob { 0xffb8b5b7 };
const juce::Colour knobHi { 0xffece9ec };
const juce::Colour clip { 0xffe23b3b };
const juce::Colour topBar { 0xff1a1819 };
}

static constexpr int kBaseW = 1040;
static constexpr int kBaseH = 430;

void SilverLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float start, float end, juce::Slider&) {
    auto r = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(w * 0.06f);
    auto cx = r.getCentreX(), cy = r.getCentreY(), rad = r.getWidth() * 0.5f;
    juce::ColourGradient grad(col::knobHi, cx - rad * 0.45f, cy - rad * 0.55f,
                              juce::Colour(0xff6e6b6e), cx + rad * 0.5f, cy + rad * 0.6f, false);
    g.setGradientFill(grad);
    g.fillEllipse(r);
    g.setColour(juce::Colour(0xff3a3a3c));
    g.drawEllipse(r, juce::jmax(1.0f, rad * 0.04f));
    g.setColour(juce::Colours::white.withAlpha(0.28f));
    g.drawEllipse(r.reduced(rad * 0.08f), 1.0f);
    float ang = start + pos * (end - start);
    float ix = cx + (rad * 0.78f) * std::sin(ang);
    float iy = cy - (rad * 0.78f) * std::cos(ang);
    float ox = cx + (rad * 0.28f) * std::sin(ang);
    float oy = cy - (rad * 0.28f) * std::cos(ang);
    g.setColour(juce::Colour(0xff1c1c1e));
    g.drawLine(ox, oy, ix, iy, juce::jmax(1.4f, rad * 0.035f));
}

void SilverLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool hi, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    bool on = b.getToggleState();
    auto fill = on ? col::mint : juce::Colour(0xff2a2a2c);
    if (down) fill = fill.darker(0.15f);
    if (hi && !on) fill = fill.brighter(0.15f);
    g.setColour(fill);
    g.fillRoundedRectangle(r, juce::jmin(6.0f, r.getHeight() * 0.35f));
    if (!on) {
        g.setColour(col::mint.withAlpha(0.55f));
        g.drawRoundedRectangle(r, juce::jmin(6.0f, r.getHeight() * 0.35f), 1.0f);
    }
    // power symbol for buttons whose name contains On or is empty-ish
    auto name = b.getButtonText();
    if (name.isEmpty()) {
        auto c = r.getCentre();
        float s = juce::jmin(r.getWidth(), r.getHeight()) * 0.22f;
        g.setColour(on ? col::display : col::mint);
        g.drawEllipse(c.x - s, c.y - s + 1.0f, s * 2, s * 2, 1.7f);
        g.drawLine(c.x, c.y - s - 1.0f, c.x, c.y, 1.8f);
    } else {
        g.setColour(on ? col::display : col::mint);
        g.setFont(juce::FontOptions((float)juce::jmax(10, (int)(r.getHeight() * 0.42f))));
        g.drawFittedText(name, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

juce::Font SilverLookAndFeel::getTextButtonFont(juce::TextButton&, int h) {
    return juce::FontOptions((float)juce::jmax(11, h / 2));
}
juce::Font SilverLookAndFeel::getComboBoxFont(juce::ComboBox&) { return juce::FontOptions(13.0f); }
juce::Font SilverLookAndFeel::getPopupMenuFont() { return juce::FontOptions(14.0f); }

static void styleKnob(juce::Slider& s) {
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    s.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                          juce::MathConstants<float>::pi * 2.8f, true);
}

GodAtomEditor::GodAtomEditor(GodAtomProcessor& p)
    : juce::AudioProcessorEditor(p), proc(p) {
    setLookAndFeel(&lnf);
    setOpaque(true);
    setResizable(true, true);
    if (auto* c = getConstrainer()) {
        c->setFixedAspectRatio((double)kBaseW / (double)kBaseH);
        c->setMinimumSize(kBaseW * 3 / 4, kBaseH * 3 / 4);
        c->setMaximumSize(kBaseW * 2, kBaseH * 2);
    }

    auto addK = [&](juce::Slider& s) { styleKnob(s); addAndMakeVisible(s); };
    addK(inKnob); addK(outKnob); addK(limKnob);
    addK(eqHigh); addK(eqMid); addK(eqLow); addK(amountKnob);

    auto addT = [&](juce::ToggleButton& b, const juce::String& t) {
        b.setButtonText(t); addAndMakeVisible(b);
    };
    addT(eqOn, {}); addT(charOn, {}); addT(limOn, {});
    addT(scInt, "INT"); addT(scExt, "EXT");
    addT(btnA, "A"); addT(btnB, "B");
    addT(animBtn, "Animation"); addT(autoBtn, "Auto-gain");

    presetBox.addItemList(juce::StringArray(ga::kPresetNames, ga::kNumPresets), 1);
    presetBox.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] {
        int i = presetBox.getSelectedId() - 1;
        if (i >= 0) proc.applyPreset(i);
    };
    addAndMakeVisible(presetBox);

    scaleBox.addItemList({ "75%", "100%", "125%", "150%", "200%" }, 1);
    osBox.addItemList({ "1x", "2x", "4x", "8x" }, 1);
    meterBox.addItemList({ "RMS + Peak", "Short-term LUFS + Peak" }, 1);
    addAndMakeVisible(scaleBox);
    addAndMakeVisible(osBox);
    addAndMakeVisible(meterBox);

    settingsBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2c));
    saveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2c));
    addAndMakeVisible(settingsBtn);
    addAndMakeVisible(saveBtn);
    settingsBtn.onClick = [this] {
        proc.showSettings = !proc.showSettings.load();
        osBox.setVisible(proc.showSettings);
        meterBox.setVisible(proc.showSettings);
        animBtn.setVisible(proc.showSettings);
        autoBtn.setVisible(proc.showSettings);
        repaint();
    };
    saveBtn.onClick = [this] { /* user presets stay in host session state via A/B + DAW save */ };

    btnA.onClick = [this] {
        proc.storeSlot(proc.activeSlot());
        proc.recallSlot(0);
        btnA.setToggleState(true, juce::dontSendNotification);
        btnB.setToggleState(false, juce::dontSendNotification);
    };
    btnB.onClick = [this] {
        proc.storeSlot(proc.activeSlot());
        if (/* empty B first time */ true) proc.recallSlot(1);
        btnA.setToggleState(false, juce::dontSendNotification);
        btnB.setToggleState(true, juce::dontSendNotification);
    };
    btnA.setClickingTogglesState(true);
    btnB.setClickingTogglesState(true);
    btnA.setToggleState(true, juce::dontSendNotification);

    // INT/EXT are mutually exclusive; bound to extSC
    scInt.setClickingTogglesState(true);
    scExt.setClickingTogglesState(true);
    scInt.onClick = [this] {
        if (auto* p = proc.apvts.getParameter("extSC")) {
            p->beginChangeGesture(); p->setValueNotifyingHost(0.f); p->endChangeGesture();
        }
        scInt.setToggleState(true, juce::dontSendNotification);
        scExt.setToggleState(false, juce::dontSendNotification);
    };
    scExt.onClick = [this] {
        if (auto* p = proc.apvts.getParameter("extSC")) {
            p->beginChangeGesture(); p->setValueNotifyingHost(1.f); p->endChangeGesture();
        }
        scInt.setToggleState(false, juce::dontSendNotification);
        scExt.setToggleState(true, juce::dontSendNotification);
    };

    auto& s = proc.apvts;
    aIn  = std::make_unique<SA>(s, "input", inKnob);
    aOut = std::make_unique<SA>(s, "output", outKnob);
    aLim = std::make_unique<SA>(s, "limiter", limKnob);
    aEh  = std::make_unique<SA>(s, "eqHigh", eqHigh);
    aEm  = std::make_unique<SA>(s, "eqMid", eqMid);
    aEl  = std::make_unique<SA>(s, "eqLow", eqLow);
    aAmt = std::make_unique<SA>(s, "amount", amountKnob);
    aEqOn = std::make_unique<BA>(s, "eqOn", eqOn);
    aCharOn = std::make_unique<BA>(s, "characterOn", charOn);
    aLimOn = std::make_unique<BA>(s, "limiterOn", limOn);
    aAnim = std::make_unique<BA>(s, "animation", animBtn);
    aAuto = std::make_unique<BA>(s, "autoGain", autoBtn);
    aScale = std::make_unique<CA>(s, "uiScale", scaleBox);
    aOs = std::make_unique<CA>(s, "oversample", osBox);
    aMeter = std::make_unique<CA>(s, "meterMode", meterBox);

    osBox.setVisible(false);
    meterBox.setVisible(false);
    animBtn.setVisible(false);
    autoBtn.setVisible(false);

    scaleBox.onChange = [this] { applyScaleFromParam(); };

    // particles
    parts.resize(220);
    for (auto& pt : parts) {
        rng = rng * 1664525u + 1013904223u;
        pt.x = (float)((rng >> 8) & 0xffff) / 65535.f;
        pt.y = (float)((rng >> 16) & 0xffff) / 65535.f;
        pt.vx = 0.0008f * (((rng >> 4) & 255) / 127.f - 1.f);
        pt.vy = 0.0008f * (((rng >> 12) & 255) / 127.f - 1.f);
        pt.r = 0.6f + 1.8f * ((rng & 255) / 255.f);
        pt.hue = ((rng >> 20) & 255) / 255.f;
        pt.a = 0.25f + 0.75f * ((rng >> 6) & 255) / 255.f;
    }

    proc.storeSlot(0);
    proc.storeSlot(1);
    applyScaleFromParam();
    startTimerHz(30);
}

GodAtomEditor::~GodAtomEditor() { setLookAndFeel(nullptr); }

void GodAtomEditor::applyScaleFromParam() {
    int idx = scaleBox.getSelectedItemIndex();
    if (idx < 0) idx = 1;
    const float scales[] = { 0.75f, 1.f, 1.25f, 1.5f, 2.f };
    float sc = scales[juce::jlimit(0, 4, idx)];
    setSize((int)std::lround(kBaseW * sc), (int)std::lround(kBaseH * sc));
}

void GodAtomEditor::resized() {
    scale = (float)getWidth() / (float)kBaseW;
    // Top bar
    presetBox.setBounds(SR(16, 6, 170, 24).toNearestInt());
    saveBtn.setBounds(SR(192, 6, 50, 24).toNearestInt());
    btnA.setBounds(SR(250, 6, 32, 24).toNearestInt());
    btnB.setBounds(SR(286, 6, 32, 24).toNearestInt());
    scInt.setBounds(SR(700, 6, 46, 24).toNearestInt());
    scExt.setBounds(SR(750, 6, 46, 24).toNearestInt());
    scaleBox.setBounds(SR(830, 6, 90, 24).toNearestInt());
    settingsBtn.setBounds(SR(930, 6, 94, 24).toNearestInt());

    // EQ knobs
    eqOn.setBounds(SR(132, 50, 52, 26).toNearestInt());
    eqHigh.setBounds(SR(126, 84, 64, 64).toNearestInt());
    eqMid.setBounds(SR(126, 154, 64, 64).toNearestInt());
    eqLow.setBounds(SR(126, 224, 64, 64).toNearestInt());

    // Amount
    amountKnob.setBounds(SR(268, 100, 150, 150).toNearestInt());
    charOn.setBounds(SR(455, 200, 74, 28).toNearestInt());

    // Limiter power
    limOn.setBounds(SR(812, 50, 52, 26).toNearestInt());

    // Chassis knobs
    inKnob.setBounds(SR(36, 318, 92, 92).toNearestInt());
    limKnob.setBounds(SR(800, 318, 92, 92).toNearestInt());
    outKnob.setBounds(SR(912, 318, 92, 92).toNearestInt());

    const bool settings = proc.showSettings.load();
    osBox.setBounds(SR(330, 100, 200, 28).toNearestInt());
    meterBox.setBounds(SR(330, 148, 280, 28).toNearestInt());
    animBtn.setBounds(SR(330, 192, 160, 28).toNearestInt());
    autoBtn.setBounds(SR(500, 192, 200, 28).toNearestInt());
    osBox.setVisible(settings);
    meterBox.setVisible(settings);
    animBtn.setVisible(settings);
    autoBtn.setVisible(settings);
    amountKnob.setVisible(!settings);
    charOn.setVisible(!settings);
}

void GodAtomEditor::timerCallback() {
    bool ext = proc.apvts.getRawParameterValue("extSC")->load() >= 0.5f;
    scInt.setToggleState(!ext, juce::dontSendNotification);
    scExt.setToggleState(ext, juce::dontSendNotification);
    int prog = proc.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != prog && !presetBox.hasKeyboardFocus(true))
        presetBox.setSelectedId(prog, juce::dontSendNotification);
    if (proc.apvts.getRawParameterValue("animation")->load() >= 0.5f) {
        for (auto& pt : parts) {
            pt.x += pt.vx; pt.y += pt.vy;
            if (pt.x < 0 || pt.x > 1) pt.vx = -pt.vx;
            if (pt.y < 0 || pt.y > 1) pt.vy = -pt.vy;
            pt.x = juce::jlimit(0.f, 1.f, pt.x);
            pt.y = juce::jlimit(0.f, 1.f, pt.y);
        }
    }
    repaint();
}

void GodAtomEditor::mouseDown(const juce::MouseEvent& e) {
    // Click input/output numeric area to reset peaks
    auto inNum = SR(16, 44, 100, 40);
    auto outNum = SR(920, 44, 104, 40);
    if (inNum.contains(e.position) || outNum.contains(e.position))
        proc.resetMeters();
}

void GodAtomEditor::drawMeters(juce::Graphics& g, juce::Rectangle<float> area,
                               float pkL, float pkR, float aL, float aR, bool inputSide, bool lufs) {
    auto dbToY = [&](float db) {
        float n = juce::jlimit(0.f, 1.f, (db + 60.f) / 60.f);
        return area.getBottom() - n * area.getHeight();
    };
    float gap = area.getWidth() * 0.18f;
    float bw = (area.getWidth() - gap) * 0.5f;
    auto left = area.removeFromLeft(bw);
    area.removeFromLeft(gap);
    auto right = area;
    auto paintBar = [&](juce::Rectangle<float> r, float pkDb, float auxDb) {
        g.setColour(col::meterTrack);
        g.fillRoundedRectangle(r, 2.0f);
        // ticks
        g.setColour(col::meter.withAlpha(0.35f));
        for (int d = 0; d <= 60; d += 6) {
            float y = dbToY(-(float)d);
            if (y > r.getY() && y < r.getBottom())
                g.drawLine(r.getX(), y, r.getRight(), y, 0.6f);
        }
        float yPk = dbToY(pkDb);
        auto fill = r.withTop(yPk);
        g.setColour(col::meter);
        g.fillRoundedRectangle(fill, 2.0f);
        if (inputSide) {
            // target brackets around -12..-4 dB (guide)
            float y0 = dbToY(-4.f), y1 = dbToY(-12.f);
            g.setColour(col::mint);
            float t = 3.0f;
            g.drawLine(r.getX() - 4, y0, r.getX() - 4, y1, t);
            g.drawLine(r.getX() - 4, y0, r.getX() + 3, y0, t);
            g.drawLine(r.getX() - 4, y1, r.getX() + 3, y1, t);
            g.drawLine(r.getRight() + 4, y0, r.getRight() + 4, y1, t);
            g.drawLine(r.getRight() - 3, y0, r.getRight() + 4, y0, t);
            g.drawLine(r.getRight() - 3, y1, r.getRight() + 4, y1, t);
        }
        (void)auxDb;
    };
    paintBar(left, pkL, aL);
    paintBar(right, pkR, aR);
}

void GodAtomEditor::drawGr(juce::Graphics& g, juce::Rectangle<float> area, const ga::Meters& m) {
    const char* names[3] = { "high", "mid", "low" };
    float vals[3] = { m.grHigh, m.grMid, m.grLow };
    float rowH = area.getHeight() / 3.0f;
    for (int i = 0; i < 3; ++i) {
        auto row = area.withY(area.getY() + i * rowH).withHeight(rowH).reduced(0, 6);
        g.setColour(col::mint);
        g.setFont(juce::FontOptions(S(12)));
        g.drawText(names[i], row.removeFromLeft(S(40)).toNearestInt(), juce::Justification::centredLeft, false);
        float gr = vals[i]; // already negative or 0
        float mag = juce::jlimit(0.f, 1.f, -gr / 12.f);
        auto meter = row.removeFromLeft(S(70)).reduced(0, S(6));
        g.setColour(col::meterTrack);
        g.fillRoundedRectangle(meter, 2.0f);
        auto fill = meter.withWidth(juce::jmax(2.f, meter.getWidth() * mag));
        g.setColour(col::meter);
        g.fillRoundedRectangle(fill, 2.0f);
        // target brackets around ~3 dB (0.25 of 12)
        float tx = meter.getX() + meter.getWidth() * 0.22f;
        float tw = meter.getWidth() * 0.18f;
        g.setColour(col::mint);
        g.drawRect(tx, meter.getY() - 3.f, tw, meter.getHeight() + 6.f, 1.4f);
        g.setFont(juce::FontOptions(S(12)));
        g.drawText(juce::String(gr, 1), row.toNearestInt(), juce::Justification::centredLeft, false);
    }
}

void GodAtomEditor::drawGalaxy(juce::Graphics& g, juce::Rectangle<float> area, float amount, float level) {
    auto oval = area.reduced(S(8));
    juce::Path clip;
    clip.addEllipse(oval);
    g.saveState();
    g.reduceClipRegion(clip);
    g.setColour(juce::Colours::black);
    g.fillEllipse(oval);
    juce::ColourGradient glow(juce::Colour(0xff102018), oval.getCentre(),
                              juce::Colours::transparentBlack, oval.getCentre().translated(oval.getWidth() * 0.45f, 0), true);
    g.setGradientFill(glow);
    g.fillEllipse(oval);
    int nShow = juce::jlimit(20, (int)parts.size(), (int)(40 + amount * 0.5f + level * 80));
    for (int i = 0; i < nShow; ++i) {
        const auto& pt = parts[(size_t)i];
        float px = oval.getX() + pt.x * oval.getWidth();
        float py = oval.getY() + pt.y * oval.getHeight();
        juce::Colour c;
        if (pt.hue < 0.25f) c = juce::Colours::white;
        else if (pt.hue < 0.5f) c = juce::Colour(0xffe6c35c);
        else if (pt.hue < 0.75f) c = col::mint;
        else c = juce::Colour(0xffd46cc8);
        g.setColour(c.withAlpha(pt.a));
        float rr = pt.r * scale * (0.8f + 0.4f * level);
        g.fillEllipse(px - rr, py - rr, rr * 2, rr * 2);
    }
    g.restoreState();
    g.setColour(juce::Colour(0xff202226));
    g.drawEllipse(oval, 1.2f * scale);

    // Amount capsule
    auto cap = juce::Rectangle<float>(S(430), S(110), S(130), S(120));
    g.setColour(juce::Colour(0xcc0b0a0e));
    g.fillRoundedRectangle(cap, S(18));
    g.setColour(col::mint);
    g.setFont(juce::FontOptions(S(16)));
    g.drawText("amount", cap.withHeight(S(28)).translated(0, S(10)).toNearestInt(),
               juce::Justification::centred, false);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(S(36)).withStyle("Bold"));
    g.drawText(juce::String((int)std::lround(amount)) + "%",
               cap.withY(cap.getY() + S(34)).withHeight(S(48)).toNearestInt(),
               juce::Justification::centred, false);
}

void GodAtomEditor::drawChassis(juce::Graphics& g) {
    auto ch = SR(0, 300, (float)kBaseW, 130);
    g.setColour(col::chassis);
    g.fillRect(ch);
    // bevel
    g.setColour(col::chassisHi);
    g.fillRect(SR(0, 300, (float)kBaseW, 4));

    // grille
    auto gr = SR(200, 318, 560, 72);
    g.setColour(juce::Colour(0xff3a3637));
    g.fillRect(gr);
    g.setColour(juce::Colour(0xff676465));
    const int rows = 14, cols = 36;
    float cw = gr.getWidth() / cols, rh = gr.getHeight() / rows;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            g.fillRoundedRectangle(gr.getX() + c * cw + 1.0f, gr.getY() + r * rh + 1.2f,
                                   cw - 2.2f, rh * 0.45f, 0.8f);

    g.setColour(col::label.withAlpha(0.7f));
    g.setFont(juce::FontOptions(S(13)).withKerningFactor(0.45f));
    g.drawText("G O D    A T O M", SR(200, 392, 560, 22).toNearestInt(),
               juce::Justification::centred, false);

    g.setColour(col::label);
    g.setFont(juce::FontOptions(S(13)));
    g.drawText("input", SR(36, 304, 92, 16).toNearestInt(), juce::Justification::centred, false);
    g.drawText("eq", SR(126, 304, 64, 16).toNearestInt(), juce::Justification::centred, false);
    g.drawText("gain reduction", SR(630, 304, 140, 16).toNearestInt(), juce::Justification::centred, false);
    g.drawText("limiter", SR(800, 304, 92, 16).toNearestInt(), juce::Justification::centred, false);
    g.drawText("output", SR(912, 304, 92, 16).toNearestInt(), juce::Justification::centred, false);
}

void GodAtomEditor::drawSettings(juce::Graphics& g) {
    if (!proc.showSettings.load()) return;
    auto pan = SR(300, 50, 440, 200);
    g.setColour(juce::Colour(0xee121214));
    g.fillRoundedRectangle(pan, 10.0f);
    g.setColour(col::mint);
    g.drawRoundedRectangle(pan, 10.0f, 1.2f);
    g.setFont(juce::FontOptions(S(16)));
    g.drawText("Settings", SR(320, 58, 200, 22).toNearestInt(), juce::Justification::centredLeft, false);
    g.setColour(col::label);
    g.setFont(juce::FontOptions(S(12)));
    g.drawText("Oversample", SR(330, 82, 200, 16).toNearestInt(), juce::Justification::centredLeft, false);
    g.drawText("Meters", SR(330, 130, 200, 16).toNearestInt(), juce::Justification::centredLeft, false);
}

void GodAtomEditor::paint(juce::Graphics& g) {
    auto m = proc.currentMeters();
    auto p = proc.readParams();
    bool lufs = p.meterMode == 1;
    float level = juce::jlimit(0.f, 1.f, (m.inPeakL + 60.f) / 60.f);

    g.fillAll(col::topBar);

    // display
    auto disp = SR(0, 36, (float)kBaseW, 264);
    g.setColour(col::display);
    g.fillRect(disp);

    // input numbers + meters
    auto fmt = [](float db) {
        if (db <= -90.f) return juce::String("-inf");
        return juce::String(db, 1);
    };
    auto peakCol = [](float db) { return db >= 0.f ? col::clip : col::mint; };
    g.setFont(juce::FontOptions(S(12)));
    g.setColour(peakCol(m.inPeakL));
    g.drawText(fmt(m.inPeakL), SR(16, 44, 48, 16).toNearestInt(), juce::Justification::centred, false);
    g.setColour(peakCol(m.inPeakR));
    g.drawText(fmt(m.inPeakR), SR(64, 44, 48, 16).toNearestInt(), juce::Justification::centred, false);
    g.setColour(col::mint);
    g.drawText(lufs ? juce::String(m.inLufs, 1) : fmt(m.inRmsL),
               SR(16, 60, 48, 16).toNearestInt(), juce::Justification::centred, false);
    g.drawText(lufs ? juce::String(m.inLufs, 1) : fmt(m.inRmsR),
               SR(64, 60, 48, 16).toNearestInt(), juce::Justification::centred, false);
    drawMeters(g, SR(28, 84, 72, 200), m.inPeakL, m.inPeakR,
               lufs ? m.inLufs : m.inRmsL, lufs ? m.inLufs : m.inRmsR, true, lufs);

    // EQ labels
    g.setColour(col::mint);
    g.setFont(juce::FontOptions(S(12)));
    g.drawText("high", SR(126, 146, 64, 14).toNearestInt(), juce::Justification::centred, false);
    g.drawText("mid",  SR(126, 216, 64, 14).toNearestInt(), juce::Justification::centred, false);
    g.drawText("low",  SR(126, 276, 64, 14).toNearestInt(), juce::Justification::centred, false);

    drawGalaxy(g, SR(220, 48, 400, 240), p.amount, level);
    drawGr(g, SR(630, 84, 160, 200), m);

    // limiter numbers
    g.setColour(col::mint);
    g.setFont(juce::FontOptions(S(12)));
    g.drawText(juce::String(m.limGrL, 1), SR(800, 84, 52, 16).toNearestInt(), juce::Justification::centred, false);
    g.drawText(juce::String(m.limGrR, 1), SR(852, 84, 52, 16).toNearestInt(), juce::Justification::centred, false);
    {
        auto bar = SR(812, 108, 20, 160);
        auto paintLim = [&](juce::Rectangle<float> r, float gr) {
            g.setColour(col::meterTrack);
            g.fillRoundedRectangle(r, 2.0f);
            float mag = juce::jlimit(0.f, 1.f, -gr / 12.f);
            auto fill = r.withTop(r.getBottom() - mag * r.getHeight());
            g.setColour(col::meter);
            g.fillRoundedRectangle(fill, 2.0f);
        };
        paintLim(SR(818, 108, 16, 160), m.limGrL);
        paintLim(SR(842, 108, 16, 160), m.limGrR);
    }

    // output numbers + meters
    g.setColour(peakCol(m.outPeakL));
    g.drawText(fmt(m.outPeakL), SR(924, 44, 48, 16).toNearestInt(), juce::Justification::centred, false);
    g.setColour(peakCol(m.outPeakR));
    g.drawText(fmt(m.outPeakR), SR(972, 44, 48, 16).toNearestInt(), juce::Justification::centred, false);
    g.setColour(col::mint);
    g.drawText(lufs ? juce::String(m.outLufs, 1) : fmt(m.outRmsL),
               SR(924, 60, 48, 16).toNearestInt(), juce::Justification::centred, false);
    g.drawText(lufs ? juce::String(m.outLufs, 1) : fmt(m.outRmsR),
               SR(972, 60, 48, 16).toNearestInt(), juce::Justification::centred, false);
    drawMeters(g, SR(936, 84, 72, 200), m.outPeakL, m.outPeakR,
               lufs ? m.outLufs : m.outRmsL, lufs ? m.outLufs : m.outRmsR, false, lufs);

    drawChassis(g);

    // top labels
    g.setColour(col::label);
    g.setFont(juce::FontOptions(S(11)));
    g.drawText("Preset", SR(16, 4, 80, 12).toNearestInt(), juce::Justification::centredLeft, false);
    g.drawText("Sidechain", SR(630, 10, 70, 16).toNearestInt(), juce::Justification::centredRight, false);
    g.drawText("UI Scale", SR(770, 10, 58, 16).toNearestInt(), juce::Justification::centredRight, false);

    drawSettings(g);
}
