#include "PluginProcessor.h"
#include "PluginEditor.h"

// =============================================================================
//  ARC FIN — LookAndFeel pour knobs (grand façade 94px + petit avancé 50px)
//  Arc 270° de 135° à 45° (sens horaire depuis 12h), dégradé cyan→teal
// =============================================================================
class ArcFinLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float startAngle, float endAngle, juce::Slider&) override
    {
        bool   isLarge   = (width >= 70);
        float  arcThick  = isLarge ? 5.5f : 4.0f;
        float  inset     = isLarge ? 8.0f : 5.0f;

        float  cx  = (float)x + (float)width  * 0.5f;
        float  cy  = (float)y + (float)height * 0.5f;
        float  outerR = (float)juce::jmin(width, height) * 0.5f;
        float  arcR   = outerR - arcThick * 0.5f;

        float  valAngle = startAngle + sliderPos * (endAngle - startAngle);

        // ── 1. Track arc (gris clair, plein 270°) ──────────────────────────
        juce::Path track;
        track.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xFFcdddd9));
        g.strokePath(track, juce::PathStrokeType(arcThick,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ── 2. Arc rempli (gradient cyan→teal jusqu'à la valeur) ────────────
        if (sliderPos > 0.005f)
        {
            juce::Path filled;
            filled.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, valAngle, true);
            juce::ColourGradient grad(
                juce::Colour(0xFF13add6), cx - arcR, cy,
                juce::Colour(0xFF1bb89e), cx + arcR, cy + arcR,
                false);
            g.setGradientFill(grad);
            g.strokePath(filled, juce::PathStrokeType(arcThick,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // ── 3. Cercle central (fond verre + ring) ───────────────────────────
        float inR = outerR - inset;
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.fillEllipse(cx - inR, cy - inR, inR * 2.0f, inR * 2.0f);
        g.setColour(juce::Colour(0xFF16b3bf).withAlpha(0.20f));
        g.drawEllipse(cx - inR + 0.75f, cy - inR + 0.75f,
                      (inR - 0.75f) * 2.0f, (inR - 0.75f) * 2.0f, 1.0f);

        // ── 4. Aiguille (rectangle arrondi gradient cyan→teal) ─────────────
        float needleW  = isLarge ? 3.0f  : 2.5f;
        float needleH  = isLarge ? 10.0f : 8.0f;
        float needleTopY = isLarge ? 6.0f  : 4.0f;  // depuis le haut du cercle interne

        // Dessin avant rotation : aiguille pointant vers 12h (y négatif = haut)
        float needleTopFromCenter = -(inR - needleTopY);
        juce::Path needle;
        needle.addRoundedRectangle(-needleW * 0.5f, needleTopFromCenter,
                                    needleW, needleH, 1.0f);
        needle.applyTransform(
            juce::AffineTransform::rotation(valAngle).translated(cx, cy));

        juce::ColourGradient ng(
            juce::Colour(0xFF13add6), cx, cy - inR,
            juce::Colour(0xFF1bb89e), cx, cy - inR + needleH,
            false);
        g.setGradientFill(ng);
        g.fillPath(needle);
    }

    juce::Font getLabelFont(juce::Label& lbl) override
    {
        // Valeur numérique dans la textbox → léger
        return juce::Font(juce::FontOptions(lbl.getHeight() > 12 ? 13.0f : 10.0f));
    }
};

static ArcFinLookAndFeel arcFinLAF;

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
    masterGainLabel.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
    masterGainLabel.setJustificationType(juce::Justification::centredRight);
    masterGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF26332f));
    addAndMakeVisible(masterGainLabel);

    // Master gain slider : Arc Fin horizontal style
    masterGainSlider.setColour(juce::Slider::thumbColourId,       juce::Colour(0xFF16b3bf));
    masterGainSlider.setColour(juce::Slider::trackColourId,       juce::Colour(0xFFcdddd9));
    masterGainSlider.setColour(juce::Slider::backgroundColourId,  juce::Colour(0xFFcdddd9));
    masterGainSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFF94a5a2));
    masterGainSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xB3FFFFFF));
    masterGainSlider.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xB3C8DCDC));

    // Descriptions légères sous les knobs façade
    auto applyFacadeDesc = [](ModernSlider& ms, const juce::String& desc) {
        ms.label.setText(desc, juce::dontSendNotification);
        ms.label.setFont(juce::Font(juce::FontOptions(9.0f)));
        ms.label.setJustificationType(juce::Justification::centred);
        ms.label.setColour(juce::Label::textColourId, juce::Colour(0xFF94a5a2));
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
    deltaBtn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF94a5a2));
    deltaBtn.button.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xFF13add6));
    deltaBtn.button.setLookAndFeel(&flatButtonLAF);
    addAndMakeVisible(deltaBtn.button);

    auto setupDeltaBtn = [&](CustomButton& btn) {
        btn.button.setButtonText(juce::String::fromUTF8("\xCE\x94"));
        btn.button.setClickingTogglesState(true);
        btn.button.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF94a5a2));
        btn.button.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xFF13add6));
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
        btn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF94a5a2));
        btn.button.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xFFcdddd9));
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

    // Façade knobs: textbox 94px large, 20px tall (Arc Fin value display)
    for (auto* ms : { &cleanSlider, &roomSlider, &reverbSlider, &deEssSlider, &airSlider })
        ms->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 94, 20);

    setSize(760, 290);
}

InvisibleFixAudioProcessorEditor::~InvisibleFixAudioProcessorEditor() = default;

// =============================================================================
//  APPLICATION DU STYLE AUX SLIDERS
// =============================================================================
void InvisibleFixAudioProcessorEditor::applyModernKnobStyle(juce::Slider& slider)
{
    slider.setLookAndFeel(&arcFinLAF);
    // Arc Fin : 270° de 225° (bas-gauche) à 135° (bas-droit), sens horaire
    // Le conic CSS démarre à 135°+90° = 225° (zone morte = 90° après "from 135deg")
    slider.setRotaryParameters(1.25f * juce::MathConstants<float>::pi,
                               2.75f * juce::MathConstants<float>::pi, true);
}

void InvisibleFixAudioProcessorEditor::setupModernSlider(ModernSlider& modernSlider, const juce::String& name)
{
    // Taille textbox par défaut pour les knobs avancés (50px)
    modernSlider.slider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    modernSlider.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    modernSlider.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFF26332f));
    modernSlider.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    modernSlider.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    modernSlider.slider.setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xFF13add6).withAlpha(0.25f));

    applyModernKnobStyle(modernSlider.slider);
    addAndMakeVisible(modernSlider.slider);

    // Libellé paramètre (sous la valeur) — Arc Fin: 8px, couleur muted
    modernSlider.label.setText(name, juce::dontSendNotification);
    modernSlider.label.setFont(juce::Font(juce::FontOptions(8.0f)));
    modernSlider.label.setJustificationType(juce::Justification::centred);
    modernSlider.label.setColour(juce::Label::textColourId, juce::Colour(0xFF94a5a2));
    addAndMakeVisible(modernSlider.label);
}

// =============================================================================
//  DESSIN GRAPHIQUE — ARC FIN
// =============================================================================
void InvisibleFixAudioProcessorEditor::paint(juce::Graphics& g)
{
    int W = getWidth(), H = getHeight();

    // ── Fond dégradé 145° ebf4f7 → e4f0ee → ddeaf0 ──────────────────────────
    {
        juce::ColourGradient bg(juce::Colour(0xFFebf4f7), 0.0f, 0.0f,
                                juce::Colour(0xFFddeaf0), (float)W, (float)H, false);
        bg.addColour(0.55, juce::Colour(0xFFe4f0ee));
        g.setGradientFill(bg);
        g.fillRoundedRectangle(0, 0, W, H, 9.0f);
    }

    // ── Bordure extérieure glass ─────────────────────────────────────────────
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawRoundedRectangle(0.5f, 0.5f, (float)W-1, (float)H-1, 9.0f, 1.0f);

    // ── Header ───────────────────────────────────────────────────────────────
    // Wordmark gauche : INVISIBLE ● FIX
    int hY = 12, hH = 28;
    g.setFont(juce::Font(juce::FontOptions(17.0f)));
    g.setColour(juce::Colour(0xFF26332f));
    g.drawText("INVISIBLE", 22, hY, 95, hH, juce::Justification::centredLeft);

    // point cyan lumineux
    float dotX = 121.0f, dotY = hY + hH * 0.5f - 3.0f;
    g.setColour(juce::Colour(0xFF13add6));
    g.fillEllipse(dotX, dotY, 6.0f, 6.0f);

    g.setFont(juce::Font(juce::FontOptions(17.0f)));
    g.setColour(juce::Colour(0xFF1bb89e));
    g.drawText("FIX", 134, hY, 40, hH, juce::Justification::centredLeft);

    // Version — droite du header
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.setColour(juce::Colour(0xFF94a5a2));
    g.drawText("V0.8  LAB STUDIO", W - 345, hY, 110, hH, juce::Justification::centredRight);

    // ── Cartes modules ───────────────────────────────────────────────────────
    // x positions : [24,168,312,456,600], w=136, y=58, h=200
    const int modX[5]   = { 24, 168, 312, 456, 600 };
    const int modW = 136, modY = 58, modH = 200;
    const char* modNames[5] = { "CLEAN", "ROOM", "DE-REV", "DE-ESS", "AIR ENHANCER" };

    AdvancedTab tabForMod[5] = {
        AdvancedTab::Clean, AdvancedTab::Room,
        AdvancedTab::None,  AdvancedTab::DeEss, AdvancedTab::Air
    };
    // bypass states
    bool bypassed[5] = {
        cleanBypassBtn.button.getToggleState(),
        roomBypassBtn.button.getToggleState(),
        reverbBypassBtn.button.getToggleState(),
        deEssBypassBtn.button.getToggleState(),
        airBypassBtn.button.getToggleState()
    };

    for (int i = 0; i < 5; ++i)
    {
        float fx = (float)modX[i], fy = (float)modY;
        float fw = (float)modW,    fh = (float)modH;
        float r  = 9.0f;
        bool  sel = (currentTab != AdvancedTab::None && currentTab == tabForMod[i]);
        float cardAlpha = bypassed[i] ? 0.30f : 0.55f;

        // Ombre douce
        g.setColour(juce::Colour(0xFF283050).withAlpha(0.07f));
        g.fillRoundedRectangle(fx + 1, fy + 2, fw, fh, r);

        // Fond carte glass
        g.setColour(juce::Colours::white.withAlpha(cardAlpha));
        g.fillRoundedRectangle(fx, fy, fw, fh, r);

        // Bordure blanche glass
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        g.drawRoundedRectangle(fx, fy, fw, fh, r, 1.0f);

        // Overlay cyan si sélectionné
        if (sel)
        {
            g.setColour(juce::Colour(0xFF13add6));
            g.drawRoundedRectangle(fx - 1, fy - 1, fw + 2, fh + 2, r, 1.5f);
        }

        // Nom module (centré, y=modY+22)
        g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFF26332f).withAlpha(bypassed[i] ? 0.40f : 1.0f));
        g.drawText(modNames[i], modX[i], modY + 22, modW, 15, juce::Justification::centred);

        // LED (bas de carte)
        float ledX = fx + fw * 0.5f - 3.5f;
        float ledY = fy + fh - 18.0f;
        g.setColour(juce::Colour(0xFFcdddd9));
        g.fillEllipse(ledX, ledY, 7.0f, 7.0f);
        if (!bypassed[i])
        {
            g.setColour(juce::Colour(0xFF13add6));
            g.fillEllipse(ledX, ledY, 7.0f, 7.0f);
        }
    }

    // ── Section Advanced ─────────────────────────────────────────────────────
    if (currentTab != AdvancedTab::None)
    {
        // Label "ADVANCED CONFIGURATION"
        g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFF94a5a2));
        g.drawText("ADVANCED CONFIGURATION", 24, 265, 300, 13, juce::Justification::centredLeft);

        auto drawAdvPanel = [&](float px, float py, float pw, float ph, const juce::String& title)
        {
            g.setColour(juce::Colours::white.withAlpha(0.40f));
            g.fillRoundedRectangle(px, py, pw, ph, 9.0f);
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.drawRoundedRectangle(px, py, pw, ph, 9.0f, 1.0f);
            g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText(title, px + 14, py + 12, pw - 28, 11, juce::Justification::centredLeft);
        };

        if (currentTab == AdvancedTab::Clean)
        {
            drawAdvPanel(24, 281, 296, 126, "NOISE REDUCTION");
            drawAdvPanel(334, 281, 402, 126, "GATE");
        }
        else if (currentTab == AdvancedTab::DeEss)
        {
            drawAdvPanel(24, 281, (float)(W - 48), 96, "DE-ESSER ADVANCED");
        }
        else if (currentTab == AdvancedTab::Room)
        {
            // no panel drawn, les sliders Room et le visualiseur sont sur fond libre
            g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText("NOTCH FREQ", 264, 293, 66, 11, juce::Justification::centredLeft);
            g.drawText("NOTCH Q",    332, 293, 54, 11, juce::Justification::centredLeft);
        }
        else if (currentTab == AdvancedTab::Air)
        {
            drawAdvPanel(24, 281, (float)(W - 48), 96, "AIR CHARACTER");
        }
    }
}

// =============================================================================
//  PLACEMENT — ARC FIN (760px, modules 136px aux x=[24,168,312,456,600])
// =============================================================================
void InvisibleFixAudioProcessorEditor::resized()
{
    // ── Façade : knob 94px + textbox 20px = 114px, centré dans 136px → x+21 ──
    const int modX[5] = { 24, 168, 312, 456, 600 };
    int sKnobY = 95;  // y du knob dans le module (après BYP+name: 58+11+14+12=95)
    int sLabelY = 95 + 114 + 5; // sous la textbox : 214

    ModernSlider* facadeSliders[5] = {
        &cleanSlider, &roomSlider, &reverbSlider, &deEssSlider, &airSlider
    };
    for (int i = 0; i < 5; ++i)
    {
        facadeSliders[i]->slider.setBounds(modX[i] + 21, sKnobY, 94, 114);
        facadeSliders[i]->label.setBounds(modX[i], sLabelY, 136, 12);
    }

    // ── BYP et △ dans chaque module ──────────────────────────────────────────
    // BYP: coin sup-gauche, △: coin sup-droit, tous deux à y=69 (dans la carte)
    int bY = 69, bW = 26, bH = 14;
    CustomButton* bypBtns[5] = {
        &cleanBypassBtn, &roomBypassBtn, &reverbBypassBtn, &deEssBypassBtn, &airBypassBtn
    };
    CustomButton* deltaBtns[5] = {
        &deltaBtn, &roomDeltaBtn, &reverbDeltaBtn, &deEssDeltaBtn, &airDeltaBtn
    };
    for (int i = 0; i < 5; ++i)
    {
        bypBtns[i]->button.setBounds(modX[i] + 8,            bY, bW, bH);
        deltaBtns[i]->button.setBounds(modX[i] + 136 - 8 - 16, bY, 16, bH);
    }

    // ── Master gain ───────────────────────────────────────────────────────────
    masterGainLabel.setBounds(getWidth() - 200, 12, 80, 28);
    masterGainSlider.setBounds(getWidth() - 116, 18, 92, 20);

    // ── Masquer tous les contrôles advanced ──────────────────────────────────
    if (roomVisualizer != nullptr) roomVisualizer->setVisible(false);

    auto hideSlider = [](ModernSlider& ms) {
        ms.slider.setVisible(false); ms.label.setVisible(false);
    };
    hideSlider(advRoomFreqSlider);  hideSlider(advRoomQSlider);
    hideSlider(advDeEssFreqSlider); hideSlider(advDeEssThreshSlider);
    hideSlider(advDeEssIntenSlider); hideSlider(advDeEssAtkSlider);
    hideSlider(advDeEssRelSlider);  hideSlider(advDeEssDetectSlider);
    hideSlider(advGateFloorSlider); hideSlider(advCleanRelSlider);
    hideSlider(advGateThresholdSlider); hideSlider(advGateAttackSlider);
    hideSlider(advGateHoldSlider);
    hideSlider(advAirDriveSlider);
    hideSlider(nrThreshSlider); hideSlider(nrRatioSlider);
    hideSlider(nrFloorSlider);  hideSlider(nrSmoothSlider);
    deEssModeBtn.button.setVisible(false);
    advAirFreqSlider.setVisible(false);
    airCharacterLabel.setVisible(false);

    // ── Advanced tabs ─────────────────────────────────────────────────────────
    // Knobs advanced: 50px rotary + 16px textbox = 66px total
    // Étiquette paramètre: sous le knob+textbox, h=10
    // advY = y départ des knobs dans le panneau
    // NR panel: x=24 pad=14 → 1er knob à x=38
    //   4 knobs, écart = (296-28-200)/3 = 22.67 → ~23px → pitch=73px
    //   positions x: 38, 111, 184, 257
    // Gate panel: x=334 pad=14 → 1er knob à x=348
    //   5 knobs, pitch=(402-28-250)/4=31px → pitch=81px
    //   positions x: 348, 429, 510, 591, 672

    if (currentTab == AdvancedTab::Clean)
    {
        int advY = 322;  // y knob dans panneau (panneaux à y=281, pad+label=41)
        int gS = 50, gH = 66, lH = 10;

        auto show = [](ModernSlider& ms) { ms.slider.setVisible(true); ms.label.setVisible(true); };
        auto place = [&](ModernSlider& ms, int px, int py) {
            ms.slider.setBounds(px, py, gS, gH);
            ms.label.setBounds(px, py + gH + 3, gS, lH);
        };
        show(nrThreshSlider);  place(nrThreshSlider,  38,  advY);
        show(nrRatioSlider);   place(nrRatioSlider,   111, advY);
        show(nrFloorSlider);   place(nrFloorSlider,   184, advY);
        show(nrSmoothSlider);  place(nrSmoothSlider,  257, advY);

        show(advGateThresholdSlider); place(advGateThresholdSlider, 348, advY);
        show(advGateAttackSlider);    place(advGateAttackSlider,    429, advY);
        show(advGateHoldSlider);      place(advGateHoldSlider,      510, advY);
        show(advGateFloorSlider);     place(advGateFloorSlider,     591, advY);
        show(advCleanRelSlider);      place(advCleanRelSlider,      672, advY);
    }
    else if (currentTab == AdvancedTab::Room)
    {
        int advY = 303;
        int advS = 50, gH = 66, lH = 10;
        if (roomVisualizer != nullptr) {
            addAndMakeVisible(roomVisualizer.get());
            roomVisualizer->setVisible(true);
            roomVisualizer->setBounds(24, advY, 220, 75);
        }
        advRoomFreqSlider.slider.setVisible(true); advRoomFreqSlider.label.setVisible(true);
        advRoomQSlider.slider.setVisible(true);    advRoomQSlider.label.setVisible(true);
        advRoomFreqSlider.slider.setBounds(264, advY + 10, advS, gH);
        advRoomFreqSlider.label.setBounds(264, advY + 10 + gH + 3, advS, lH);
        advRoomQSlider.slider.setBounds(330, advY + 10, advS, gH);
        advRoomQSlider.label.setBounds(330, advY + 10 + gH + 3, advS, lH);
    }
    else if (currentTab == AdvancedTab::DeEss)
    {
        int advY = 308;
        int advS = 50, gH = 66, lH = 10;
        // 7 knobs + mode btn espacés dans le panneau
        auto show = [](ModernSlider& ms) { ms.slider.setVisible(true); ms.label.setVisible(true); };
        auto place = [&](ModernSlider& ms, int px) {
            ms.slider.setBounds(px, advY, advS, gH);
            ms.label.setBounds(px, advY + gH + 3, advS, lH);
        };
        show(advDeEssFreqSlider);   place(advDeEssFreqSlider,   50);
        show(advDeEssThreshSlider); place(advDeEssThreshSlider, 115);
        show(advDeEssIntenSlider);  place(advDeEssIntenSlider,  180);
        show(advDeEssAtkSlider);    place(advDeEssAtkSlider,    245);
        show(advDeEssRelSlider);    place(advDeEssRelSlider,    310);
        deEssModeBtn.button.setVisible(true);
        deEssModeBtn.button.setBounds(378, advY + 20, 55, 22);
        show(advDeEssDetectSlider); place(advDeEssDetectSlider, 448);
    }
    else if (currentTab == AdvancedTab::Air)
    {
        int advY = 308;
        advAirFreqSlider.setVisible(true);
        advAirDriveSlider.slider.setVisible(true); advAirDriveSlider.label.setVisible(true);
        airCharacterLabel.setVisible(true);
        advAirFreqSlider.setBounds(40, advY + 20, 360, 25);
        advAirDriveSlider.slider.setBounds(424, advY, 50, 66);
        advAirDriveSlider.label.setBounds(424, advY + 69, 50, 10);
        airCharacterLabel.setBounds(490, advY + 20, 220, 25);
    }
}

void InvisibleFixAudioProcessorEditor::mouseUp(const juce::MouseEvent& event)
{
    juce::Point<int> clickPos = event.getPosition();
    AdvancedTab clickedTab = AdvancedTab::None;

    // Zones de clic : 5 modules x=[24,168,312,456,600] w=136, y=58 h=200
    if      (juce::Rectangle<int>( 24, 58, 136, 200).contains(clickPos)) clickedTab = AdvancedTab::Clean;
    else if (juce::Rectangle<int>(168, 58, 136, 200).contains(clickPos)) clickedTab = AdvancedTab::Room;
    else if (juce::Rectangle<int>(456, 58, 136, 200).contains(clickPos)) clickedTab = AdvancedTab::DeEss;
    else if (juce::Rectangle<int>(600, 58, 136, 200).contains(clickPos)) clickedTab = AdvancedTab::Air;

    if (clickedTab != AdvancedTab::None)
    {
        if (currentTab == clickedTab) currentTab = AdvancedTab::None;
        else                          currentTab = clickedTab;

        if (currentTab == AdvancedTab::None) setSize(760, 290);
        else                                  setSize(760, 440);

        resized();
        repaint();
    }
}