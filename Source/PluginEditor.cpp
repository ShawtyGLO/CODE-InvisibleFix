#include "PluginProcessor.h"
#include "PluginEditor.h"

// =============================================================================
//  STYLE DE DESSIN UNIQUE POUR LES SLIDERS MODERNES (Extrait pour stabilité C++)
// =============================================================================
class ModernSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernSliderLookAndFeel() = default;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
        juce::Slider& s) override
    {
        auto radius = (float)juce::jmin(width / 2, height / 2) - 6.0f;
        auto centreX = (float)x + (float)width * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // 1. Fond de l'anneau (Gris très clair, très fin)
        g.setColour(juce::Colour(0xFFE4EAEB));
        g.drawEllipse(rx, ry, rw, rw, 1.5f);

        // 2. Arc de valeur actif (Bleu moderne fin et élégant)
        juce::Path p;
        p.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(0xFF4A90E2));
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Petit pointeur rotatif discret sur l'arc
        g.setColour(juce::Colour(0xFF2C3E50));
        float pointerRadius = 3.0f;
        float px = centreX + radius * std::sin(angle);
        float py = centreY - radius * std::cos(angle);
        g.fillEllipse(px - pointerRadius, py - pointerRadius, pointerRadius * 2.0f, pointerRadius * 2.0f);
    }
};

// Instance globale statique partagée proprement
static ModernSliderLookAndFeel modernSliderLookAndFeelInstance;

// =============================================================================
//  BOUTONS PLATS — aucun bord visible, fond transparent sauf état actif
// =============================================================================
class FlatButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
                               const juce::Colour&, bool, bool) override
    {
        if (b.getToggleState())
        {
            g.setColour(b.findColour(juce::TextButton::buttonOnColourId));
            g.fillRoundedRectangle(b.getLocalBounds().toFloat().reduced(1.0f), 3.0f);
        }
        // Off state : aucun fond, aucun bord
    }
};
static FlatButtonLookAndFeel flatButtonLAF;

// =============================================================================
//  COMPOSANT : RoomVisualizer (Écran Graphique pour le Notch)
// =============================================================================
class RoomVisualizer : public juce::Component, private juce::Timer
{
public:
    RoomVisualizer(InvisibleFixAudioProcessor& p) : processor(p)
    {
        startTimerHz(30);
    }

    ~RoomVisualizer() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        float w = (float)getWidth();
        float h = (float)getHeight();

        // === HEATMAP — couvre tout le fond (remplace le fillAll noir) ===
        processor.copyFftMagnitudes(displayMags, 2048);
        float sr = (float)processor.getSampleRate();
        if (sr < 1.0f) sr = 44100.0f;

        for (int px = 0; px < getWidth(); ++px)
        {
            float freq = 100.0f * std::pow(2000.0f / 100.0f, (float)px / w);
            int   bin  = juce::jlimit(0, 2047, (int)(freq * 2048.0f / sr));

            float magDB  = 20.0f * std::log10(displayMags[bin] / 2048.0f + 1e-7f);
            float nrm    = juce::jlimit(0.0f, 1.0f, (magDB + 65.0f) / 50.0f);

            float hue = (1.0f - nrm) * 0.66f;         // rouge=0.0, bleu=0.66
            float sat = (nrm > 0.05f) ? 0.88f : 0.0f; // gris si silence
            float bri = 0.08f + nrm * 0.80f;          // très sombre → brillant
            g.setColour(juce::Colour::fromHSV(hue, sat, bri, 1.0f));
            g.fillRect(px, 0, 1, getHeight());
        }

        // Grille discrète par-dessus la heatmap
        g.setColour(juce::Colour(0x30FFFFFF));
        for (float fx = 0.25f; fx < 1.0f; fx += 0.25f)
            g.drawVerticalLine((int)(w * fx), 0.0f, h);
        for (float fy = 0.25f; fy < 1.0f; fy += 0.25f)
            g.drawHorizontalLine((int)(h * fy), 0.0f, w);

        // === COURBE DU FILTRE NOTCH ===
        auto& apvts  = processor.apvts;
        float knobRoom  = *apvts.getRawParameterValue("room");
        double fc       = juce::jlimit(100.0, 2000.0, (double)*apvts.getRawParameterValue("roomNotchFreq"));
        double Q        = juce::jlimit(0.5, 15.0,    (double)*apvts.getRawParameterValue("roomNotchQ"));
        double maxAtten = juce::jlimit(-16.0, 0.0, -16.0 * (double)knobRoom);

        juce::Path curve;
        bool first = true;
        for (int px = 0; px < getWidth(); ++px)
        {
            float freq = 100.0f * std::pow(2000.0f / 100.0f, (float)px / w);
            float gain = 0.0f;
            if (knobRoom > 0.001f)
            {
                float dist = std::abs(freq - (float)fc);
                float bw   = (float)(fc / Q);
                if (dist < bw * 2.0f)
                    gain = (float)maxAtten * (1.0f - std::tanh(dist / (bw * 2.0f) * 2.0f));
            }
            float py = juce::jlimit(0.0f, h, h * 0.15f + (gain / -16.0f) * (h * 0.70f));
            if (first) { curve.startNewSubPath((float)px, py); first = false; }
            else          curve.lineTo((float)px, py);
        }

        juce::Path fill = curve;
        fill.lineTo(w, h); fill.lineTo(0.0f, h); fill.closeSubPath();
        g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.10f));
        g.fillPath(fill);

        g.setColour(juce::Colour(0xFF00E5FF));
        g.strokePath(curve, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(juce::Colour(0xFF3A4D5C));
        g.drawRect(getLocalBounds(), 1);
    }

private:
    void timerCallback() override { repaint(); }
    InvisibleFixAudioProcessor& processor;
    float displayMags[2048]{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoomVisualizer)
};

// =============================================================================
//  InvisibleFixAudioProcessorEditor — CONSTRUCTEUR
// =============================================================================
InvisibleFixAudioProcessorEditor::InvisibleFixAudioProcessorEditor(InvisibleFixAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    setVisible(true);

    // --- 0. ALLOCATION DU VISUALISEUR ---
    roomVisualizer = std::make_unique<RoomVisualizer>(audioProcessor);
    addChildComponent(roomVisualizer.get());

    // --- 1. CONFIGURATION DES POTENTIOMÈTRES FAÇADE PRINCIPALE ---
    setupModernSlider(cleanSlider,  "CLEAN");
    setupModernSlider(roomSlider,   "ROOM");
    setupModernSlider(reverbSlider, "DE-REV");
    setupModernSlider(deEssSlider,  "DE-ESS");
    setupModernSlider(airSlider,    "AIR");

    // Master Gain (juce::Slider direct)
    masterGainSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    masterGainSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(masterGainSlider);

    masterGainLabel.setText("MASTER GAIN", juce::dontSendNotification);
    masterGainLabel.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    masterGainLabel.setJustificationType(juce::Justification::centredRight);
    masterGainLabel.setColour(juce::Label::textColourId, juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.4f));
    addAndMakeVisible(masterGainLabel);

    // Descriptions légères sous les knobs façade
    auto applyFacadeDesc = [](ModernSlider& ms, const juce::String& desc) {
        ms.label.setText(desc, juce::dontSendNotification);
        ms.label.setFont(juce::Font(juce::FontOptions(10.0f)));
        ms.label.setJustificationType(juce::Justification::centred);
        ms.label.setColour(juce::Label::textColourId, juce::Colour(0xFF2C3E50).withAlpha(0.60f));
    };
    applyFacadeDesc(cleanSlider,  "noise reduction & gate");
    applyFacadeDesc(roomSlider,   "ROOM NOTCH");
    applyFacadeDesc(reverbSlider, "reverb reducer");
    applyFacadeDesc(deEssSlider,  "sibilance reducer");
    applyFacadeDesc(airSlider,    "shelf & exciter");

    // --- 2. CONFIGURATION DES BOUTONS INTERACTIFS DISCRETS ---
    // CORRECTION DELTA : Encodage UTF-8 universel sécurisé via code hexadécimal
    deltaBtn.button.setButtonText(juce::String::fromUTF8("\xCE\x94"));
    deltaBtn.button.setClickingTogglesState(true);
    deltaBtn.button.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    deltaBtn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    deltaBtn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF2C3E50).withAlpha(0.28f));
    deltaBtn.button.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xFF4A90E2));
    deltaBtn.button.setLookAndFeel(&flatButtonLAF);
    addAndMakeVisible(deltaBtn.button);

    auto setupDeltaBtn = [&](CustomButton& btn) {
        btn.button.setButtonText(juce::String::fromUTF8("\xCE\x94"));
        btn.button.setClickingTogglesState(true);
        btn.button.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF2C3E50).withAlpha(0.28f));
        btn.button.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xFF4A90E2));
        btn.button.setLookAndFeel(&flatButtonLAF);
        addAndMakeVisible(btn.button);
    };
    setupDeltaBtn(roomDeltaBtn);
    setupDeltaBtn(reverbDeltaBtn);
    setupDeltaBtn(deEssDeltaBtn);
    setupDeltaBtn(airDeltaBtn);

    // Bouton De-Ess Mode
    deEssModeBtn.button.setClickingTogglesState(true);
    deEssModeBtn.button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFFFFFF));
    deEssModeBtn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF00E5FF).withAlpha(0.12f));
    deEssModeBtn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF2C3E50).withAlpha(0.6f));
    deEssModeBtn.button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF00B1CC));

    deEssModeBtn.button.onClick = [this]() {
        bool isBell = deEssModeBtn.button.getToggleState();
        deEssModeBtn.button.setButtonText(isBell ? "BELL" : "SHELF");
        };
    {
        bool initialBell = *audioProcessor.apvts.getRawParameterValue("deEssFilterMode") > 0.5f;
        deEssModeBtn.button.setButtonText(initialBell ? "BELL" : "SHELF");
    }
    addAndMakeVisible(deEssModeBtn.button);

    // Boutons Bypass (5 modules) — plats, sans bord visible
    auto setupBypassBtn = [&](CustomButton& btn) {
        btn.button.setClickingTogglesState(true);
        btn.button.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFE74C3C).withAlpha(0.12f));
        btn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF2C3E50).withAlpha(0.45f));
        btn.button.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xFFE74C3C));
        btn.button.setButtonText("BYP");
        btn.button.setLookAndFeel(&flatButtonLAF);
        addAndMakeVisible(btn.button);
    };
    setupBypassBtn(cleanBypassBtn);
    setupBypassBtn(roomBypassBtn);
    setupBypassBtn(reverbBypassBtn);
    setupBypassBtn(deEssBypassBtn);
    setupBypassBtn(airBypassBtn);

    // --- 3. CONFIGURATION DES TIROIRS AVANCÉS ---
    setupModernSlider(advRoomFreqSlider, "Notch Freq");
    setupModernSlider(advRoomQSlider, "Notch Q");
    setupModernSlider(advDeEssFreqSlider,    "De-Ess Freq");
    setupModernSlider(advDeEssThreshSlider,  "Thresh dB");
    setupModernSlider(advDeEssIntenSlider,   "Intensity");
    setupModernSlider(advDeEssAtkSlider,     "Attack");
    setupModernSlider(advDeEssRelSlider,     "Release");
    setupModernSlider(advDeEssDetectSlider,  "Detect");
    setupModernSlider(advGateFloorSlider,    "Gate Floor");
    setupModernSlider(advCleanRelSlider,     "Gate Release");

    // Gate advanced
    setupModernSlider(advGateThresholdSlider, "Gate Thresh");
    setupModernSlider(advGateAttackSlider,    "Gate Attack");
    setupModernSlider(advGateHoldSlider,      "Gate Hold");

    // Air drive
    setupModernSlider(advAirDriveSlider, "Air Drive");

    // Curseur AIR FREQ (juce::Slider direct)
    advAirFreqSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    advAirFreqSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addChildComponent(advAirFreqSlider);

    airCharacterLabel.setText("CHARACTER: PRESENCE", juce::dontSendNotification);
    airCharacterLabel.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    airCharacterLabel.setJustificationType(juce::Justification::centredLeft);
    airCharacterLabel.setColour(juce::Label::textColourId, juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.4f));
    addChildComponent(airCharacterLabel);

    advAirFreqSlider.onValueChange = [this]() {
        float currentFreq = (float)advAirFreqSlider.getValue();
        if (currentFreq <= 10000.0f) {
            airCharacterLabel.setText("CHARACTER: PRESENCE", juce::dontSendNotification);
            airCharacterLabel.setColour(juce::Label::textColourId, juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.5f));
        }
        else {
            airCharacterLabel.setText("CHARACTER: PURE AIR", juce::dontSendNotification);
            airCharacterLabel.setColour(juce::Label::textColourId, juce::Colour((juce::uint8)74, (juce::uint8)144, (juce::uint8)226));
        }
        };

    // --- 4. ATTACHEMENTS APVTS ---
    cleanAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "clean", cleanSlider.slider);
    deEssAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deess", deEssSlider.slider);
    roomAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "room", roomSlider.slider);
    reverbAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "reverb", reverbSlider.slider);
    airAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "air", airSlider.slider);

    masterGainAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "outputGain", masterGainSlider);

    // Bypass attachments
    cleanBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "cleanBypass", cleanBypassBtn.button);
    roomBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "roomBypass", roomBypassBtn.button);
    reverbBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "reverbBypass", reverbBypassBtn.button);
    deEssBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "deessBypass", deEssBypassBtn.button);
    airBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "airBypass", airBypassBtn.button);

    // Delta attachments
    deltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "cleanDelta", deltaBtn.button);
    roomDeltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "roomDelta", roomDeltaBtn.button);
    reverbDeltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "reverbDelta", reverbDeltaBtn.button);
    deEssDeltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "deessDelta", deEssDeltaBtn.button);
    airDeltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "airDelta", airDeltaBtn.button);

    deEssModeAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "deEssFilterMode", deEssModeBtn.button);

    advRoomFreqAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "roomNotchFreq", advRoomFreqSlider.slider);
    advRoomQAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "roomNotchQ", advRoomQSlider.slider);

    advDeEssFreqAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssFreq",       advDeEssFreqSlider.slider);
    advDeEssThreshAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssThreshold",  advDeEssThreshSlider.slider);
    advDeEssIntenAttach   = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssIntensity",  advDeEssIntenSlider.slider);
    advDeEssAtkAttach     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssAttack",     advDeEssAtkSlider.slider);
    advDeEssRelAttach     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssRelease",    advDeEssRelSlider.slider);
    advDeEssDetectAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssDetectRatio", advDeEssDetectSlider.slider);
    advGateFloorAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanGateFloor",  advGateFloorSlider.slider);
    advCleanRelAttach     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanRelease",    advCleanRelSlider.slider);

    advAirFreqAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "airFreq", advAirFreqSlider);

    // Gate advanced attachments
    advGateThresholdAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanGateThreshold", advGateThresholdSlider.slider);
    advGateAttackAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanGateAttack",    advGateAttackSlider.slider);
    advGateHoldAttach      = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanGateHold",      advGateHoldSlider.slider);

    // Air drive
    advAirDriveAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "airDrive", advAirDriveSlider.slider);

    // NR — Noise Reduction
    setupModernSlider(nrThreshSlider, "NR Thresh");
    setupModernSlider(nrRatioSlider, "NR Ratio");
    setupModernSlider(nrFloorSlider, "NR Floor");
    setupModernSlider(nrSmoothSlider, "NR Smooth");
    nrThreshAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrThreshold", nrThreshSlider.slider);
    nrRatioAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrRatio", nrRatioSlider.slider);
    nrFloorAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrFloor", nrFloorSlider.slider);
    nrSmoothAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrSmooth", nrSmoothSlider.slider);

    setSize(680, 240);
}

InvisibleFixAudioProcessorEditor::~InvisibleFixAudioProcessorEditor() = default;

// =============================================================================
//  APPLICATION DU STYLE AUX SLIDERS
// =============================================================================
void InvisibleFixAudioProcessorEditor::applyModernKnobStyle(juce::Slider& slider)
{
    slider.setLookAndFeel(&modernSliderLookAndFeelInstance);
}

void InvisibleFixAudioProcessorEditor::setupModernSlider(ModernSlider& modernSlider, const juce::String& name)
{
    modernSlider.slider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    modernSlider.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 14);
    modernSlider.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFF2C3E50).withAlpha(0.70f));
    modernSlider.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    modernSlider.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    applyModernKnobStyle(modernSlider.slider);
    addAndMakeVisible(modernSlider.slider);

    modernSlider.label.setText(name, juce::dontSendNotification);
    modernSlider.label.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    modernSlider.label.setJustificationType(juce::Justification::centred);
    modernSlider.label.setColour(juce::Label::textColourId, juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.4f));
    addAndMakeVisible(modernSlider.label);
}

// =============================================================================
//  DESSIN GRAPHIQUE DES PANELS
// =============================================================================
void InvisibleFixAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour((juce::uint8)235, (juce::uint8)240, (juce::uint8)242));

    g.setColour(juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.1f));
    g.drawHorizontalLine(35, 0.0f, (float)getWidth());

    g.setColour(juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80));
    g.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
    g.drawText("INVISIBLE FIX", 20, 0, 150, 35, juce::Justification::centredLeft);

    // TEXTE VERSION CORRIGÉ : Sans aucun caractère instable pour le compilateur
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.setColour(juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.4f));
    g.drawText("V0.8 - LAB STUDIO", getWidth() - 310, 0, 110, 35, juce::Justification::centredRight);

    auto drawModuleGroup = [&](int x, int y, int w, int h, const juce::String& title, bool isSelected)
        {
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle((float)x, (float)y, (float)w, (float)h, 6.0f);

            g.setColour(isSelected ? juce::Colour((juce::uint8)74, (juce::uint8)144, (juce::uint8)226).withAlpha(0.8f)
                : juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.08f));
            g.drawRoundedRectangle((float)x, (float)y, (float)w, (float)h, 6.0f, isSelected ? 1.5f : 1.0f);

            g.setColour(juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(isSelected ? 0.9f : 0.4f));
            g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
            g.drawText(title, x, y + 8, w, 20, juce::Justification::centred);

            g.setColour(isSelected ? juce::Colour((juce::uint8)74, (juce::uint8)144, (juce::uint8)226)
                : juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.1f));
            g.fillEllipse((float)x + ((float)w / 2.0f) - 2.5f, (float)y + (float)h - 12.0f, 5.0f, 5.0f);
        };

    // 5 modules identiques : 128px chacun, gap 6px, marges 8px → 680px exact
    bool cleanTabActive = (currentTab == AdvancedTab::Clean);
    bool deEssTabActive = (currentTab == AdvancedTab::DeEss);
    drawModuleGroup(  8, 50, 128, 170, "CLEAN",        cleanTabActive);
    drawModuleGroup(142, 50, 128, 170, "ROOM",          currentTab == AdvancedTab::Room);
    drawModuleGroup(276, 50, 128, 170, "DE-REV",        false);
    drawModuleGroup(410, 50, 128, 170, "DE-ESS",        deEssTabActive);
    drawModuleGroup(544, 50, 128, 170, "AIR ENHANCER",  currentTab == AdvancedTab::Air);

    if (currentTab != AdvancedTab::None)
    {
        g.setColour(juce::Colour((juce::uint8)228, (juce::uint8)234, (juce::uint8)235));
        g.fillRect(0, 236, getWidth(), getHeight() - 236);

        g.setColour(juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.08f));
        g.drawHorizontalLine(235, 0.0f, (float)getWidth());

        g.setColour(juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
        g.drawText("ADVANCED CONFIGURATION", 20, 242, 200, 15, juce::Justification::centredLeft);

        if (currentTab == AdvancedTab::Clean)
        {
            int advY = 280;

            // Panneau NOISE REDUCTION (gauche)
            g.setColour(juce::Colours::white.withAlpha(0.40f));
            g.fillRoundedRectangle(14.0f, (float)(advY - 20), 248.0f, 78.0f, 5.0f);
            g.setColour(juce::Colour(0xFF2C3E50).withAlpha(0.07f));
            g.drawRoundedRectangle(14.0f, (float)(advY - 20), 248.0f, 78.0f, 5.0f, 1.0f);

            // Panneau GATE (droite)
            g.setColour(juce::Colours::white.withAlpha(0.40f));
            g.fillRoundedRectangle(318.0f, (float)(advY - 20), 352.0f, 78.0f, 5.0f);
            g.setColour(juce::Colour(0xFF2C3E50).withAlpha(0.07f));
            g.drawRoundedRectangle(318.0f, (float)(advY - 20), 352.0f, 78.0f, 5.0f, 1.0f);

            g.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF2C3E50).withAlpha(0.55f));
            g.drawText("NOISE REDUCTION", 22,  advY - 15, 180, 12, juce::Justification::centredLeft);
            g.drawText("GATE",            330, advY - 15, 80,  12, juce::Justification::centredLeft);
        }
        else if (currentTab == AdvancedTab::DeEss)
        {
            int advY = 280;
            g.setColour(juce::Colour(0xFF2C3E50).withAlpha(0.45f));
            g.drawText("DE-ESSER", 50, advY - 16, 200, 13, juce::Justification::centredLeft);
        }
        else if (currentTab == AdvancedTab::Room)
        {
            int advY = 262;
            g.setColour(juce::Colour(0xFF2C3E50).withAlpha(0.45f));
            g.drawText("NOTCH FREQ", 260, advY - 16, 66, 13, juce::Justification::centredLeft);
            g.drawText("NOTCH Q",    326, advY - 16, 54, 13, juce::Justification::centredLeft);
        }
    }
}

// =============================================================================
//  PLACEMENT ET GEOMETRIE DES ELEMENTS (RESIZED)
// =============================================================================
void InvisibleFixAudioProcessorEditor::resized()
{
    // Modules : x=[8,142,276,410,544], w=128 chacun, gap=6px, marges=8px → 680px
    // Knob 90px (rotary 76px + textbox 14px), centré dans 128px → offset=19px
    int sSize   = 90;
    int sY      = 72;   // juste après les boutons (finissent à y=71)
    int sLabelY = 172;  // description légère sous la textbox (qui finit à y=162)

    cleanSlider.slider.setBounds(  8+19, sY, sSize, sSize);
    cleanSlider.label.setBounds(   8,   sLabelY, 128, 14);

    roomSlider.slider.setBounds( 142+19, sY, sSize, sSize);
    roomSlider.label.setBounds(  142,  sLabelY, 128, 14);

    reverbSlider.slider.setBounds(276+19, sY, sSize, sSize);
    reverbSlider.label.setBounds( 276, sLabelY, 128, 14);

    deEssSlider.slider.setBounds( 410+19, sY, sSize, sSize);
    deEssSlider.label.setBounds(  410, sLabelY, 128, 14);

    airSlider.slider.setBounds(  544+19, sY, sSize, sSize);
    airSlider.label.setBounds(   544, sLabelY, 128, 14);

    // Bypass : coin sup-gauche de chaque module — même ligne que Delta (bY=53)
    int bY = 53, bW = 28, bH = 18;
    cleanBypassBtn.button.setBounds(   8+4, bY, bW, bH);
    roomBypassBtn.button.setBounds(  142+4, bY, bW, bH);
    reverbBypassBtn.button.setBounds(276+4, bY, bW, bH);
    deEssBypassBtn.button.setBounds( 410+4, bY, bW, bH);
    airBypassBtn.button.setBounds(   544+4, bY, bW, bH);

    // Delta : coin sup-droit de chaque module — même ligne que Bypass
    int dW = 24, dH = 18;
    deltaBtn.button.setBounds(       8+128-dW-4, bY, dW, dH);
    roomDeltaBtn.button.setBounds(  142+128-dW-4, bY, dW, dH);
    reverbDeltaBtn.button.setBounds(276+128-dW-4, bY, dW, dH);
    deEssDeltaBtn.button.setBounds( 410+128-dW-4, bY, dW, dH);
    airDeltaBtn.button.setBounds(   544+128-dW-4, bY, dW, dH);

    masterGainLabel.setBounds(getWidth() - 195, 0, 75, 35);
    masterGainSlider.setBounds(getWidth() - 115, 8, 100, 20);

    if (roomVisualizer != nullptr)
        roomVisualizer->setVisible(false);

    advRoomFreqSlider.slider.setVisible(false);   advRoomFreqSlider.label.setVisible(false);
    advRoomQSlider.slider.setVisible(false);      advRoomQSlider.label.setVisible(false);
    advDeEssFreqSlider.slider.setVisible(false);    advDeEssFreqSlider.label.setVisible(false);
    advDeEssThreshSlider.slider.setVisible(false);  advDeEssThreshSlider.label.setVisible(false);
    advDeEssIntenSlider.slider.setVisible(false);   advDeEssIntenSlider.label.setVisible(false);
    advDeEssAtkSlider.slider.setVisible(false);     advDeEssAtkSlider.label.setVisible(false);
    advDeEssRelSlider.slider.setVisible(false);     advDeEssRelSlider.label.setVisible(false);
    advDeEssDetectSlider.slider.setVisible(false);  advDeEssDetectSlider.label.setVisible(false);
    advGateFloorSlider.slider.setVisible(false);    advGateFloorSlider.label.setVisible(false);
    advCleanRelSlider.slider.setVisible(false);     advCleanRelSlider.label.setVisible(false);
    deEssModeBtn.button.setVisible(false);

    advAirFreqSlider.setVisible(false);
    airCharacterLabel.setVisible(false);
    advGateThresholdSlider.slider.setVisible(false); advGateThresholdSlider.label.setVisible(false);
    advGateAttackSlider.slider.setVisible(false);    advGateAttackSlider.label.setVisible(false);
    advGateHoldSlider.slider.setVisible(false);      advGateHoldSlider.label.setVisible(false);
    advAirDriveSlider.slider.setVisible(false);      advAirDriveSlider.label.setVisible(false);
    nrThreshSlider.slider.setVisible(false);  nrThreshSlider.label.setVisible(false);
    nrRatioSlider.slider.setVisible(false);   nrRatioSlider.label.setVisible(false);
    nrFloorSlider.slider.setVisible(false);   nrFloorSlider.label.setVisible(false);
    nrSmoothSlider.slider.setVisible(false);  nrSmoothSlider.label.setVisible(false);

    int advY = 280;
    int advS = 54;

    if (currentTab == AdvancedTab::Room)
    {
        int advY = 262; // le visualiseur est plus haut pour tenir dans 360px
        if (roomVisualizer != nullptr) {
            addAndMakeVisible(roomVisualizer.get());
            roomVisualizer->setVisible(true);
            roomVisualizer->setBounds(20, advY, 210, 80);
        }

        advRoomFreqSlider.slider.setVisible(true); advRoomFreqSlider.label.setVisible(true);
        advRoomQSlider.slider.setVisible(true);    advRoomQSlider.label.setVisible(true);
        advRoomFreqSlider.slider.setBounds(260, advY, advS, advS);
        advRoomFreqSlider.label.setBounds(260, advY + 54, advS, 14);
        advRoomQSlider.slider.setBounds(326, advY, advS, advS);
        advRoomQSlider.label.setBounds(326, advY + 54, advS, 14);
    }
    else if (currentTab == AdvancedTab::Clean)
    {
        // --- NR (gauche) + GATE (droite) sur la même ligne ---
        int gS = 50;

        // NR — 4 knobs à gauche
        nrThreshSlider.slider.setVisible(true);  nrThreshSlider.label.setVisible(true);
        nrRatioSlider.slider.setVisible(true);   nrRatioSlider.label.setVisible(true);
        nrFloorSlider.slider.setVisible(true);   nrFloorSlider.label.setVisible(true);
        nrSmoothSlider.slider.setVisible(true);  nrSmoothSlider.label.setVisible(true);

        nrThreshSlider.slider.setBounds(22,  advY, gS, gS); nrThreshSlider.label.setBounds(22,  advY + gS, gS, 14);
        nrRatioSlider.slider.setBounds(78,   advY, gS, gS); nrRatioSlider.label.setBounds(78,   advY + gS, gS, 14);
        nrFloorSlider.slider.setBounds(134,  advY, gS, gS); nrFloorSlider.label.setBounds(134,  advY + gS, gS, 14);
        nrSmoothSlider.slider.setBounds(190, advY, gS, gS); nrSmoothSlider.label.setBounds(190, advY + gS, gS, 14);

        // GATE — 5 knobs à droite
        advGateThresholdSlider.slider.setVisible(true); advGateThresholdSlider.label.setVisible(true);
        advGateAttackSlider.slider.setVisible(true);    advGateAttackSlider.label.setVisible(true);
        advGateHoldSlider.slider.setVisible(true);      advGateHoldSlider.label.setVisible(true);
        advGateFloorSlider.slider.setVisible(true);     advGateFloorSlider.label.setVisible(true);
        advCleanRelSlider.slider.setVisible(true);      advCleanRelSlider.label.setVisible(true);

        advGateThresholdSlider.slider.setBounds(330, advY, gS, gS); advGateThresholdSlider.label.setBounds(330, advY + gS, gS, 14);
        advGateAttackSlider.slider.setBounds(386,    advY, gS, gS); advGateAttackSlider.label.setBounds(386,    advY + gS, gS, 14);
        advGateHoldSlider.slider.setBounds(442,      advY, gS, gS); advGateHoldSlider.label.setBounds(442,      advY + gS, gS, 14);
        advGateFloorSlider.slider.setBounds(498,     advY, gS, gS); advGateFloorSlider.label.setBounds(498,     advY + gS, gS, 14);
        advCleanRelSlider.slider.setBounds(554,      advY, gS, gS); advCleanRelSlider.label.setBounds(554,      advY + gS, gS, 14);
    }
    else if (currentTab == AdvancedTab::DeEss)
    {
        advDeEssFreqSlider.slider.setVisible(true);   advDeEssFreqSlider.label.setVisible(true);
        advDeEssThreshSlider.slider.setVisible(true); advDeEssThreshSlider.label.setVisible(true);
        advDeEssIntenSlider.slider.setVisible(true);  advDeEssIntenSlider.label.setVisible(true);
        advDeEssAtkSlider.slider.setVisible(true);    advDeEssAtkSlider.label.setVisible(true);
        advDeEssRelSlider.slider.setVisible(true);    advDeEssRelSlider.label.setVisible(true);
        deEssModeBtn.button.setVisible(true);
        advDeEssDetectSlider.slider.setVisible(true); advDeEssDetectSlider.label.setVisible(true);

        advDeEssFreqSlider.slider.setBounds(50,   advY, advS, advS);
        advDeEssFreqSlider.label.setBounds(50,    advY + 54, advS, 14);
        advDeEssThreshSlider.slider.setBounds(115, advY, advS, advS);
        advDeEssThreshSlider.label.setBounds(115,  advY + 54, advS, 14);
        advDeEssIntenSlider.slider.setBounds(180, advY, advS, advS);
        advDeEssIntenSlider.label.setBounds(180,  advY + 54, advS, 14);
        advDeEssAtkSlider.slider.setBounds(245,   advY, advS, advS);
        advDeEssAtkSlider.label.setBounds(245,    advY + 54, advS, 14);
        advDeEssRelSlider.slider.setBounds(310,   advY, advS, advS);
        advDeEssRelSlider.label.setBounds(310,    advY + 54, advS, 14);
        deEssModeBtn.button.setBounds(375, advY + 16, 55, 22);
        advDeEssDetectSlider.slider.setBounds(445, advY, advS, advS);
        advDeEssDetectSlider.label.setBounds(445,  advY + 54, advS, 14);
    }
    else if (currentTab == AdvancedTab::Air)
    {
        advAirFreqSlider.setVisible(true);
        advAirDriveSlider.slider.setVisible(true); advAirDriveSlider.label.setVisible(true);
        airCharacterLabel.setVisible(true);
        advAirFreqSlider.setBounds(40, advY + 15, 360, 25);
        advAirDriveSlider.slider.setBounds(420, advY - 5, 60, 60);
        advAirDriveSlider.label.setBounds(420, advY + 55, 60, 14);
        airCharacterLabel.setBounds(490, advY + 15, 180, 25);
    }
}

void InvisibleFixAudioProcessorEditor::mouseUp(const juce::MouseEvent& event)
{
    juce::Point<int> clickPos = event.getPosition();
    AdvancedTab clickedTab = AdvancedTab::None;

    if      (juce::Rectangle<int>(  8, 50, 128, 170).contains(clickPos)) clickedTab = AdvancedTab::Clean;
    else if (juce::Rectangle<int>(142, 50, 128, 170).contains(clickPos)) clickedTab = AdvancedTab::Room;
    else if (juce::Rectangle<int>(410, 50, 128, 170).contains(clickPos)) clickedTab = AdvancedTab::DeEss;
    else if (juce::Rectangle<int>(544, 50, 128, 170).contains(clickPos)) clickedTab = AdvancedTab::Air;

    if (clickedTab != AdvancedTab::None)
    {
        if (currentTab == clickedTab) currentTab = AdvancedTab::None;
        else                          currentTab = clickedTab;

        if      (currentTab == AdvancedTab::None)  setSize(680, 240);
        else if (currentTab == AdvancedTab::Clean)  setSize(680, 360);
        else                                        setSize(680, 360);

        resized();
        repaint();
    }
}