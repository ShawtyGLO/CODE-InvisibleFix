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
        g.fillAll(juce::Colour(0xFF12161A));

        // Grille d'arrière-plan discrète
        g.setColour(juce::Colour(0xFF1A2229));
        for (float x = 0.25f; x < 1.0f; x += 0.25f)
            g.drawVerticalLine(static_cast<int>(getWidth() * x), 0.0f, static_cast<float>(getHeight()));
        for (float y = 0.25f; y < 1.0f; y += 0.25f)
            g.drawHorizontalLine(static_cast<int>(getHeight() * y), 0.0f, static_cast<float>(getWidth()));

        auto& apvts = processor.apvts;
        float knobRoom = *apvts.getRawParameterValue("room");
        float knobFocus = *apvts.getRawParameterValue("focus");
        float targetNotchFreq = *apvts.getRawParameterValue("roomNotchFreq");
        float targetNotchQ = *apvts.getRawParameterValue("roomNotchQ");

        double fc = targetNotchFreq + (1.0 - (double)knobFocus) * 50.0;
        fc = juce::jlimit(100.0, 2000.0, fc);
        double Q = targetNotchQ * (1.0 + (double)knobFocus * 1.5);
        Q = juce::jlimit(0.5, 15.0, Q);

        // CORRECTION VISUALISEUR : Atténuation (creux négatif) vers le bas
        double maxAttenDB = -16.0 * (double)knobRoom;
        maxAttenDB = juce::jlimit(-16.0, 0.0, maxAttenDB);

        juce::Path responsePath;
        bool firstPoint = true;
        float w = static_cast<float>(getWidth());
        float h = static_cast<float>(getHeight());

        for (int pixelX = 0; pixelX < getWidth(); ++pixelX)
        {
            float t = static_cast<float>(pixelX) / w;
            float freq = 100.0f * std::pow(2000.0f / 100.0f, t);

            float gainDB = 0.0f;
            if (knobRoom > 0.001f)
            {
                float distance = std::abs(freq - static_cast<float>(fc));
                float bandwidth = static_cast<float>(fc / Q);
                if (distance < bandwidth * 2.0f)
                {
                    float normDist = distance / (bandwidth * 2.0f);
                    gainDB = static_cast<float>(maxAttenDB) * (1.0f - std::tanh(normDist * 2.0f));
                }
            }

            // Placement vertical ajusté pour dessiner vers le bas
            float pixelY = (h * 0.15f) + (gainDB / -16.0f) * (h * 0.7f);
            pixelY = juce::jlimit(0.0f, h, pixelY);

            if (firstPoint) {
                responsePath.startNewSubPath(static_cast<float>(pixelX), pixelY);
                firstPoint = false;
            }
            else {
                responsePath.lineTo(static_cast<float>(pixelX), pixelY);
            }
        }

        // Dessin du trait fin bleu néon
        g.setColour(juce::Colour(0xFF00E5FF));
        g.strokePath(responsePath, juce::PathStrokeType(1.5f)); // Ligne fine élégante

        // Remplissage dégradé sous la courbe
        g.setColour(juce::Colour(0x1500E5FF));
        responsePath.lineTo(w, h);
        responsePath.lineTo(0, h);
        responsePath.closeSubPath();
        g.fillPath(responsePath);

        g.setColour(juce::Colour(0xFF3A4D5C));
        g.drawRect(getLocalBounds(), 1);
    }

private:
    void timerCallback() override { repaint(); }
    InvisibleFixAudioProcessor& processor;
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

    // --- CONFIGURATION DU SLIDER DE-ESS ---
    deEssSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    deEssSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    applyModernKnobStyle(deEssSlider); // Applique ton style visuel moderne
    addAndMakeVisible(deEssSlider);

       // --- 0. ALLOCATION DU VISUALISEUR ---
    roomVisualizer = new RoomVisualizer(audioProcessor);
    addChildComponent(roomVisualizer);

    // --- 1. CONFIGURATION DES POTENTIOMÈTRES FAÇADE PRINCIPALE ---
    setupModernSlider(cleanSlider, "CLEAN");
    setupModernSlider(roomSlider, "ROOM");
    setupModernSlider(reverbSlider, "REVERB");
    setupModernSlider(airSlider, "AIR");
    setupModernSlider(focusSlider, "FOCUS");

    // Master Gain (juce::Slider direct)
    masterGainSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    masterGainSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(masterGainSlider);

    masterGainLabel.setText("MASTER GAIN", juce::dontSendNotification);
    masterGainLabel.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    masterGainLabel.setJustificationType(juce::Justification::centredRight);
    masterGainLabel.setColour(juce::Label::textColourId, juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.4f));
    addAndMakeVisible(masterGainLabel);

    // --- 2. CONFIGURATION DES BOUTONS INTERACTIFS DISCRETS ---
    // CORRECTION DELTA : Encodage UTF-8 universel sécurisé via code hexadécimal
    deltaBtn.button.setButtonText(juce::String::fromUTF8("\xCE\x94"));
    deltaBtn.button.setClickingTogglesState(true);
    deltaBtn.button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    deltaBtn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    deltaBtn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF2C3E50).withAlpha(0.3f));
    deltaBtn.button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF4A90E2));
    addAndMakeVisible(deltaBtn.button);

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
    deEssModeBtn.button.setButtonText("SHELF");
    addAndMakeVisible(deEssModeBtn.button);

    // --- 3. CONFIGURATION DES TIROIRS AVANCÉS ---
    setupModernSlider(advRoomFreqSlider, "Notch Freq");
    setupModernSlider(advRoomQSlider, "Notch Q");
    setupModernSlider(advDeEssIntenSlider, "De-Ess Intensity");
    setupModernSlider(advDeEssFreqSlider, "De-Ess Freq");
    setupModernSlider(advDeEssRelSlider, "De-Ess Release");
    setupModernSlider(advGateFloorSlider, "Gate Floor");
    setupModernSlider(advCleanRelSlider, "Gate Release");
    setupModernSlider(advPlosiveSlider, "Plosive Ctrl");

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
    deEssAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "deess", deEssSlider);
    roomAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "room", roomSlider.slider);
    reverbAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "reverb", reverbSlider.slider);
    focusAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "focus", focusSlider.slider);
    airAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "air", airSlider.slider);

    masterGainAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "outputGain", masterGainSlider);

    deltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "cleanDelta", deltaBtn.button);
    deEssModeAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "cleanDeEssFilterMode", deEssModeBtn.button);

    advRoomFreqAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "roomNotchFreq", advRoomFreqSlider.slider);
    advRoomQAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "roomNotchQ", advRoomQSlider.slider);

    advDeEssIntenAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanDeEss", advDeEssIntenSlider.slider);
    advDeEssFreqAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanDeEssFreq", advDeEssFreqSlider.slider);
    advDeEssRelAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanDeEssRelease", advDeEssRelSlider.slider);
    advGateFloorAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanGateFloor", advGateFloorSlider.slider);
    advCleanRelAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanRelease", advCleanRelSlider.slider);
    advPlosiveAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanPlosive", advPlosiveSlider.slider);

    advAirFreqAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "airFreq", advAirFreqSlider);

    setSize(680, 240);
}

InvisibleFixAudioProcessorEditor::~InvisibleFixAudioProcessorEditor()
{
    if (roomVisualizer != nullptr)
    {
        delete roomVisualizer;
        roomVisualizer = nullptr;
    }
}

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
    modernSlider.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

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
            g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
            g.drawText(title, x, y + 10, w, 18, juce::Justification::centred);

            g.setColour(isSelected ? juce::Colour((juce::uint8)74, (juce::uint8)144, (juce::uint8)226)
                : juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.1f));
            g.fillEllipse((float)x + ((float)w / 2.0f) - 2.5f, (float)y + (float)h - 12.0f, 5.0f, 5.0f);
        };

    drawModuleGroup(20, 50, 125, 170, "CLEAN & DE-ESS", currentTab == AdvancedTab::Clean);
    drawModuleGroup(185, 50, 125, 170, "ROOM NOTCH", currentTab == AdvancedTab::Room);
    drawModuleGroup(350, 50, 125, 170, "REVERB ABSORB", false);
    drawModuleGroup(515, 50, 125, 170, "AIR ENHANCER", currentTab == AdvancedTab::Air);

    if (currentTab != AdvancedTab::None)
    {
        g.setColour(juce::Colour((juce::uint8)228, (juce::uint8)234, (juce::uint8)235));
        g.fillRect(0, 236, getWidth(), getHeight() - 236);

        g.setColour(juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.08f));
        g.drawHorizontalLine(235, 0.0f, (float)getWidth());

        g.setColour(juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
        g.drawText("ADVANCED CONFIGURATION", 20, 242, 200, 15, juce::Justification::centredLeft);
    }
}

// =============================================================================
//  PLACEMENT ET GEOMETRIE DES ELEMENTS (RESIZED)
// =============================================================================
void InvisibleFixAudioProcessorEditor::resized()
{
    int sSize = 70;
    int sY = 85;
    int sLabelY = 175;

    // Modifie les coordonnées (X, Y, Largeur, Hauteur) selon ton design
    deEssSlider.setBounds(90, sY + 2, 50, 50);

    masterGainLabel.setBounds(getWidth() - 195, 0, 75, 35);
    masterGainSlider.setBounds(getWidth() - 115, 8, 100, 20);

    cleanSlider.slider.setBounds(35, sY, 55, 55);
    cleanSlider.label.setBounds(35, sLabelY, 55, 15);

    deltaBtn.button.setBounds(70, 62, 14, 14);

    roomSlider.slider.setBounds(192, sY, 52, 52);
    roomSlider.label.setBounds(192, sLabelY - 14, 52, 14);

    focusSlider.slider.setBounds(247, sY + 12, 45, 45);
    focusSlider.label.setBounds(247, sLabelY, 45, 14);

    reverbSlider.slider.setBounds(377, sY, sSize, sSize);
    reverbSlider.label.setBounds(377, sLabelY, sSize, 15);

    airSlider.slider.setBounds(542, sY, sSize, sSize);
    airSlider.label.setBounds(542, sLabelY, sSize, 15);

    if (roomVisualizer != nullptr)
        roomVisualizer->setVisible(false);

    advRoomFreqSlider.slider.setVisible(false);   advRoomFreqSlider.label.setVisible(false);
    advRoomQSlider.slider.setVisible(false);      advRoomQSlider.label.setVisible(false);
    advDeEssIntenSlider.slider.setVisible(false); advDeEssIntenSlider.label.setVisible(false);
    advDeEssFreqSlider.slider.setVisible(false);  advDeEssFreqSlider.label.setVisible(false);
    advDeEssRelSlider.slider.setVisible(false);   advDeEssRelSlider.label.setVisible(false);
    advGateFloorSlider.slider.setVisible(false);  advGateFloorSlider.label.setVisible(false);
    advCleanRelSlider.slider.setVisible(false);   advCleanRelSlider.label.setVisible(false);
    advPlosiveSlider.slider.setVisible(false);    advPlosiveSlider.label.setVisible(false);
    deEssModeBtn.button.setVisible(false);
    advAirFreqSlider.setVisible(false);
    airCharacterLabel.setVisible(false);

    int advY = 262;
    int advS = 54;

    if (currentTab == AdvancedTab::Room)
    {
        if (roomVisualizer != nullptr) {
            addAndMakeVisible(roomVisualizer);
            roomVisualizer->setVisible(true);
            roomVisualizer->setBounds(25, advY, 150, 65);
        }

        advRoomFreqSlider.slider.setVisible(true); advRoomFreqSlider.label.setVisible(true);
        advRoomQSlider.slider.setVisible(true);    advRoomQSlider.label.setVisible(true);

        advRoomFreqSlider.slider.setBounds(195, advY, advS, advS);
        advRoomFreqSlider.label.setBounds(195, advY + 54, advS, 14);
        advRoomQSlider.slider.setBounds(265, advY, advS, advS);
        advRoomQSlider.label.setBounds(265, advY + 54, advS, 14);
    }
    else if (currentTab == AdvancedTab::Clean)
    {
        advDeEssIntenSlider.slider.setVisible(true); advDeEssIntenSlider.label.setVisible(true);
        advDeEssFreqSlider.slider.setVisible(true);  advDeEssFreqSlider.label.setVisible(true);
        advDeEssRelSlider.slider.setVisible(true);   advDeEssRelSlider.label.setVisible(true);
        advGateFloorSlider.slider.setVisible(true);  advGateFloorSlider.label.setVisible(true);
        advCleanRelSlider.slider.setVisible(true);   advCleanRelSlider.label.setVisible(true);
        advPlosiveSlider.slider.setVisible(true);    advPlosiveSlider.label.setVisible(true);
        deEssModeBtn.button.setVisible(true);

        advDeEssIntenSlider.slider.setBounds(35, advY, advS, advS);
        advDeEssIntenSlider.label.setBounds(35, advY + 54, advS, 14);
        advDeEssFreqSlider.slider.setBounds(100, advY, advS, advS);
        advDeEssFreqSlider.label.setBounds(100, advY + 54, advS, 14);

        deEssModeBtn.button.setBounds(165, advY + 16, 60, 22);

        advDeEssRelSlider.slider.setBounds(240, advY, advS, advS);
        advDeEssRelSlider.label.setBounds(240, advY + 54, advS, 14);
        advGateFloorSlider.slider.setBounds(305, advY, advS, advS);
        advGateFloorSlider.label.setBounds(305, advY + 54, advS, 14);
        advCleanRelSlider.slider.setBounds(370, advY, advS, advS);
        advCleanRelSlider.label.setBounds(370, advY + 54, advS, 14);
        advPlosiveSlider.slider.setBounds(435, advY, advS, advS);
        advPlosiveSlider.label.setBounds(435, advY + 54, advS, 14);
    }
    else if (currentTab == AdvancedTab::Air)
    {
        advAirFreqSlider.setVisible(true);
        airCharacterLabel.setVisible(true);
        advAirFreqSlider.setBounds(40, advY + 15, 360, 25);
        airCharacterLabel.setBounds(420, advY + 15, 220, 25);
    }
}

void InvisibleFixAudioProcessorEditor::mouseUp(const juce::MouseEvent& event)
{
    juce::Point<int> clickPos = event.getPosition();
    AdvancedTab clickedTab = AdvancedTab::None;

    if (juce::Rectangle<int>(20, 50, 125, 170).contains(clickPos))       clickedTab = AdvancedTab::Clean;
    else if (juce::Rectangle<int>(185, 50, 125, 170).contains(clickPos)) clickedTab = AdvancedTab::Room;
    else if (juce::Rectangle<int>(515, 50, 125, 170).contains(clickPos)) clickedTab = AdvancedTab::Air;

    if (clickedTab != AdvancedTab::None)
    {
        if (currentTab == clickedTab) currentTab = AdvancedTab::None;
        else                          currentTab = clickedTab;

        if (currentTab == AdvancedTab::None) setSize(680, 240);
        else                                 setSize(680, 355);

        resized();
        repaint();
    }
}