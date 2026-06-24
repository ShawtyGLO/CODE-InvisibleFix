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
    void updateRoomCoefficients(float room);

    // Copie thread-safe des magnitudes FFT vers le visualiseur
    void copyFftMagnitudes(float* dest, int numBins);

    // APVTS Public pour les liaisons de l'interface (Attachments)
    juce::AudioProcessorValueTreeState apvts;

private:
    // --- PARAMÈTRES AUDIO GLOBAUX ---
    double currentSampleRate{ 44100.0 };

    // --- COEFFS ET MÉMOIRES : MODULE ROOM (NOTCH + COMPENSATION) ---
    double notchB0{ 1.0 }, notchB1{ 0.0 }, notchB2{ 0.0 };
    double notchA1{ 0.0 }, notchA2{ 0.0 };
    double shelfB0{ 1.0 }, shelfB1{ 0.0 }, shelfB2{ 0.0 };
    double shelfA1{ 0.0 }, shelfA2{ 0.0 };

    double notchZ1[2]{ 0.0, 0.0 }, notchZ2[2]{ 0.0, 0.0 };
    double shelfZ1[2]{ 0.0, 0.0 }, shelfZ2[2]{ 0.0, 0.0 };

    // --- DELTA ROOM : PASSE-BANDE RÉSONANT (même freq/Q que le notch) ---
    double notchBPB0{ 1.0 }, notchBPB1{ 0.0 }, notchBPB2{ 0.0 };
    double notchBPA1{ 0.0 }, notchBPA2{ 0.0 };
    double notchBPZ1[2]{ 0.0, 0.0 }, notchBPZ2[2]{ 0.0, 0.0 };

    float lastRoomValue{ -999.0f };
    float lastRoomFreq{ -999.0f };
    float lastRoomQ{ -999.0f };

    // --- ÉTAT HPF SIDECHAIN DE-ESS (persistant entre les blocs) ---
    double deEssHpZ1[2]{ 0.0, 0.0 }, deEssHpZ2[2]{ 0.0, 0.0 };

    // --- MÉMOIRES : MODULE CLEAN (GATE + DE-ESS) ---
    double cleanShelfZ1[2]{ 0.0, 0.0 }, cleanShelfZ2[2]{ 0.0, 0.0 };
    float smoothedCleanGain[2]{ 1.0f, 1.0f };
    float smoothedGateGain[2]{ 1.0f, 1.0f };

    // --- DÉTECTEUR RMS GATE (fenêtre glissante ~23ms) ---
    static constexpr int RMS_WINDOW_SIZE = 1024; // ~23ms @ 44100 Hz
    float rmsBuffer[2][RMS_WINDOW_SIZE]{};
    int   rmsWritePos[2]{ 0, 0 };
    float rmsSumSq[2]{ 0.0f, 0.0f };

    // --- SIDECHAIN PASSE-BANDE GATE (HP 200 Hz + LP 3000 Hz en cascade) ---
    double gateSidechainZ1[2]{ 0.0, 0.0 };   // mémoires étage HP
    double gateSidechainZ2[2]{ 0.0, 0.0 };
    double gateSidechainZ1b[2]{ 0.0, 0.0 };  // mémoires étage LP
    double gateSidechainZ2b[2]{ 0.0, 0.0 };

    // États du Gate (hystérésis adaptatif)
    bool gateIsOpen[2]{ true, true };
    int gateHoldCounter[2]{ 0, 0 };

    // --- ENVELOPPE DUAL : MODULE REVERB ABSORBER ---
    float reverbEnv[2]{ 0.0f, 0.0f };     // fast : suit les transitoires
    float reverbEnvSlow[2]{ 0.0f, 0.0f }; // slow : suit la queue de réverb

    // --- COEFFS ET MÉMOIRES : MODULE AIR & OUTPUT ---
    double airShelfZ1[2]{ 0.0, 0.0 }, airShelfZ2[2]{ 0.0, 0.0 };
    float smoothedOutputGain[2]{ 1.0f, 1.0f };

    // --- NR : CROSSOVER LR4 (2× biquads par filtre) ---
    double nrLPLowB0[2]{ 1.0,1.0 }, nrLPLowB1[2]{ 0.0,0.0 }, nrLPLowB2[2]{ 0.0,0.0 };
    double nrLPLowA1[2]{ 0.0,0.0 }, nrLPLowA2[2]{ 0.0,0.0 };
    double nrLPLowZ1[2][2]{ {0.0},{0.0} }, nrLPLowZ2[2][2]{ {0.0},{0.0} };

    double nrHPHighB0[2]{ 1.0,1.0 }, nrHPHighB1[2]{ 0.0,0.0 }, nrHPHighB2[2]{ 0.0,0.0 };
    double nrHPHighA1[2]{ 0.0,0.0 }, nrHPHighA2[2]{ 0.0,0.0 };
    double nrHPHighZ1[2][2]{ {0.0},{0.0} }, nrHPHighZ2[2][2]{ {0.0},{0.0} };

    // --- NR : ENVELOPPES, FLOOR, GAIN PAR BANDE ---
    float nrEnv[3][2]{ 0.0f,0.0f, 0.0f,0.0f, 0.0f,0.0f };
    float nrFloor[3][2]{ 0.0f,0.0f, 0.0f,0.0f, 0.0f,0.0f };
    float nrSmoothGain[3][2]{ 1.0f,1.0f, 1.0f,1.0f, 1.0f,1.0f };

    // --- FFT HEATMAP (ROOM) ---
    static constexpr int fftSize = 4096;
    static constexpr int fftOrder = 12;
    float fftBuffer[fftSize]{ 0.0f };
    float fftMagnitudes[fftSize / 2]{ 0.0f };
    int fftWriteIndex{ 0 };
    juce::CriticalSection fftMutex;

    // Layout des paramètres de l'arbre APVTS
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisibleFixAudioProcessor)
};