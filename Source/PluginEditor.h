#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class RoomVisualizer;

class InvisibleFixAudioProcessorEditor : public juce::AudioProcessorEditor
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
    juce::Component* roomVisualizer{ nullptr };

    enum class AdvancedTab { None, Clean, Room, Air };
    AdvancedTab currentTab = AdvancedTab::None;

    // Façade principale
    ModernSlider cleanSlider, roomSlider, reverbSlider, airSlider, focusSlider;
    juce::Slider deEssSlider;       // <--- Placé ici de manière propre (juce::Slider brut)
    juce::Slider masterGainSlider;
    juce::Label masterGainLabel;

    CustomButton deltaBtn;
    CustomButton deEssModeBtn;

    // Configuration avancée (Tiroirs)
    ModernSlider advRoomFreqSlider, advRoomQSlider;
    ModernSlider advDeEssIntenSlider, advDeEssFreqSlider, advDeEssRelSlider;
    ModernSlider advGateFloorSlider, advCleanRelSlider, advPlosiveSlider;

    // Le curseur AIR FREQ est isolé pour garder son comportement horizontal linéaire personnalisé
    juce::Slider advAirFreqSlider;
    juce::Label airCharacterLabel;

    // Raccourcis d'alias JUCE
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Liste des attachements APVTS
    std::unique_ptr<SliderAttachment> cleanAttach, deEssAttach, roomAttach, reverbAttach, airAttach, focusAttach, masterGainAttach;
    std::unique_ptr<ButtonAttachment> deltaAttach, deEssModeAttach;
    std::unique_ptr<SliderAttachment> advRoomFreqAttach, advRoomQAttach;
    std::unique_ptr<SliderAttachment> advDeEssIntenAttach, advDeEssFreqAttach, advDeEssRelAttach;
    std::unique_ptr<SliderAttachment> advGateFloorAttach, advCleanRelAttach, advPlosiveAttach;
    std::unique_ptr<SliderAttachment> advAirFreqAttach;

    // Fonctions utilitaires de design
    void setupModernSlider(ModernSlider& modernSlider, const juce::String& name);
    void applyModernKnobStyle(juce::Slider& slider);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisibleFixAudioProcessorEditor)
};