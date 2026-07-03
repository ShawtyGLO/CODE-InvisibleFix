#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class RoomVisualizer;
class DualRailComponent;
class AHREnvelopeEditor;
class DeEssDetectDisplay;
class PresetBrowserPanel;
class DeRevCanvas;

class InvisibleFixAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    InvisibleFixAudioProcessorEditor(InvisibleFixAudioProcessor&);
    ~InvisibleFixAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void updatePresetBar();      // public : appelé depuis PresetBrowserPanel
    void togglePresetBrowser();  // public : appelé depuis browsePresetBtn

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

    enum class AdvancedTab { None, Clean, Room, Air, DeEss, DeRev };
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
    ModernSlider advRoomFreqSlider, advRoomQSlider, advRoomDepthBoostSlider;
    ModernSlider advDeEssFreqSlider, advDeEssThreshSlider, advDeEssIntenSlider;
    ModernSlider advDeEssAtkSlider, advDeEssRelSlider, advDeEssDetectSlider;
    ModernSlider advGateFloorSlider, advCleanRelSlider;

    // Gate advanced
    ModernSlider advGateThresholdSlider, advGateAttackSlider, advGateHoldSlider;
    ModernSlider plosiveProtectSlider;
    // Air drive
    ModernSlider advAirDriveSlider;

    // Air saturation mode — 4 radio buttons (TRIODE / CLASS-A / TAPE / IRON)
    juce::TextButton airSatTriodeBtn, airSatClassABtn, airSatTapeBtn, airSatIronBtn;

    // Air transient shaper bipolaire (-1..+1)
    juce::Slider advAirShapeSlider;
    juce::Label  advAirShapeLabel;


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
    std::unique_ptr<ButtonAttachment> nrBypassAttach, gateBypassAttach;
    std::unique_ptr<SliderAttachment> advRoomFreqAttach, advRoomQAttach, advRoomDepthBoostAttach;

    // Metering I/O + gains d'entrée/sortie
    juce::Slider inputGainSlider, outputGainSlider;
    std::unique_ptr<SliderAttachment> inputGainAttach, outputGainAttach;
    float meterInputPeak{ 0.f }, meterOutputPeak{ 0.f };
    float meterInputHold{ 0.f }, meterOutputHold{ 0.f };
    int   meterInputHoldTimer{ 0 }, meterOutputHoldTimer{ 0 };

    // A/B Snapshot (session only — pas de persistance disque)
    juce::TextButton abBtnA, abArrowBtn, abBtnB;
    juce::MemoryBlock snapshotA, snapshotB;
    bool hasSnapshotA{ false }, hasSnapshotB{ false };
    int  abActiveSlot{ -1 }; // -1=aucun, 0=A, 1=B
    void abSaveSlot(int slot);
    void abRecallSlot(int slot);
    void abUpdateButtons();
    std::unique_ptr<SliderAttachment> advDeEssFreqAttach, advDeEssThreshAttach, advDeEssIntenAttach;
    std::unique_ptr<SliderAttachment> advDeEssAtkAttach, advDeEssRelAttach, advDeEssDetectAttach;
    std::unique_ptr<SliderAttachment> advGateFloorAttach, advCleanRelAttach;
    std::unique_ptr<SliderAttachment> advAirFreqAttach;
    std::unique_ptr<SliderAttachment> advGateThresholdAttach, advGateAttackAttach, advGateHoldAttach;
    std::unique_ptr<SliderAttachment> advAirDriveAttach;
    std::unique_ptr<SliderAttachment> advAirShapeAttach;
    std::unique_ptr<SliderAttachment> plosiveProtectAttach;
    std::unique_ptr<SliderAttachment> nrThreshAttach, nrRatioAttach, nrFloorAttach, nrSmoothAttach;
    std::unique_ptr<SliderAttachment> nrArtifactBlendAttach;

    // De-Rev Floor + band intensity
    ModernSlider deRevFloorSlider;
    ModernSlider deRevBandLFSlider, deRevBandMFSlider, deRevBandHFSlider;
    std::unique_ptr<SliderAttachment> deRevFloorAttach;
    std::unique_ptr<SliderAttachment> deRevBandLFAttach, deRevBandMFAttach, deRevBandHFAttach;

    // De-Rev Canvas (visualisation dual-axis + signal live)
    std::unique_ptr<DeRevCanvas> deRevCanvas;
    juce::TextButton deRevLinkBtn;

    // NR — Noise Reduction
    ModernSlider nrThreshSlider, nrRatioSlider, nrFloorSlider, nrSmoothSlider;
    ModernSlider nrArtifactBlendSlider;

    // Fonctions utilitaires de design
    void setupModernSlider(ModernSlider& modernSlider, const juce::String& name);
    void applyModernKnobStyle(juce::Slider& slider);
    void updateBypassAlphas();
    void timerCallback() override;
    void mouseMove(const juce::MouseEvent&) override;

    bool bypassBlinkOn = true;
    int  blinkCounter  = 0;

    // Nouveaux composants graphiques UX
    juce::TextButton nrLearnBtn;
    bool nrLearnBtnActive{ false };
    juce::TextButton nrBypassBtn;
    juce::TextButton gateBypassBtn;
    juce::TextButton gateAutoBtn;
    std::unique_ptr<DualRailComponent> nrDualRail;
    std::unique_ptr<juce::Component>   nrBandDisplay;  // forward-declared as Component, cast to NRSpectrumDisplay
    std::unique_ptr<DualRailComponent>   gateDualRail;
    std::unique_ptr<juce::Component>     gateWaveform;
    std::unique_ptr<juce::Component>   gateAHR;
    std::unique_ptr<juce::Component>   deEssAR;
    std::unique_ptr<DeEssDetectDisplay> deEssDetect;
    float nrVuLevel = 0.0f;

    // AUTO gate : lissage du seuil détecté (remplace les static dans timerCallback)
    float autoSmoothValley{ -40.0f };
    bool  autoHasValley{ false };

    // Bypass global A/B
    juce::TextButton globalBypassBtn;

    // LIVE/HQ mode toggle (top-right)
    juce::TextButton liveHQBtn;

    // LAF totalement transparent pour le bypass zone (empêche JUCE de dessiner une bordure)
    struct TransparentLAF : juce::LookAndFeel_V4 {
        void drawButtonBackground(juce::Graphics&, juce::Button&,
                                   const juce::Colour&, bool, bool) override {}
    };
    TransparentLAF bypassZoneLAF;

    // ── Barre preset (style Arturia) ─────────────────────────────────────────
    juce::TextButton prevPresetBtn;
    juce::TextButton nextPresetBtn;
    juce::TextButton savePresetBtn;
    juce::TextButton initPresetBtn;
    juce::TextButton browsePresetBtn;
    juce::Label      presetNameLabel;
    juce::Label      presetTypeLabel;   // "FACTORY" ou "USER"
    bool             presetNameEditing{ false };

    // ── Browser de presets (panneau flottant scrollable) ─────────────────────
    std::unique_ptr<PresetBrowserPanel> presetBrowser;

    // ── Resize avec snap ──────────────────────────────────────────────────────
    float currentScale{ 1.0f };

    // Constrainer qui snap PENDANT le drag (checkBounds = avant resize)
    class ScaleConstrainer : public juce::ComponentBoundsConstrainer {
    public:
        AdvancedTab* tabPtr{ nullptr };
        float        currentSnap{ 1.0f }; // palier actif — avec hystérésis
        void checkBounds(juce::Rectangle<int>& bounds,
                         const juce::Rectangle<int>& previousBounds,
                         const juce::Rectangle<int>& limits,
                         bool isStretchingTop, bool isStretchingLeft,
                         bool isStretchingBottom, bool isStretchingRight) override;
    };
    ScaleConstrainer resizeConstrainer;

    // Undo / Redo buttons
    juce::TextButton undoBtn, redoBtn;

    // Tooltip bar
    juce::String currentTooltipText;
    juce::String currentTooltipParam;

    // Indicateur de scale cliquable (bottom-right)
    juce::TextButton scaleBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisibleFixAudioProcessorEditor)
};