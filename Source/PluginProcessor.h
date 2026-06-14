#pragma once

#include <JuceHeader.h>

// =============================================================================
//  InvisibleFix — PluginProcessor.h
//  V0.9 — Déclarations du Moteur DSP (Hystérésis, Plosives & Coefficients)
// =============================================================================

class InvisibleFixAudioProcessor : public juce::AudioProcessor
{
public:
    InvisibleFixAudioProcessor();
    ~InvisibleFixAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Fonction de calcul des filtres du tiroir Room
    void updateRoomCoefficients(float room, float focus);

    // APVTS Public pour les liaisons de l'interface (Attachments)
    juce::AudioProcessorValueTreeState apvts;

private:
    // --- PARAMÈTRES AUDIO GLOBAUX ---
    double currentSampleRate{ 44100.0 };
    int currentBlockSize{ 512 };

    // --- COEFFS ET MÉMOIRES : MODULE ROOM (NOTCH & COMPENSATION) ---
    double notchB0{ 1.0 }, notchB1{ 0.0 }, notchB2{ 0.0 };
    double notchA1{ 0.0 }, notchA2{ 0.0 };
    double shelfB0{ 1.0 }, shelfB1{ 0.0 }, shelfB2{ 0.0 };
    double shelfA1{ 0.0 }, shelfA2{ 0.0 };

    double notchZ1[2]{ 0.0, 0.0 }, notchZ2[2]{ 0.0, 0.0 };
    double shelfZ1[2]{ 0.0, 0.0 }, shelfZ2[2]{ 0.0, 0.0 };

    float lastRoomValue{ -999.0f };
    float lastFocusValue{ -999.0f };

    // --- COEFFS ET MÉMOIRES : MODULE CLEAN & DE-ESSER ---
    double cleanShelfZ1[2]{ 0.0, 0.0 }, cleanShelfZ2[2]{ 0.0, 0.0 };
    float smoothedCleanGain[2]{ 1.0f, 1.0f };
    float smoothedGateGain[2]{ 1.0f, 1.0f };
    double deEssSidechainZ1[2]{ 0.0, 0.0 };
    double deEssSidechainZ2[2]{ 0.0, 0.0 };

    // États d'hystérésis pour éviter le bégaiement (Chattering)
    bool gateIsOpen[2]{ true, true };

    // --- MÉMOIRES : FILTRE ANTI-PLOSIVE DYNAMIQUE ---
    double plosiveZ1[2]{ 0.0, 0.0 }, plosiveZ2[2]{ 0.0, 0.0 };

    // --- ENVELOPPE : MODULE REVERB ABSORBER ---
    float reverbEnv[2]{ 0.0f, 0.0f };

    // --- COEFFS ET MÉMOIRES : MODULE AIR & OUTPUT ---
    double airShelfZ1[2]{ 0.0, 0.0 }, airShelfZ2[2]{ 0.0, 0.0 };
    float smoothedOutputGain[2]{ 1.0f, 1.0f };

    // Layout des paramètres de l'arbre APVTS
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisibleFixAudioProcessor)
};