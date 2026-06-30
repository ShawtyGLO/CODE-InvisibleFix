#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class RoomVisualizer;
class DualRailComponent;
class AHREnvelopeEditor;
class DeEssDetectDisplay;

class InvisibleFixAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    InvisibleFixAudioProcessorEditor(InvisibleFixAudioProcessor&);
    ~InvisibleFixAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

    // Structure uniforme pour les sliders
    struct ModernSlider
    {
        juce::Slider slider;
        juce::Label label;
    };

    // Structure uniforme pour les boutons
    struct CustomButton
    {
        juce::TextButton button;
    };

private:
    InvisibleFixAudioProcessor& audioProcessor;

    // Visualiseur graphique
    std::unique_ptr<juce::Component> roomVisualizer;

    enum class AdvancedTab { None, Clean, Room, Air, DeEss };
    AdvancedTab currentTab = AdvancedTab::None;

    // Façade principale
    ModernSlider cleanSlider, deEssSlider, roomSlider, reverbSlider, airSlider;
    juce::Slider masterGainSlider;
    juce::Label masterGainLabel;

    // Boutons Bypass
    CustomButton cleanBypassBtn, roomBypassBtn, reverbBypassBtn, deEssBypassBtn, airBypassBtn;
    // Boutons Delta
    CustomButton deltaBtn, roomDeltaBtn, reverbDeltaBtn, deEssDeltaBtn, airDeltaBtn;

    CustomButton deEssModeBtn;

    // Configuration avancée (Tiroirs)
    ModernSlider advRoomFreqSlider, advRoomQSlider;
    ModernSlider advDeEssFreqSlider, advDeEssThreshSlider, advDeEssIntenSlider;
    ModernSlider advDeEssAtkSlider, advDeEssRelSlider, advDeEssDetectSlider;
    ModernSlider advGateFloorSlider, advCleanRelSlider;

    // Gate advanced
    ModernSlider advGateThresholdSlider, advGateAttackSlider, advGateHoldSlider;
    // Air drive
    ModernSlider advAirDriveSlider;


    // Le curseur AIR FREQ est isolé pour garder son comportement horizontal linéaire personnalisé
    juce::Slider advAirFreqSlider;
    juce::Label airCharacterLabel;

    // Raccourcis d'alias JUCE
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Liste des attachements APVTS
    std::unique_ptr<SliderAttachment> cleanAttach, deEssAttach, roomAttach, reverbAttach, airAttach, masterGainAttach;
    std::unique_ptr<ButtonAttachment> cleanBypassAttach, roomBypassAttach, reverbBypassAttach, deEssBypassAttach, airBypassAttach;
    std::unique_ptr<ButtonAttachment> deltaAttach, roomDeltaAttach, reverbDeltaAttach, deEssDeltaAttach, airDeltaAttach;
    std::unique_ptr<ButtonAttachment> deEssModeAttach;
    std::unique_ptr<SliderAttachment> advRoomFreqAttach, advRoomQAttach;
    std::unique_ptr<SliderAttachment> advDeEssFreqAttach, advDeEssThreshAttach, advDeEssIntenAttach;
    std::unique_ptr<SliderAttachment> advDeEssAtkAttach, advDeEssRelAttach, advDeEssDetectAttach;
    std::unique_ptr<SliderAttachment> advGateFloorAttach, advCleanRelAttach;
    std::unique_ptr<SliderAttachment> advAirFreqAttach;
    std::unique_ptr<SliderAttachment> advGateThresholdAttach, advGateAttackAttach, advGateHoldAttach;
    std::unique_ptr<SliderAttachment> advAirDriveAttach;
    std::unique_ptr<SliderAttachment> nrThreshAttach, nrRatioAttach, nrFloorAttach, nrSmoothAttach;

    // NR — Noise Reduction
    ModernSlider nrThreshSlider, nrRatioSlider, nrFloorSlider, nrSmoothSlider;

    // Fonctions utilitaires de design
    void setupModernSlider(ModernSlider& modernSlider, const juce::String& name);
    void applyModernKnobStyle(juce::Slider& slider);
    void updateBypassAlphas();
    void timerCallback() override;

    bool bypassBlinkOn = true;
    int  blinkCounter  = 0;

    // Nouveaux composants graphiques UX
    juce::TextButton nrLearnBtn;
    bool nrLearnBtnActive{ false };
    juce::TextButton gateAutoBtn;
    std::unique_ptr<DualRailComponent> nrDualRail;
    std::unique_ptr<juce::Component>   nrBandDisplay;
    std::unique_ptr<DualRailComponent>   gateDualRail;
    std::unique_ptr<juce::Component>     gateWaveform;
    std::unique_ptr<juce::Component>   gateAHR;
    std::unique_ptr<juce::Component>   deEssAR;
    std::unique_ptr<DeEssDetectDisplay> deEssDetect;
    float nrVuLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisibleFixAudioProcessorEditor)
};