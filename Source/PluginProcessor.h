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

    // Copie thread-safe des magnitudes FFT vers le visualiseur (post-room)
    void copyNRBandGains(float* dest, int numBands);
    void copyFftMagnitudes(float* dest, int numBins);

    // Copie des magnitudes FFT de l'entrée brute (pré-tout-traitement, actif même en Delta)
    void copyInputFftMagnitudes(float* dest, int numBins);

    // APVTS Public pour les liaisons de l'interface (Attachments)
    juce::AudioProcessorValueTreeState apvts;

    // NR Snapshot — accès public pour l'UI (atomics thread-safe)
    bool  nrSnapshotActive{ false };
    std::atomic<bool> nrSnapshotRequest{ false };
    std::atomic<bool> nrSnapshotRelease{ false };

    // Gate auto-threshold — données exposées vers l'UI (lecture seule depuis UI thread)
    float gateHistogram[80]{};          // histogramme niveaux -80..0 dBFS (1 dB/bucket)
    float gateCurrentLevelDB{ -80.0f }; // niveau RMS sidechain courant
    float gateAutoThreshDB{ -40.0f };   // seuil optimal détecté
    float gateAutoZoneLow{ -80.0f };    // borne basse de la zone optimale
    float gateAutoZoneHigh{ -80.0f };   // borne haute de la zone optimale
    bool  gateAutoZoneValid{ false };   // fiabilité de la détection
    bool  gateIsOpenForDisplay{ true }; // état gate canal 0, mis à jour dans processBlock

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
    int gateHistCounter{ 0 };  // compteur pour mise à jour histogramme (canal 0 seulement)

    // --- DE-REV 3 BANDES (Cedar/Eventide-style) ---
    // Banc de filtres LP complémentaires : coupures 500 Hz / 4000 Hz
    double revSplitZ1[2][2]{};   // états biquad [coupure 0-1][canal]
    double revSplitZ2[2][2]{};
    // Sidechain 200-3000 Hz pour détection vocale (évite pompage sur résidus basse fréquence)
    double revScHPZ1[2]{}, revScHPZ2[2]{};  // HP 200 Hz
    double revScLPZ1[2]{}, revScLPZ2[2]{};  // LP 3000 Hz
    // Enveloppes par bande [bande 0-2][canal]
    float revBandFast[3][2]{};   // enveloppe rapide (transitoires)
    float revBandSlow[3][2]{};   // enveloppe lente (queue de réverb)

    // --- DE-ESS : LOOK-AHEAD + SEUIL ADAPTATIF ---
    static constexpr int DEESS_LOOKAHEAD_MAX = 512;
    float deEssLookahead[2][DEESS_LOOKAHEAD_MAX]{};
    int   deEssLookaheadWrite[2]{ 0, 0 };
    int   deEssLookaheadLen{ 88 };   // recalculé dans prepareToPlay (= 2ms)
    float deEssAdaptEnv[2]{ 0.0f, 0.0f };  // slow follower du niveau sibilant

    // --- ENVELOPPE DUAL : MODULE REVERB ABSORBER (legacy, conservé pour compatibilité) ---
    float reverbEnv[2]{ 0.0f, 0.0f };
    float reverbEnvSlow[2]{ 0.0f, 0.0f };

    // --- COEFFS ET MÉMOIRES : MODULE AIR & OUTPUT ---
    double airShelfZ1[2]{ 0.0, 0.0 }, airShelfZ2[2]{ 0.0, 0.0 };
    // Filtre HPF du chemin Heat (parallèle, indépendant du shelf)
    double heatHPZ1[2]{ 0.0, 0.0 }, heatHPZ2[2]{ 0.0, 0.0 };
    float smoothedOutputGain[2]{ 1.0f, 1.0f };

    // --- NR CEDAR 6 BANDES : banc de filtres LP complémentaires ---
    double nrSplitZ1[5][2]{};
    double nrSplitZ2[5][2]{};
    float nrBandEnv[6][2]{};
    float nrBandPrevEnv[6][2]{};
    float nrNoiseFloor[6][2]{};
    float nrBandGain[6][2]{};

    // --- NR FLOOR ESTIMATION : rolling minimum (deux demi-fenêtres de 0.5s) ---
    int   nrWindowPeriod{ 22050 };         // recalculé dans prepareToPlay
    int   nrHistCounter[2]{ 0, 0 };
    float nrCurrWindowMin[6][2]{};         // minimum de la demi-fenêtre courante
    float nrPrevWindowMin[6][2]{};         // minimum de la demi-fenêtre précédente
    float nrRollingMin[6][2]{};            // min(curr, prev) = estimation plancher

    // --- NR SNAPSHOT (bruit de référence capturé manuellement) ---
    float nrSnapshotFloor[6][2]{};

    // --- FFT HEATMAP (ROOM, post-traitement) ---
    static constexpr int fftSize = 4096;
    static constexpr int fftOrder = 12;
    float fftBuffer[fftSize]{ 0.0f };
    float fftMagnitudes[fftSize / 2]{ 0.0f };
    int fftWriteIndex{ 0 };

    // --- FFT ENTRÉE BRUTE (pré-tout-traitement, pour visualiseurs en mode Delta) ---
    float inputFftBuffer[fftSize]{ 0.0f };
    float inputFftMagnitudes[fftSize / 2]{ 0.0f };
    int inputFftWriteIndex{ 0 };

    juce::CriticalSection fftMutex;

    // Layout des paramètres de l'arbre APVTS
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InvisibleFixAudioProcessor)
};