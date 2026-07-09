#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// =============================================================================
//  InvisibleFix — PluginProcessor.cpp
//  V1.00 — Séparation Totale Clean (Gate) / DeEss & Delta Dynamique Pur
// =============================================================================

namespace {
    // Hann window in-place (interleaved complex: real at even, imag at odd indices)
    void applyHannWindow(float* data, int numSamples) {
        for (int i = 0; i < numSamples; ++i)
            data[i * 2] *= 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / (numSamples - 1)));
    }

    // Radix-2 in-place FFT (interleaved complex)
    void fftRadix2(float* data, int n) {
        // Bit reversal
        for (int i = 1, j = 0; i < n - 1; ++i) {
            int bit = n >> 1;
            while (j & bit) { j ^= bit; bit >>= 1; }
            j ^= bit;
            if (i < j) { std::swap(data[i * 2], data[j * 2]); std::swap(data[i * 2 + 1], data[j * 2 + 1]); }
        }
        // FFT butterflies
        for (int len = 2; len <= n; len <<= 1) {
            float wAngle = -2.0f * 3.14159265f / len;
            float wRe = std::cos(wAngle), wIm = std::sin(wAngle);
            for (int i = 0; i < n; i += len) {
                float curRe = 1.0f, curIm = 0.0f;
                for (int j = 0; j < len / 2; ++j) {
                    int a = (i + j) * 2, b = (i + j + len / 2) * 2;
                    float tRe = curRe * data[b] - curIm * data[b + 1];
                    float tIm = curRe * data[b + 1] + curIm * data[b];
                    data[b] = data[a] - tRe; data[b + 1] = data[a + 1] - tIm;
                    data[a] += tRe; data[a + 1] += tIm;
                    float newRe = curRe * wRe - curIm * wIm;
                    curIm = curRe * wIm + curIm * wRe;
                    curRe = newRe;
                }
            }
        }
    }
}

InvisibleFixAudioProcessor::InvisibleFixAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    apvts(*this, &undoManager, "Parameters", createParameterLayout())
#endif
{
    // Chaque geste (mouseUp sur slider, clic bouton) = une étape undo indépendante
    // maxNumberOfUnitsToKeep : chaque float stocke ~15 "unités" (longueur string)
    // minimumTransactionsToKeep : garde toujours au moins 50 transactions (undo steps)
    undoManager.setMaxNumberOfStoredUnits(30000000, 50);
    buildPresetList();
    currentPresetName      = presetNames.empty() ? "Init" : presetNames[0];
    currentPresetIsFactory = true;
}

InvisibleFixAudioProcessor::~InvisibleFixAudioProcessor() {}

const juce::String InvisibleFixAudioProcessor::getName() const { return JucePlugin_Name; }
bool InvisibleFixAudioProcessor::acceptsMidi() const { return false; }
bool InvisibleFixAudioProcessor::producesMidi() const { return false; }
bool InvisibleFixAudioProcessor::isMidiEffect() const { return false; }
double InvisibleFixAudioProcessor::getTailLengthSeconds() const { return 0.0; }
// getNumPrograms / setCurrentProgram / etc. → implémentés dans la section Preset Manager

void InvisibleFixAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (int ch = 0; ch < 2; ++ch)
    {
        notchZ1[ch] = 0.0;  notchZ2[ch] = 0.0;
        shelfZ1[ch] = 0.0;  shelfZ2[ch] = 0.0;

        cleanShelfZ1[ch] = 0.0;  cleanShelfZ2[ch] = 0.0;
        smoothedCleanGain[ch] = 1.0f;
        smoothedGateGain[ch] = 1.0f;

        reverbEnv[ch] = 0.0f;
        reverbEnvSlow[ch] = 0.0f;
        gateIsOpen[ch] = true;
        gateHoldCounter[ch] = 0;
        if (ch == 0) {
            gateHistCounter = 0;
            std::fill(std::begin(gateHistogram), std::end(gateHistogram), 0.0f);
            gateCurrentLevelDB = -80.0f;
            gateAutoZoneValid  = false;
        }

        // De-Rev 3 bandes
        for (int s = 0; s < 2; ++s) { revSplitZ1[s][ch] = 0.0; revSplitZ2[s][ch] = 0.0; }
        revScHPZ1[ch] = revScHPZ2[ch] = revScLPZ1[ch] = revScLPZ2[ch] = 0.0;
        for (int b = 0; b < 3; ++b) { revBandFast[b][ch] = 0.0f; revBandSlow[b][ch] = 0.0f; }
        revTransientEnv[ch]  = 0.f;
        revTransientHold[ch] = 0;
        if (ch == 0) for (int b = 0; b < 3; ++b) { revBandLiveEnergy[b].store(0.f); revBandLiveGR[b].store(1.f); }

        // De-Ess look-ahead + adaptatif + reference band
        deEssLookaheadWrite[ch] = 0;
        deEssAdaptEnv[ch] = 0.0f;
        std::memset(deEssLookahead[ch], 0, sizeof(float) * DEESS_LOOKAHEAD_MAX);
        deEssHpZ1[ch] = deEssHpZ2[ch] = 0.0;
        deEssRefZ1[ch] = deEssRefZ2[ch] = 0.0;

        rmsWritePos[ch] = 0;
        rmsSumSq[ch]    = 0.0f;
        for (int i = 0; i < RMS_WINDOW_SIZE; ++i)
            rmsBuffer[ch][i] = 0.0f;
        gateSidechainZ1[ch]  = 0.0; gateSidechainZ2[ch]  = 0.0;
        gateSidechainZ1b[ch] = 0.0; gateSidechainZ2b[ch] = 0.0;

        airShelfZ1[ch] = 0.0; airShelfZ2[ch] = 0.0;
        heatHPZ1[ch]  = 0.0; heatHPZ2[ch]  = 0.0;
        smoothedOutputGain[ch] = 1.0f;
        airFastEnv[ch] = 0.0f;
        airSlowEnv[ch] = 0.0f;

        for (int s = 0; s < 5; ++s) { nrSplitZ1[s][ch] = 0.0; nrSplitZ2[s][ch] = 0.0; }
        for (int b = 0; b < 6; ++b) {
            nrBandEnv[b][ch]      = 0.0f;
            nrBandPrevEnv[b][ch]  = 0.0f;
            nrNoiseFloor[b][ch]   = 0.0f;
            nrBandGain[b][ch]     = 1.0f;
            nrCurrWindowMin[b][ch] = 1e-4f;   // prior −80 dBFS, corrigé dès la 1ère demi-fenêtre
            nrPrevWindowMin[b][ch] = 1e-4f;
            nrRollingMin[b][ch]    = 1e-4f;
            nrSnapshotFloor[b][ch] = 1e-4f;
        }
        nrHistCounter[ch] = 0;
    }

    lastRoomValue = -999.0f;

    deEssLookaheadLen = juce::jlimit(1, DEESS_LOOKAHEAD_MAX - 1,
                                     (int)(0.002f * (float)currentSampleRate));

    // Gate lookahead : 3ms
    gateLookaheadLen = juce::jlimit(1, GATE_LOOKAHEAD_MAX - 1,
                                    (int)(0.003 * sampleRate));
    for (int ch = 0; ch < 2; ++ch) {
        std::memset(gateLookahead[ch], 0, sizeof(float) * GATE_LOOKAHEAD_MAX);
        gateLookaheadWrite[ch] = 0;
    }
    // Latence totale = De-Ess lookahead + Gate lookahead
    setLatencySamples(deEssLookaheadLen + gateLookaheadLen);
    nrWindowPeriod = juce::jmax(1024, (int)(0.5f * (float)currentSampleRate));

    // Coefficients du détecteur plosives (LP 250 Hz + enveloppes)
    {
        double sr2 = currentSampleRate;
        double wc  = 2.0 * juce::MathConstants<double>::pi * 250.0 / sr2;
        double cosW = std::cos(wc);
        double alph = std::sin(wc) / (2.0 * 0.707);
        double a0   = 1.0 + alph;
        plosiveLPB0 = (1.0 - cosW) * 0.5 / a0;
        plosiveLPB1 = (1.0 - cosW)       / a0;
        plosiveLPB2 = (1.0 - cosW) * 0.5 / a0;
        plosiveLPA1 = -2.0 * cosW        / a0;
        plosiveLPA2 = (1.0 - alph)       / a0;
        float fsr = (float)sr2;
        plosiveFastAtkCoef = std::exp(-1.0f / (0.002f * fsr));
        plosiveFastRelCoef = std::exp(-1.0f / (0.025f * fsr));
        plosiveSlowAtkCoef = std::exp(-1.0f / (0.060f * fsr));
        plosiveSlowRelCoef = std::exp(-1.0f / (0.250f * fsr));
        for (int ch = 0; ch < 2; ++ch) {
            plosiveLPZ1[ch] = plosiveLPZ2[ch] = 0.0;
            plosiveFastEnv[ch] = plosiveSlowEnv[ch] = 0.0f;
        }
    }

    // Coefficients du transient shaper Air (dépendent du sample rate)
    float sr = (float)currentSampleRate;
    airShapeFastAtkCoef = std::exp(-1.0f / (0.001f * sr));
    airShapeFastRelCoef = std::exp(-1.0f / (0.010f * sr));
    airShapeSlowAtkCoef = std::exp(-1.0f / (0.015f * sr));
    airShapeSlowRelCoef = std::exp(-1.0f / (0.080f * sr));

    // Initialiser le suivi de freq Room depuis le parametre courant (evite cold start a 440 Hz)
    float initFreq = *apvts.getRawParameterValue("roomNotchFreq");
    dynNotchSmoothedFreq  = initFreq;
    lastRoomFreq          = -999.0f; // force recalcul des coefficients au premier bloc
    dynNotchCoeffsStale   = true;
}

void InvisibleFixAudioProcessor::releaseResources() {}

bool InvisibleFixAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void InvisibleFixAudioProcessor::updateRoomCoefficients(float room)
{
    double fc = (double)*apvts.getRawParameterValue("roomNotchFreq");
    fc = juce::jlimit(100.0, 2000.0, fc);

    double Q = (double)*apvts.getRawParameterValue("roomNotchQ");
    Q = juce::jlimit(0.5, 15.0, Q);

    double maxAttenDB = -16.0 * (double)room;
    maxAttenDB = juce::jlimit(-16.0, 0.0, maxAttenDB);
    double A = std::pow(10.0, maxAttenDB / 40.0);

    double w0 = juce::MathConstants<double>::twoPi * fc / currentSampleRate;
    double cosW0 = std::cos(w0);
    double sinW0 = std::sin(w0);
    double alpha = sinW0 / (2.0 * Q);

    double b0 = 1.0 + alpha * A;
    double b1 = -2.0 * cosW0;
    double b2 = 1.0 - alpha * A;
    double a0 = 1.0 + alpha / A;
    double a1 = -2.0 * cosW0;
    double a2 = 1.0 - alpha / A;

    notchB0 = b0 / a0;  notchB1 = b1 / a0;  notchB2 = b2 / a0;
    notchA1 = a1 / a0;  notchA2 = a2 / a0;

    // Passe-bande résonnant (delta Room) — même fréquence/Q
    double alphaBP = sinW0 / (2.0 * Q);
    notchBPB0 = alphaBP / (1.0 + alphaBP);
    notchBPB1 = 0.0;
    notchBPB2 = -alphaBP / (1.0 + alphaBP);
    notchBPA1 = -2.0 * cosW0 / (1.0 + alphaBP);
    notchBPA2 = (1.0 - alphaBP) / (1.0 + alphaBP);

    double shelfGainDB = -maxAttenDB * 0.15;
    double Ashelf = std::pow(10.0, shelfGainDB / 40.0);

    double w0s = juce::MathConstants<double>::twoPi * 3000.0 / currentSampleRate;
    double cosW0s = std::cos(w0s);
    double sinW0s = std::sin(w0s);
    double alphaS = sinW0s / (2.0 * 0.707) * std::sqrt((Ashelf + 1.0 / Ashelf) * (1.0 / 0.707 - 1.0) + 2.0);

    double sb0 = Ashelf * ((Ashelf + 1.0) + (Ashelf - 1.0) * cosW0s + 2.0 * std::sqrt(Ashelf) * alphaS);
    double sb1 = -2.0 * Ashelf * ((Ashelf - 1.0) + (Ashelf + 1.0) * cosW0s);
    double sb2 = Ashelf * ((Ashelf + 1.0) + (Ashelf - 1.0) * cosW0s - 2.0 * std::sqrt(Ashelf) * alphaS);
    double sa0 = (Ashelf + 1.0) - (Ashelf - 1.0) * cosW0s + 2.0 * std::sqrt(Ashelf) * alphaS;
    double sa1 = 2.0 * ((Ashelf - 1.0) - (Ashelf + 1.0) * cosW0s);
    double sa2 = (Ashelf + 1.0) - (Ashelf - 1.0) * cosW0s - 2.0 * std::sqrt(Ashelf) * alphaS;

    shelfB0 = sb0 / sa0;  shelfB1 = sb1 / sa0;  shelfB2 = sb2 / sa0;
    shelfA1 = sa1 / sa0;  shelfA2 = sa2 / sa0;
}

// =============================================================================
//  processBlock
// =============================================================================
void InvisibleFixAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // ── Bypass global : passe l'audio sans traitement ─────────────────────────
    if (globalBypassActive.load())
        return;

    // --- LECTURE DES PARAMÈTRES DE FAÇADE ---
    float knobClean = *apvts.getRawParameterValue("clean");
    float knobDeEss = *apvts.getRawParameterValue("deess");
    float knobRoom = *apvts.getRawParameterValue("room");
    float knobReverb = *apvts.getRawParameterValue("reverb");
    float knobAir = *apvts.getRawParameterValue("air");
    float knobOutputGain = *apvts.getRawParameterValue("outputGain");
    float knobInputGain  = *apvts.getRawParameterValue("inputGain");
    float inputGainLin   = juce::Decibels::decibelsToGain(knobInputGain);

    // Appliquer le gain d'entrée avant tout traitement + mesurer le niveau
    float inputPeak = 0.f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            d[s] *= inputGainLin;
            inputPeak = std::max(inputPeak, std::abs(d[s]));
        }
    }
    // Decay exponentiel : retient le pic ~200ms
    {
        float prev = inputLevelDisplay.load();
        float decay = std::exp(-1.f / (0.200f * (float)currentSampleRate / (float)buffer.getNumSamples()));
        inputLevelDisplay.store(std::max(inputPeak, prev * decay));
    }

    // --- PARAMÈTRES AVANCÉS ---
    bool cleanBypass   = *apvts.getRawParameterValue("cleanBypass") > 0.5f;
    bool roomBypass    = *apvts.getRawParameterValue("roomBypass") > 0.5f;
    bool reverbBypass  = *apvts.getRawParameterValue("reverbBypass") > 0.5f;
    bool deessBypass   = *apvts.getRawParameterValue("deessBypass") > 0.5f;
    bool airBypass     = *apvts.getRawParameterValue("airBypass") > 0.5f;
    bool cleanDelta    = *apvts.getRawParameterValue("cleanDelta") > 0.5f;
    bool roomDelta     = *apvts.getRawParameterValue("roomDelta") > 0.5f;
    bool reverbDelta   = *apvts.getRawParameterValue("reverbDelta") > 0.5f;
    bool deessDelta    = *apvts.getRawParameterValue("deessDelta") > 0.5f;
    bool airDelta      = *apvts.getRawParameterValue("airDelta") > 0.5f;

    float advAirFreq = *apvts.getRawParameterValue("airFreq");
    float advDeEssInten = *apvts.getRawParameterValue("deEssIntensity");
    float advDeEssFreq = *apvts.getRawParameterValue("deEssFreq");
    bool  isBellMode = *apvts.getRawParameterValue("deEssFilterMode") > 0.5f;
    float advDeEssRel = *apvts.getRawParameterValue("deEssRelease");
    float advDeEssAtk = *apvts.getRawParameterValue("deEssAttack");
    float advGateFloorDB = *apvts.getRawParameterValue("cleanGateFloor");
    float advCleanRelease = *apvts.getRawParameterValue("cleanRelease");
    float advGateThresholdDB = *apvts.getRawParameterValue("cleanGateThreshold");
    float advGateAttack = *apvts.getRawParameterValue("cleanGateAttack");
    float advGateHold = *apvts.getRawParameterValue("cleanGateHold");
    float plosiveProtect = *apvts.getRawParameterValue("cleanPlosiveProtect");
    float advDeEssThreshDB = *apvts.getRawParameterValue("deEssThreshold");
    float advDeEssDetectRatio = *apvts.getRawParameterValue("deEssDetectRatio");
    float advAirDrive = *apvts.getRawParameterValue("airDrive");
    float advAirShape = *apvts.getRawParameterValue("airShape");
    float nrArtifactBlend = *apvts.getRawParameterValue("nrArtifactBlend");
    bool  liveModeActive  = *apvts.getRawParameterValue("liveMode")   > 0.5f;
    bool  nrBypass        = *apvts.getRawParameterValue("nrBypass")   > 0.5f;
    bool  gateBypass      = *apvts.getRawParameterValue("gateBypass") > 0.5f;
    float deRevBandIntensity[3] = {
        *apvts.getRawParameterValue("deRevBandLF"),
        *apvts.getRawParameterValue("deRevBandMF"),
        *apvts.getRawParameterValue("deRevBandHF")
    };
    float deRevThresh[3] = {
        *apvts.getRawParameterValue("deRevThreshLF"),
        *apvts.getRawParameterValue("deRevThreshMF"),
        *apvts.getRawParameterValue("deRevThreshHF")
    };
    float deRevFloor = *apvts.getRawParameterValue("deRevFloor");
    float revTransAtkCoef  = 1.f - std::exp(-1.f / (0.001f * (float)currentSampleRate));
    float revTransRelCoef  = 1.f - std::exp(-1.f / (0.080f * (float)currentSampleRate));
    int   revTransHoldLen  = (int)(0.020f * (float)currentSampleRate);

    // Mise à jour latence déclarée au DAW si mode LIVE/HQ change
    if (liveModeActive != lastLiveMode)
    {
        lastLiveMode = liveModeActive;
        setLatencySamples(liveModeActive ? 0 : deEssLookaheadLen + gateLookaheadLen);
    }

    float currentRoomQ = *apvts.getRawParameterValue("roomNotchQ");

    // ── Notch hybride : freq tracking lisse + profondeur dynamique chirurgicale ──
    if (knobRoom > 0.001f || roomDelta)
    {
        float sr = (float)currentSampleRate;
        float manualFreq = *apvts.getRawParameterValue("roomNotchFreq");

        // --- 1. Peak detection dans ±15% de manualFreq ---
        // Le slider est un "aimant" : le système trouve le pic exact autour de lui
        float searchLo   = manualFreq * 0.85f;
        float searchHi   = manualFreq * 1.15f;
        int binSearchLo  = juce::jmax(1, (int)(searchLo * (float)fftSize / sr));
        int binSearchHi  = juce::jmin(fftSize / 2 - 1, (int)(searchHi * (float)fftSize / sr));

        float peakMag    = 0.0f;
        int   peakBin    = juce::jlimit(1, fftSize / 2 - 1,
                                        (int)(manualFreq * (float)fftSize / sr));
        for (int b = binSearchLo; b <= binSearchHi; ++b)
        {
            if (inputFftMagnitudes[b] > peakMag)
            {
                peakMag = inputFftMagnitudes[b];
                peakBin = b;
            }
        }
        float detectedFreq = (float)peakBin * sr / (float)fftSize;

        // --- 2. Suivi de fréquence lisse (~1-2s pour traverser la zone) ---
        // Évite les sauts brusques de fréquence qui créent des artéfacts
        dynNotchSmoothedFreq = dynNotchSmoothedFreq * 0.992f + detectedFreq * 0.008f;
        float trackedFreq = dynNotchSmoothedFreq;

        // --- 3. Proéminence autour de la fréquence trackée ---
        int binCenter = juce::jlimit(1, fftSize / 2 - 1,
                                     (int)(trackedFreq * (float)fftSize / sr));
        int halfBW    = juce::jmax(1, (int)(binCenter / (float)currentRoomQ));
        int binLo     = juce::jmax(1, binCenter - halfBW);
        int binHi     = juce::jmin(fftSize / 2 - 1, binCenter + halfBW);

        float bandEnergy = 0.0f;
        for (int b = binLo; b <= binHi; ++b)
            bandEnergy += inputFftMagnitudes[b];
        bandEnergy /= (float)(binHi - binLo + 1);

        int wLo = juce::jmax(1, (int)(80.0f   * (float)fftSize / sr));
        int wHi = juce::jmin(fftSize / 2 - 1, (int)(1200.0f * (float)fftSize / sr));
        float wideEnergy = 0.0f;
        for (int b = wLo; b <= wHi; ++b) wideEnergy += inputFftMagnitudes[b];
        wideEnergy /= (float)(wHi - wLo + 1);

        // --- 4. Profondeur chirurgicale ---
        // Seuil 1.5 (vs 1.2) : déclenche uniquement sur les vraies résonances
        // Pente /3.0 (vs /5.0) : coupe franchement quand c'est bien présent
        float prominence  = (wideEnergy > 1e-12f) ? (bandEnergy / (wideEnergy + 1e-12f)) : 1.0f;
        float targetDepth = juce::jlimit(0.0f, 1.0f, (prominence - 1.2f) / 2.0f);

        // Lissage asymétrique : attaque rapide (capte vite), release lent (évite pompage)
        float smoothCoef  = (targetDepth > dynNotchSmoothedDepth) ? 0.970f : 0.985f;
        dynNotchSmoothedDepth = dynNotchSmoothedDepth * smoothCoef
                              + targetDepth * (1.0f - smoothCoef);

        // Profondeur effective — plancher passif 28% + boost
        // Le plancher évite le saut brutal quand le traitement dynamique s'active
        float roomDepthBoost = *apvts.getRawParameterValue("roomDepthBoost");
        constexpr float kPassiveFloor = 0.28f;
        float effectiveDepth = knobRoom * juce::jmax(kPassiveFloor, dynNotchSmoothedDepth);
        float effectiveQ     = (float)currentRoomQ;

        // Recalcul des coefficients si freq ou depth ont bougé
        float freqDelta  = std::abs(trackedFreq - lastRoomFreq);
        float depthDelta = std::abs(effectiveDepth - lastRoomValue);
        if (freqDelta > 1.0f || depthDelta > 0.003f || dynNotchCoeffsStale)
        {
            double fc    = (double)trackedFreq;
            double Q     = (double)effectiveQ;
            // Profondeur : -24 dB × effectiveDepth × boost, plafonné à -30 dB
            double maxDB = -24.0 * (double)effectiveDepth * (double)roomDepthBoost;
            maxDB = juce::jlimit(-30.0, 0.0, maxDB);
            double A = std::pow(10.0, maxDB / 40.0);
            double w0 = juce::MathConstants<double>::twoPi * fc / currentSampleRate;
            double cosW = std::cos(w0), sinW = std::sin(w0);
            double alpha = sinW / (2.0 * Q);
            double b0=1.0+alpha*A, b1=-2.0*cosW, b2=1.0-alpha*A;
            double a0=1.0+alpha/A, a2=1.0-alpha/A;
            notchB0=b0/a0; notchB1=b1/a0; notchB2=b2/a0;
            notchA1=b1/a0; notchA2=a2/a0;   // a1 = b1 normalisé

            // Passe-bande delta (même fc/Q)
            double alphaBP = sinW / (2.0 * Q);
            double bpDen = 1.0 + alphaBP;
            notchBPB0= alphaBP / bpDen; notchBPB1=0.0; notchBPB2=-alphaBP / bpDen;
            notchBPA1=-2.0*cosW / bpDen; notchBPA2=(1.0-alphaBP)/bpDen;

            // Shelf de compensation (HPF doux à 3 kHz)
            double shGainDB = -maxDB * 0.15;
            double Ash = std::pow(10.0, shGainDB / 40.0);
            double w0s = juce::MathConstants<double>::twoPi * 3000.0 / currentSampleRate;
            double cosS=std::cos(w0s), sinS=std::sin(w0s);
            double alphaS = sinS/2.0*std::sqrt((Ash+1.0/Ash)*(1.0/0.707-1.0)+2.0);
            double sb0=Ash*((Ash+1)+(Ash-1)*cosS+2*std::sqrt(Ash)*alphaS);
            double sb1=-2*Ash*((Ash-1)+(Ash+1)*cosS);
            double sb2=Ash*((Ash+1)+(Ash-1)*cosS-2*std::sqrt(Ash)*alphaS);
            double sa0=(Ash+1)-(Ash-1)*cosS+2*std::sqrt(Ash)*alphaS;
            double sa1=2*((Ash-1)-(Ash+1)*cosS);
            double sa2=(Ash+1)-(Ash-1)*cosS-2*std::sqrt(Ash)*alphaS;
            shelfB0=sb0/sa0; shelfB1=sb1/sa0; shelfB2=sb2/sa0;
            shelfA1=sa1/sa0; shelfA2=sa2/sa0;

            lastRoomFreq  = trackedFreq;
            lastRoomValue = effectiveDepth;
            dynNotchCoeffsStale = false;
        }
    }

    // --- CONFIGURATIONS PRÉ-BLOC ---
    float gateThresholdLin = juce::Decibels::decibelsToGain(advGateThresholdDB);
    float fastGateAttackCoef = 1.0f - std::exp(-1.0f / (advGateAttack * (float)currentSampleRate));
    float fastGateReleaseCoef = 1.0f - std::exp(-1.0f / (advCleanRelease * (float)currentSampleRate));
    int holdSamples = (int)(advGateHold * (float)currentSampleRate);

    float deEssRatio = 1.0f + (knobDeEss * 15.0f * advDeEssInten);
    float safeDeEssAtk = juce::jlimit(0.001f, 0.010f, advDeEssAtk);
    float deEssAttackCoef = 1.0f - std::exp(-1.0f / (safeDeEssAtk * (float)currentSampleRate));

    // === DE-REV 3 BANDES — coefficients LP Butterworth (coupures 500 / 4000 Hz) ===
    static constexpr float REV_XOVER[2] = { 500.0f, 4000.0f };
    double revSplitB0[2], revSplitB1[2], revSplitB2[2], revSplitA1c[2], revSplitA2c[2];
    for (int s = 0; s < 2; ++s) {
        double w0 = juce::MathConstants<double>::twoPi * REV_XOVER[s] / currentSampleRate;
        double c0 = std::cos(w0), s0 = std::sin(w0), alp = s0 / (2.0 * 0.7071);
        double b0 = (1.0-c0)*0.5, b1 = 1.0-c0, b2 = (1.0-c0)*0.5, a0 = 1.0+alp;
        revSplitB0[s]=b0/a0; revSplitB1[s]=b1/a0; revSplitB2[s]=b2/a0;
        revSplitA1c[s]=-2.0*c0/a0; revSplitA2c[s]=(1.0-alp)/a0;
    }
    // Sidechain De-Rev : HP 200 Hz + LP 3000 Hz (zone voix, évite pompage sur résidus)
    double rvScHPB0,rvScHPB1,rvScHPB2,rvScHPA1,rvScHPA2;
    double rvScLPB0,rvScLPB1,rvScLPB2,rvScLPA1,rvScLPA2;
    {
        double wh = juce::MathConstants<double>::twoPi * 200.0 / currentSampleRate;
        double ch2 = std::cos(wh), sh2 = std::sin(wh), alh = sh2 / (2.0*0.7071), ah0 = 1.0+alh;
        rvScHPB0=(1+ch2)*0.5/ah0; rvScHPB1=-(1+ch2)/ah0; rvScHPB2=(1+ch2)*0.5/ah0;
        rvScHPA1=-2*ch2/ah0; rvScHPA2=(1-alh)/ah0;
        double wl = juce::MathConstants<double>::twoPi * 3000.0 / currentSampleRate;
        double cl2 = std::cos(wl), sl2 = std::sin(wl), all2 = sl2 / (2.0*0.7071), al0 = 1.0+all2;
        rvScLPB0=(1-cl2)*0.5/al0; rvScLPB1=(1-cl2)/al0; rvScLPB2=(1-cl2)*0.5/al0;
        rvScLPA1=-2*cl2/al0; rvScLPA2=(1-all2)/al0;
    }
    // Constantes de temps par bande : aigus décroissent plus vite que graves (physique réverb)
    float rvSr = (float)currentSampleRate;
    // [0]=<500Hz  [1]=500-4000Hz  [2]=>4000Hz
    float rvFastA[3]={ 1.f-std::exp(-1.f/(0.003f*rvSr)), 1.f-std::exp(-1.f/(0.001f*rvSr)), 1.f-std::exp(-1.f/(0.0005f*rvSr)) };
    float rvFastR[3]={ 1.f-std::exp(-1.f/(0.080f*rvSr)), 1.f-std::exp(-1.f/(0.035f*rvSr)), 1.f-std::exp(-1.f/(0.015f*rvSr)) };
    float rvSlowA[3]={ 1.f-std::exp(-1.f/(0.015f*rvSr)), 1.f-std::exp(-1.f/(0.005f*rvSr)), 1.f-std::exp(-1.f/(0.002f*rvSr)) };
    float rvSlowR[3]={ 1.f-std::exp(-1.f/(1.000f*rvSr)), 1.f-std::exp(-1.f/(0.600f*rvSr)), 1.f-std::exp(-1.f/(0.250f*rvSr)) };

    // De-Ess : constante du seuil adaptatif (slow follower ~2s)
    float deEssAdaptCoef = 1.0f - std::exp(-1.0f / (2.0f * (float)currentSampleRate));

    float targetOutputGain = juce::Decibels::decibelsToGain(knobOutputGain);
    float smoothCoefOut = 1.0f - std::exp(-1.0f / (0.01f * (float)currentSampleRate));

    // NR amount = facade knob clean
    float nrAmount = knobClean;
    float nrThreshDB = *apvts.getRawParameterValue("nrThreshold");
    float nrRatioVal = *apvts.getRawParameterValue("nrRatio");
    float nrFloorDB = *apvts.getRawParameterValue("nrFloor");
    float nrSmoothVal = *apvts.getRawParameterValue("nrSmooth");
    bool nrActive = (nrAmount > 0.001f && !cleanBypass && !nrBypass);

    // === NR CEDAR 6 BANDES — coefficients LP Butterworth 2nd ordre (recalculés par bloc) ===
    // Coupures : 150 / 500 / 1500 / 4000 / 8000 Hz
    static constexpr float NR_XOVER[5] = { 150.0f, 500.0f, 1500.0f, 4000.0f, 8000.0f };
    double nrSplitB0[5], nrSplitB1[5], nrSplitB2[5], nrSplitA1[5], nrSplitA2[5];
    for (int s = 0; s < 5; ++s)
    {
        double w0  = juce::MathConstants<double>::twoPi * NR_XOVER[s] / currentSampleRate;
        double c0  = std::cos(w0), s0 = std::sin(w0);
        double alp = s0 / (2.0 * 0.7071);
        double b0  = (1.0 - c0) * 0.5, b1 = 1.0 - c0, b2 = (1.0 - c0) * 0.5;
        double a0  = 1.0 + alp,         a1 = -2.0 * c0, a2 = 1.0 - alp;
        nrSplitB0[s] = b0/a0; nrSplitB1[s] = b1/a0; nrSplitB2[s] = b2/a0;
        nrSplitA1[s] = a1/a0; nrSplitA2[s] = a2/a0;
    }
    // Constantes de temps NR (pré-calcul par bloc pour éviter exp() par sample)
    float nrSr = (float)currentSampleRate;
    float nrEnvAtk      = 1.0f - std::exp(-1.0f / (0.001f * nrSr));  // 1ms  attaque enveloppe
    float nrEnvRel      = 1.0f - std::exp(-1.0f / (0.060f * nrSr));  // 60ms release enveloppe
    // Constantes hybrides plancher
    float nrFlrGate     = 1.0f - std::exp(-1.0f / (0.20f  * nrSr));  // 0.2s gate-couplé (silence précis)
    float nrFlrRoll     = 1.0f - std::exp(-1.0f / (1.50f  * nrSr));  // 1.5s rolling min (voix continue)
    float nrFlrSnap     = 1.0f - std::exp(-1.0f / (10.0f  * nrSr));  // 10s  snapshot drift
    float nrSmoothT     = juce::jmax(0.002f, nrSmoothVal);
    float nrGainAtk     = 1.0f - std::exp(-1.0f / (nrSmoothT * 0.015f * nrSr));
    float nrGainRel     = 1.0f - std::exp(-1.0f / (nrSmoothT * 0.120f * nrSr));

    // Traitement snapshot request/release (une fois par bloc, avant la boucle sample)
    if (nrSnapshotRequest.exchange(false))
    {
        for (int b = 0; b < 6; ++b)
            for (int ch2 = 0; ch2 < buffer.getNumChannels(); ++ch2)
                nrSnapshotFloor[b][ch2] = juce::jmax(nrRollingMin[b][ch2], 1e-10f);
        nrSnapshotActive = true;
    }
    if (nrSnapshotRelease.exchange(false))
        nrSnapshotActive = false;

    // --- FILTRE SIDECHAIN GATE (Passe-bande 200–3000 Hz, corps vocal) ---
    // Étage 1 : HP 200 Hz, Q=0.707
    double gsHPw0    = juce::MathConstants<double>::twoPi * 200.0 / currentSampleRate;
    double gsHPcos   = std::cos(gsHPw0);
    double gsHPalpha = std::sin(gsHPw0) / (2.0 * 0.707);
    double gsHPA0    = 1.0 + gsHPalpha;
    double gsHPB0    = ((1.0 + gsHPcos) / 2.0) / gsHPA0;
    double gsHPB1    = (-(1.0 + gsHPcos))       / gsHPA0;
    double gsHPB2    = ((1.0 + gsHPcos) / 2.0)  / gsHPA0;
    double gsHPA1    = (-2.0 * gsHPcos)          / gsHPA0;
    double gsHPA2    = (1.0 - gsHPalpha)         / gsHPA0;
    // Étage 2 : LP 3000 Hz, Q=0.707
    double gsLPw0    = juce::MathConstants<double>::twoPi * 3000.0 / currentSampleRate;
    double gsLPcos   = std::cos(gsLPw0);
    double gsLPalpha = std::sin(gsLPw0) / (2.0 * 0.707);
    double gsLPA0    = 1.0 + gsLPalpha;
    double gsLPB0    = ((1.0 - gsLPcos) / 2.0) / gsLPA0;
    double gsLPB1    = (1.0 - gsLPcos)          / gsLPA0;
    double gsLPB2    = ((1.0 - gsLPcos) / 2.0)  / gsLPA0;
    double gsLPA1    = (-2.0 * gsLPcos)          / gsLPA0;
    double gsLPA2    = (1.0 - gsLPalpha)         / gsLPA0;

    // ── Détecteur De-Ess : Q dynamique depuis le slider WIDTH (Weiss style) ─────
    // advDeEssDetectRatio (slider 0→1) mappé en Q : 0→Q=1.0 (large), 1→Q=10.0 (chirurgical)
    double deDetectionQ = juce::jlimit(1.0, 10.0, 1.0 + (double)advDeEssDetectRatio * 9.0);

    double deDetB0{ 1.0 }, deDetB1{ 0.0 }, deDetB2{ 0.0 }, deDetA1{ 0.0 }, deDetA2{ 0.0 };
    // Bande de référence large (Q=0.7) — capture le caractère vocal à cette fréquence
    double deRefB0{ 1.0 }, deRefB1{ 0.0 }, deRefB2{ 0.0 }, deRefA1{ 0.0 }, deRefA2{ 0.0 };

    if (knobDeEss > 0.001f && !deessBypass) {
        double sideFreq = juce::jlimit(2000.0, 9000.0, (double)advDeEssFreq);
        double w0d = juce::MathConstants<double>::twoPi * sideFreq / currentSampleRate;
        double cd = std::cos(w0d), sd = std::sin(w0d);

        // Bande étroite : Q dynamique (WIDTH slider)
        double ad = sd / (2.0 * deDetectionQ);
        double a0d = 1.0 + ad;
        deDetB0 = ad / a0d; deDetB1 = 0.0; deDetB2 = -ad / a0d;
        deDetA1 = -2.0 * cd / a0d; deDetA2 = (1.0 - ad) / a0d;

        // Bande de référence large (Q=0.7) — même centre, capture la voix à cette zone
        double ar = sd / (2.0 * 0.7);
        double a0r = 1.0 + ar;
        deRefB0 = ar / a0r; deRefB1 = 0.0; deRefB2 = -ar / a0r;
        deRefA1 = -2.0 * cd / a0r; deRefA2 = (1.0 - ar) / a0r;
    }
    // ── Précompute De-Ess : trig constants (1× par bloc, pas 44100×) ─────────
    double deEssSafeFreq = juce::jlimit(4500.0f, 9500.0f, advDeEssFreq);
    double deEssW0c = juce::MathConstants<double>::twoPi * deEssSafeFreq / currentSampleRate;
    double deEssCosW = std::cos(deEssW0c);
    double deEssSinW = std::sin(deEssW0c);
    double deEssAlphBell = deEssSinW / (2.0 * 3.5);  // bell Q=3.5
    double deEssAlphShelfBase = deEssSinW / (2.0 * 0.707);  // shelf base
    // cb1 = ca1 = -2*cosW est constant par bloc en mode bell
    double deEssCb1Ca1 = -2.0 * deEssCosW;

    // ── Précompute AIR Heat HP : trig constants (1× par bloc) ────────────────
    double airW0h   = juce::MathConstants<double>::twoPi * (double)advAirFreq / currentSampleRate;
    double airCosH  = std::cos(airW0h);
    double airSinH  = std::sin(airW0h);
    double airAlpH  = airSinH / (2.0 * 0.707);
    double airHpa0  = 1.0 + airAlpH;
    double airHpb0n = (1.0 + airCosH) * 0.5  / airHpa0;
    double airHpb1n = -(1.0 + airCosH)        / airHpa0;
    double airHpb2n = (1.0 + airCosH) * 0.5  / airHpa0;
    double airHpa1n = -2.0 * airCosH          / airHpa0;
    double airHpa2n = (1.0 - airAlpH)         / airHpa0;

    // ── Précompute AIR Shelf : tous les coefficients (1× par bloc) ───────────
    // Aa et les coefficients shelf sont constants car knobAir et advAirFreq ne
    // changent pas au sein d'un bloc — les recalculer par sample était inutile.
    double airShelfb0n{1},airShelfb1n{0},airShelfb2n{0};
    double airShelfa1n{0},airShelfa2n{0};
    if (knobAir > 0.001f && !airBypass)
    {
        double boost = juce::jlimit(0.0, 9.5, 9.5 * (double)knobAir);
        double Aa    = std::pow(10.0, boost / 40.0);
        double cosA  = std::cos(airW0h); // même fréquence que le HP
        double sinA  = airSinH;
        double sqrtAa = std::sqrt(Aa);
        double alpA = sinA / (2.0 * 0.707) * std::sqrt((Aa + 1.0/Aa) * (1.0/0.707 - 1.0) + 2.0);
        double ab0 = Aa * ((Aa+1.0) + (Aa-1.0)*cosA + 2.0*sqrtAa*alpA);
        double ab1 = -2.0 * Aa * ((Aa-1.0) + (Aa+1.0)*cosA);
        double ab2 = Aa * ((Aa+1.0) + (Aa-1.0)*cosA - 2.0*sqrtAa*alpA);
        double aa0 = (Aa+1.0) - (Aa-1.0)*cosA + 2.0*sqrtAa*alpA;
        double aa1 = 2.0 * ((Aa-1.0) - (Aa+1.0)*cosA);
        double aa2 = (Aa+1.0) - (Aa-1.0)*cosA - 2.0*sqrtAa*alpA;
        airShelfb0n = ab0/aa0; airShelfb1n = ab1/aa0; airShelfb2n = ab2/aa0;
        airShelfa1n = aa1/aa0; airShelfa2n = aa2/aa0;
    }

    // --- BOUCLE ÉCHANTILLONS ---
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            double x = (double)data[sample];

            // Capture FFT entrée brute (canal gauche, avant tout traitement)
            if (channel == 0) {
                inputFftBuffer[inputFftWriteIndex] = data[sample];
                inputFftWriteIndex = (inputFftWriteIndex + 1) % fftSize;
                if (inputFftWriteIndex == 0) {
                    juce::ScopedLock lock(fftMutex);
                    float temp[fftSize * 2]{ 0.0f };
                    for (int i = 0; i < fftSize; ++i) temp[i * 2] = inputFftBuffer[i];
                    applyHannWindow(temp, fftSize);
                    fftRadix2(temp, fftSize);
                    for (int i = 0; i < fftSize / 2; ++i) {
                        float re = temp[i * 2], im = temp[i * 2 + 1];
                        inputFftMagnitudes[i] = std::sqrt(re * re + im * im);
                    }
                }
            }

            // Capture pré-CLEAN (pour cleanDelta)
            double preCleanX = x;

            // === CLEAN : GATE + NR 3-BANDES ===
            if (!cleanBypass)
            {
                if (knobClean > 0.001f && !gateBypass)
                {
                    // NOTE : currentEnv est RMS filtré 200–3000 Hz.
                    // Les valeurs RMS sont ~3–10 dB sous les valeurs peak équivalentes.
                    // Si le gate semble trop fermé, abaisser cleanGateThreshold de 3–6 dB.
                    // Étage 1 : HP 200 Hz
                    double gsSC_hp = gsHPB0 * x + gateSidechainZ1[channel];
                    gateSidechainZ1[channel]  = gsHPB1 * x       - gsHPA1 * gsSC_hp + gateSidechainZ2[channel];
                    gateSidechainZ2[channel]  = gsHPB2 * x       - gsHPA2 * gsSC_hp;
                    // Étage 2 : LP 3000 Hz (cascade sur sortie HP)
                    double gsSC_bp = gsLPB0 * gsSC_hp + gateSidechainZ1b[channel];
                    gateSidechainZ1b[channel] = gsLPB1 * gsSC_hp - gsLPA1 * gsSC_bp + gateSidechainZ2b[channel];
                    gateSidechainZ2b[channel] = gsLPB2 * gsSC_hp - gsLPA2 * gsSC_bp;
                    // RMS sur fenêtre glissante
                    float scSample = (float)gsSC_bp;
                    float newSq    = scSample * scSample;
                    float oldSq    = rmsBuffer[channel][rmsWritePos[channel]];
                    rmsSumSq[channel] += newSq - oldSq;
                    rmsBuffer[channel][rmsWritePos[channel]] = newSq;
                    rmsWritePos[channel] = (rmsWritePos[channel] + 1) % RMS_WINDOW_SIZE;
                    float currentEnv = std::sqrt(juce::jmax(0.0f, rmsSumSq[channel] / (float)RMS_WINDOW_SIZE));

                    // --- Histogramme gate auto-threshold (canal 0 uniquement) ---
                    if (channel == 0)
                    {
                        gateCurrentLevelDB = 20.0f * std::log10(currentEnv + 1e-8f);
                        gateHistCounter++;
                        if (gateHistCounter >= 256)
                        {
                            gateHistCounter = 0;
                            float histDecay = std::exp(-256.0f / (5.0f * (float)currentSampleRate));
                            for (auto& h : gateHistogram) h *= histDecay;
                            int bucket = juce::jlimit(0, 79, (int)(gateCurrentLevelDB + 80.0f));
                            gateHistogram[bucket] += 1.0f;
                        }
                    }

                    float noiseFloorEst = nrNoiseFloor[3][channel]; // bande 1500-4000 Hz (présence voix)
                    float hysteresisDB = juce::jlimit(1.5f, 5.0f, 2.0f + noiseFloorEst * 8.0f);
                    float hysteresisFactor = std::pow(10.0f, hysteresisDB / 20.0f);
                    float gateOpenThreshold = gateThresholdLin * hysteresisFactor;

                    if (gateIsOpen[channel]) {
                        if (currentEnv < gateThresholdLin) {
                            if (gateHoldCounter[channel] > 0) --gateHoldCounter[channel];
                            else gateIsOpen[channel] = false;
                        } else {
                            gateHoldCounter[channel] = holdSamples;
                        }
                    } else {
                        if (currentEnv > gateOpenThreshold) {
                            gateIsOpen[channel] = true;
                            gateHoldCounter[channel] = holdSamples;
                        }
                    }

                    // Plosive protection : LP 250 Hz + dual envelope → force gate open
                    if (plosiveProtect > 0.001f)
                    {
                        double lpOut = plosiveLPB0 * x + plosiveLPZ1[channel];
                        plosiveLPZ1[channel] = plosiveLPB1 * x - plosiveLPA1 * lpOut + plosiveLPZ2[channel];
                        plosiveLPZ2[channel] = plosiveLPB2 * x - plosiveLPA2 * lpOut;
                        float lpAbs = std::abs((float)lpOut);
                        float fpc = (lpAbs > plosiveFastEnv[channel]) ? plosiveFastAtkCoef : plosiveFastRelCoef;
                        plosiveFastEnv[channel] = fpc * plosiveFastEnv[channel] + (1.0f - fpc) * lpAbs;
                        float spc = (lpAbs > plosiveSlowEnv[channel]) ? plosiveSlowAtkCoef : plosiveSlowRelCoef;
                        plosiveSlowEnv[channel] = spc * plosiveSlowEnv[channel] + (1.0f - spc) * lpAbs;
                        float pRatio = plosiveFastEnv[channel] / (plosiveSlowEnv[channel] + 1e-6f);
                        float sensitivity = 1.0f + plosiveProtect * 4.0f;
                        if (pRatio > sensitivity && plosiveFastEnv[channel] > 0.0005f)
                        {
                            gateIsOpen[channel] = true;
                            gateHoldCounter[channel] = juce::jmax(gateHoldCounter[channel], holdSamples / 4);
                        }
                    }

                    if (channel == 0) gateIsOpenForDisplay.store(gateIsOpen[0]);

                    // Stereo linking : canal 1+ suit l'état de détection du canal 0
                    if (channel > 0 && totalNumInputChannels > 1)
                    {
                        gateIsOpen[channel]      = gateIsOpen[0];
                        gateHoldCounter[channel] = gateHoldCounter[0];
                    }

                    float targetGateGain = 1.0f;
                    if (!gateIsOpen[channel]) {
                        float floor = juce::Decibels::decibelsToGain(advGateFloorDB);
                        float ratio = currentEnv / gateThresholdLin;
                        targetGateGain = floor + (ratio * ratio) * (1.0f - floor);
                        targetGateGain = juce::jlimit(floor, 1.0f, targetGateGain);
                    }

                    float cCoef = (targetGateGain > smoothedGateGain[channel]) ? fastGateAttackCoef : fastGateReleaseCoef;
                    smoothedGateGain[channel] += cCoef * (targetGateGain - smoothedGateGain[channel]);

                    // Lookahead gate : alimenter le buffer toujours; lire en HQ seulement
                    {
                        int wp = gateLookaheadWrite[channel];
                        gateLookahead[channel][wp] = (float)x;
                        gateLookaheadWrite[channel] = (wp + 1) % GATE_LOOKAHEAD_MAX;
                        if (!liveModeActive)
                        {
                            int rdPos = (gateLookaheadWrite[channel] - gateLookaheadLen - 1 + GATE_LOOKAHEAD_MAX) % GATE_LOOKAHEAD_MAX;
                            x = (double)gateLookahead[channel][rdPos];
                        }
                    }

                    // Capture pré-CLEAN APRÈS le delay lookahead (correction delta = clean residuel uniquement)
                    preCleanX = x;

                    x *= (double)smoothedGateGain[channel];
                }

                if (nrActive)
                {
                    // === ROLLING WINDOW MINIMUM — mise à jour demi-fenêtre ===
                    nrHistCounter[channel]++;
                    if (nrHistCounter[channel] >= nrWindowPeriod)
                    {
                        nrHistCounter[channel] = 0;
                        for (int b = 0; b < 6; ++b)
                        {
                            nrPrevWindowMin[b][channel] = nrCurrWindowMin[b][channel];
                            nrCurrWindowMin[b][channel] = 1e10f;  // reset fenêtre courante
                        }
                    }

                    // === BANC DE FILTRES CEDAR 6 BANDES ===
                    double rem = x;
                    double bands[6];
                    for (int s = 0; s < 5; ++s)
                    {
                        double lp = nrSplitB0[s] * rem + nrSplitZ1[s][channel];
                        nrSplitZ1[s][channel] = nrSplitB1[s] * rem - nrSplitA1[s] * lp + nrSplitZ2[s][channel];
                        nrSplitZ2[s][channel] = nrSplitB2[s] * rem - nrSplitA2[s] * lp;
                        bands[s] = lp;
                        rem     -= lp;  // complément HP : contenu au-dessus de fc[s]
                    }
                    bands[5] = rem;  // bande haute (> 8000 Hz)

                    for (int b = 0; b < 6; ++b)
                    {
                        float absBand = std::abs((float)bands[b]);

                        // --- Enveloppe rapide par bande ---
                        float envCoef = (absBand > nrBandEnv[b][channel]) ? nrEnvAtk : nrEnvRel;
                        nrBandEnv[b][channel] += envCoef * (absBand - nrBandEnv[b][channel]);
                        float envLin = nrBandEnv[b][channel];

                        // --- Rolling window minimum (mise à jour par bande) ---
                        nrCurrWindowMin[b][channel] = juce::jmin(nrCurrWindowMin[b][channel], envLin);
                        nrRollingMin[b][channel]    = juce::jmin(nrCurrWindowMin[b][channel],
                                                                  nrPrevWindowMin[b][channel]);

                        // --- Estimation hybride du plancher ---
                        float& floorRef = nrNoiseFloor[b][channel];
                        if (nrSnapshotActive)
                        {
                            // Snapshot actif : plancher figé sur la capture, dérive douce (10s)
                            floorRef += nrFlrSnap * (nrSnapshotFloor[b][channel] - floorRef);
                        }
                        else if (!gateIsOpen[channel])
                        {
                            // Gate fermé (silence) : recalage précis et rapide vers l'énergie actuelle
                            floorRef += nrFlrGate * (envLin - floorRef);
                        }
                        else
                        {
                            // Gate ouvert (voix) : dérive vers le minimum glissant
                            if (floorRef < 1e-12f) floorRef = nrRollingMin[b][channel] + 1e-10f;
                            floorRef += nrFlrRoll * (nrRollingMin[b][channel] - floorRef);
                        }
                        floorRef = juce::jmax(floorRef, 1e-12f);
                        float snrLin = envLin / floorRef;

                        // --- Calcul du gain par courbe SNR ---
                        // nrThreshDB (-80→-10) mappe vers la fenêtre SNR de détection (3→20 dB)
                        float snrHeadroom = 3.0f + (nrThreshDB + 80.0f) / 70.0f * 17.0f;
                        float snrDB       = 20.0f * std::log10(snrLin + 1e-7f);
                        float floorGain   = juce::Decibels::decibelsToGain(nrFloorDB);
                        float targetGain  = 1.0f;
                        if (snrDB < snrHeadroom)
                        {
                            float t = juce::jmax(0.0f, snrDB / snrHeadroom);
                            t = t * t * (3.0f - 2.0f * t);  // smoothstep
                            float curve = std::pow(t, juce::jmax(1.0f, nrRatioVal * 0.5f));
                            targetGain  = floorGain + curve * (1.0f - floorGain);
                        }

                        // --- Lissage gain : adaptatif en attaque ET en release ---
                        float prevEnv    = nrBandPrevEnv[b][channel];
                        float slope      = (envLin - prevEnv) / (prevEnv + 1e-8f);
                        float adaptBoost = juce::jlimit(0.0f, 0.85f, slope * 5.0f);
                        // Transitoire d'ouverture (plosive, attaque vocale) :
                        // si l'énergie monte vite et que le gain veut s'ouvrir, accélérer fortement
                        float transientOpen = juce::jlimit(0.0f, 1.0f, slope * 3.0f);
                        float fastAtkCoef   = 1.0f - std::exp(-1.0f / (0.002f * nrSr)); // 2ms
                        float gainAtk    = nrGainAtk + transientOpen * (fastAtkCoef - nrGainAtk);
                        float gainRel    = nrGainRel * (1.0f + adaptBoost * 4.0f);

                        float smoothCoef = (targetGain < nrBandGain[b][channel]) ? gainAtk : gainRel;
                        nrBandGain[b][channel]    += smoothCoef * (targetGain - nrBandGain[b][channel]);
                        nrBandPrevEnv[b][channel]  = envLin;

                        float fullG = 1.0f + (nrBandGain[b][channel] - 1.0f) * nrAmount;
                        float softG = 1.0f + (nrBandGain[b][channel] - 1.0f) * nrAmount * 0.25f;
                        float g = fullG + nrArtifactBlend * (softG - fullG);
                        bands[b] *= (double)g;
                    }
                    x = 0.0;
                    for (int b = 0; b < 6; ++b) x += bands[b];
                }
            }

            // === CLEAN DELTA : sortie = différence CLEAN uniquement ===
            if (cleanDelta) {
                x = preCleanX - x;
                data[sample] = (float)(x * (double)smoothedOutputGain[channel]);
                continue;
            }

            // === ROOM : NOTCH + COMPENSATION ===
            bool roomShouldProcess = (knobRoom > 0.001f && !roomBypass) || roomDelta;
            if (roomShouldProcess && !cleanDelta) {
                double preNotchX = x;
                double notchOut = notchB0 * x + notchZ1[channel];
                notchZ1[channel] = notchB1 * x - notchA1 * notchOut + notchZ2[channel];
                notchZ2[channel] = notchB2 * x - notchA2 * notchOut;
                x = notchOut;
                // ROOM DELTA : passe-bande résonnant (entendre la bande notchée)
                if (roomDelta) {
                    double bpOut = notchBPB0 * x + notchBPZ1[channel];
                    notchBPZ1[channel] = notchBPB1 * x - notchBPA1 * bpOut + notchBPZ2[channel];
                    notchBPZ2[channel] = notchBPB2 * x - notchBPA2 * bpOut;
                    x = bpOut;
                    data[sample] = (float)(x * (double)smoothedOutputGain[channel]);
                    continue;
                }
                double shelfOut = shelfB0 * x + shelfZ1[channel];
                shelfZ1[channel] = shelfB1 * x - shelfA1 * shelfOut + shelfZ2[channel];
                shelfZ2[channel] = shelfB2 * x - shelfA2 * shelfOut;
                x = shelfOut;
            }

            // FFT buffer après ROOM (visualiseur, canal gauche uniquement)
            if (channel == 0) {
                fftBuffer[fftWriteIndex] = (float)x;
                fftWriteIndex = (fftWriteIndex + 1) % fftSize;
                if (fftWriteIndex == 0) {
                    juce::ScopedLock lock(fftMutex);
                    float temp[fftSize * 2]{ 0.0f };
                    for (int i = 0; i < fftSize; ++i) temp[i * 2] = fftBuffer[i];
                    applyHannWindow(temp, fftSize);
                    fftRadix2(temp, fftSize);
                    for (int i = 0; i < fftSize / 2; ++i) {
                        float mag = std::sqrt(temp[i * 2] * temp[i * 2] + temp[i * 2 + 1] * temp[i * 2 + 1]);
                        float alpha = 0.3f;
                        fftMagnitudes[i] = alpha * mag + (1.0f - alpha) * fftMagnitudes[i];
                    }
                }
            }

            // === REVERB DELTA : capture avant DE-REV ===
            double preReverbX = x;

            // === DE-REV 3 BANDES (Cedar/Eventide style) ===
            if (knobReverb > 0.001f && !reverbBypass && !cleanDelta)
            {
                // Sidechain 200-3000 Hz : détection de présence vocale
                // Quand la voix est présente, l'aggressivité de de-rev est réduite (anti-pompage)
                double scHP = rvScHPB0 * x + revScHPZ1[channel];
                revScHPZ1[channel] = rvScHPB1 * x - rvScHPA1 * scHP + revScHPZ2[channel];
                revScHPZ2[channel] = rvScHPB2 * x - rvScHPA2 * scHP;
                double scBP = rvScLPB0 * scHP + revScLPZ1[channel];
                revScLPZ1[channel] = rvScLPB1 * scHP - rvScLPA1 * scBP + revScLPZ2[channel];
                revScLPZ2[channel] = rvScLPB2 * scHP - rvScLPA2 * scBP;
                float scLevel = std::abs((float)scBP);

                // Transient preservation : détection onset vocal → freeze gain 20ms
                float tCoef = (scLevel > revTransientEnv[channel]) ? revTransAtkCoef : revTransRelCoef;
                revTransientEnv[channel] = tCoef * revTransientEnv[channel] + (1.f - tCoef) * scLevel;
                bool isOnset = (scLevel > revTransientEnv[channel] * 1.8f) && (scLevel > 0.0005f);
                if (isOnset) revTransientHold[channel] = revTransHoldLen;
                else if (revTransientHold[channel] > 0) --revTransientHold[channel];
                bool freezeDeRev = (revTransientHold[channel] > 0);

                // Banc 3 bandes (LP complémentaires séquentiels)
                double rem = x;
                double rvBands[3];
                for (int s = 0; s < 2; ++s) {
                    double lp = revSplitB0[s] * rem + revSplitZ1[s][channel];
                    revSplitZ1[s][channel] = revSplitB1[s] * rem - revSplitA1c[s] * lp + revSplitZ2[s][channel];
                    revSplitZ2[s][channel] = revSplitB2[s] * rem - revSplitA2c[s] * lp;
                    rvBands[s] = lp; rem -= lp;
                }
                rvBands[2] = rem;

                x = 0.0;
                for (int b = 0; b < 3; ++b)
                {
                    float ab = std::abs((float)rvBands[b]);
                    revBandFast[b][channel] += ((ab > revBandFast[b][channel]) ? rvFastA[b] : rvFastR[b])
                                               * (ab - revBandFast[b][channel]);
                    revBandSlow[b][channel] += ((ab > revBandSlow[b][channel]) ? rvSlowA[b] : rvSlowR[b])
                                               * (ab - revBandSlow[b][channel]);
                    float gain = 1.0f;
                    if (revBandSlow[b][channel] > 1e-5f && !freezeDeRev)
                    {
                        float transRatio = juce::jlimit(0.0f, 1.5f,
                            revBandFast[b][channel] / (revBandSlow[b][channel] + 1e-6f));
                        float T = juce::jmax(0.01f, deRevThresh[b]);
                        float tailDepth = (transRatio < T)
                            ? juce::jmax(0.0f, 1.0f - transRatio / T)
                            : 0.0f;
                        float voiceGuard = juce::jmin(0.7f,
                            scLevel / (revBandSlow[b][channel] + 1e-6f) * 0.5f);
                        gain = 1.0f - deRevBandIntensity[b] * knobReverb * 0.65f * tailDepth * (1.0f - voiceGuard);
                        gain = juce::jlimit(deRevFloor, 1.0f, gain);
                    }
                    if (channel == 0)
                    {
                        revBandLiveEnergy[b].store(revBandFast[b][0]);
                        revBandLiveGR[b].store(gain);
                    }
                    x += rvBands[b] * (double)gain;
                }
            }

            // === REVERB DELTA : sortie = différence DE-REV uniquement ===
            if (reverbDelta) {
                x = preReverbX - x;
                data[sample] = (float)(x * (double)smoothedOutputGain[channel]);
                continue;
            }

            // Le buffer look-ahead est alimenté en continu pour éviter les pops à l'activation
            {
                int wp = deEssLookaheadWrite[channel];
                deEssLookahead[channel][wp] = (float)x;
                deEssLookaheadWrite[channel] = (wp + 1) % DEESS_LOOKAHEAD_MAX;
            }

            // Pré-capture pour delta mode (sera mise à jour vers xDelayed si de-ess actif)
            double preDeEssX = x;

            // === DE-ESS avec look-ahead 2ms + seuil adaptatif (Orban 764A style) ===
            if (knobDeEss > 0.001f && !deessBypass && !cleanDelta)
            {
                // LIVE : pas de délai — HQ : lecture retardée 2ms (look-ahead)
                double xTarget;
                if (liveModeActive)
                {
                    xTarget = x;
                }
                else
                {
                    int rdPos = (deEssLookaheadWrite[channel] - deEssLookaheadLen - 1 + DEESS_LOOKAHEAD_MAX) % DEESS_LOOKAHEAD_MAX;
                    xTarget = (double)deEssLookahead[channel][rdPos];
                }
                preDeEssX = xTarget;
                // ── Détection Weiss-style : ratio spectral étroit/large ────────────
                // Sidechain sur x COURANT (non retardé) → look-ahead effectif

                // Bande étroite (sibilante) — Q dynamique depuis slider WIDTH
                double hpDet = deDetB0 * x + deEssHpZ1[channel];
                deEssHpZ1[channel] = deDetB1 * x - deDetA1 * hpDet + deEssHpZ2[channel];
                deEssHpZ2[channel] = deDetB2 * x - deDetA2 * hpDet;
                float deEssEnv = std::abs((float)hpDet);

                // Bande de référence large (Q=0.7, même centre) — capture voix/harmoniques
                double refDet = deRefB0 * x + deEssRefZ1[channel];
                deEssRefZ1[channel] = deRefB1 * x - deRefA1 * refDet + deEssRefZ2[channel];
                deEssRefZ2[channel] = deRefB2 * x - deRefA2 * refDet;
                float deEssRefEnv = std::abs((float)refDet);

                // Ratio spectral : si l'énergie est très concentrée dans la bande étroite
                // par rapport à la bande large → c'est une sibilante (pas un formant vocal)
                // Un formant vocal a une énergie répartie → ratio proche de 1
                // Une sibilante a une énergie très focalisée → ratio > 2.5
                float spectralRatio = deEssEnv / (deEssRefEnv + 1e-7f);

                // Seuil adaptatif (style Orban 764A) — monte si la voix est naturellement sibilante
                deEssAdaptEnv[channel] += deEssAdaptCoef * (deEssEnv - deEssAdaptEnv[channel]);
                float baseThreshLin = juce::Decibels::decibelsToGain(advDeEssThreshDB);
                float adaptRatio    = deEssAdaptEnv[channel] / (baseThreshLin + 1e-7f);
                float adaptOffsetDB = juce::jlimit(0.0f, 6.0f,
                    20.0f * std::log10(juce::jmax(1.0f, adaptRatio)) * 0.35f);
                float deEssThreshLin = juce::Decibels::decibelsToGain(advDeEssThreshDB + adaptOffsetDB);

                float targetCleanGain = 1.0f;
                // Déclenchement : niveau minimal ET ratio spectral > 2.5 (concentré = sibilante)
                // Le ratio 2.5 correspond à ~8 dB de différence entre bande étroite et large
                if (deEssEnv > 0.0002f && spectralRatio > 2.5f)
                {
                    if (deEssEnv > deEssThreshLin)
                        targetCleanGain = std::pow(deEssThreshLin / deEssEnv, deEssRatio);
                }
                float safeRelease = juce::jlimit(0.010f, 0.080f, advDeEssRel);
                float deEssSmoothCoef = (targetCleanGain < smoothedCleanGain[channel]) ? deEssAttackCoef
                    : (1.0f - std::exp(-1.0f / (safeRelease * (float)currentSampleRate)));
                smoothedCleanGain[channel] += deEssSmoothCoef * (targetCleanGain - smoothedCleanGain[channel]);
                float attenDB = juce::jlimit(-20.0f, 0.0f, -20.0f * (1.0f - smoothedCleanGain[channel]) * knobDeEss);
                double Acl = std::pow(10.0, attenDB / 40.0);
                // cos/sin précomputés une fois par bloc — économie de ~44100 appels trig/s
                double cb0, cb1, cb2, ca0, ca1, ca2;
                if (isBellMode) {
                    cb0 = 1.0 + deEssAlphBell * Acl; cb1 = deEssCb1Ca1; cb2 = 1.0 - deEssAlphBell * Acl;
                    ca0 = 1.0 + deEssAlphBell / Acl; ca1 = deEssCb1Ca1; ca2 = 1.0 - deEssAlphBell / Acl;
                } else {
                    double alph = deEssAlphShelfBase * std::sqrt((Acl + 1.0/Acl) * (1.0/0.707 - 1.0) + 2.0);
                    double sqrtAcl = std::sqrt(Acl);
                    cb0 = Acl * ((Acl+1.0) + (Acl-1.0)*deEssCosW + 2.0*sqrtAcl*alph);
                    cb1 = -2.0 * Acl * ((Acl-1.0) + (Acl+1.0)*deEssCosW);
                    cb2 = Acl * ((Acl+1.0) + (Acl-1.0)*deEssCosW - 2.0*sqrtAcl*alph);
                    ca0 = (Acl+1.0) - (Acl-1.0)*deEssCosW + 2.0*sqrtAcl*alph;
                    ca1 = 2.0 * ((Acl-1.0) - (Acl+1.0)*deEssCosW);
                    ca2 = (Acl+1.0) - (Acl-1.0)*deEssCosW - 2.0*sqrtAcl*alph;
                }
                // Le filtre est appliqué sur xDelayed (signal retardé de 2ms)
                // La réduction de gain est déjà calculée avant l'arrivée de la sibilance → look-ahead
                double aDeEssOut = (cb0 / ca0) * xTarget + cleanShelfZ1[channel];
                cleanShelfZ1[channel] = (cb1 / ca0) * xTarget - (ca1 / ca0) * aDeEssOut + cleanShelfZ2[channel];
                cleanShelfZ2[channel] = (cb2 / ca0) * xTarget - (ca2 / ca0) * aDeEssOut;
                x = aDeEssOut;
            }

            // === DE-ESS DELTA : sortie = différence DE-ESS uniquement ===
            if (deessDelta) {
                x = preDeEssX - x;
                data[sample] = (float)(x * (double)smoothedOutputGain[channel]);
                continue;
            }

            // === AIR DELTA : capture avant AIR ===
            double preAirX = x;

            // === AIR : ENHANCER + DRIVE ===
            if (!airBypass && !cleanDelta)
            {
                // === HEAT : excitation harmonique parallèle (style Aphex Aural Exciter) ===
                // Passe-haut à airFreq → saturation → injection des harmoniques uniquement
                // Le signal de base n'est pas modifié → Heat et Shelf sont totalement indépendants
                double heatSignal = 0.0;
                if (advAirDrive > 0.001f)
                {
                    float driveAmt = juce::jlimit(0.0f, 1.0f, advAirDrive);
                    double hpOut = airHpb0n * x + heatHPZ1[channel];
                    heatHPZ1[channel] = airHpb1n * x - airHpa1n * hpOut + heatHPZ2[channel];
                    heatHPZ2[channel] = airHpb2n * x - airHpa2n * hpOut;
                    float driveScale = 1.0f + driveAmt * 5.0f;
                    float satIn = (float)hpOut * driveScale;

                    int satMode = (int)*apvts.getRawParameterValue("airSatMode");
                    float satOut;
                    switch (satMode)
                    {
                        case 0: // TRIODE — asymétrique, harmoniques paires (2e dominant)
                        {
                            static const float bias = 0.18f;
                            static const float den  = std::tanh(1.0f + bias) - std::tanh(bias);
                            satOut = (std::tanh(satIn + bias) - std::tanh(bias)) / (den * driveScale);
                            break;
                        }
                        case 1: // CLASS-A — cubique symétrique, harmoniques impaires (3e dominant)
                        {
                            float xc = juce::jlimit(-1.5f, 1.5f, satIn);
                            satOut = (xc - (xc * xc * xc) / 3.0f) / driveScale;
                            break;
                        }
                        case 2: // TAPE — atan très doux, minimum d'harmoniques (plus transparent)
                        {
                            satOut = (2.0f / juce::MathConstants<float>::pi)
                                     * std::atan(satIn * 1.2f) / driveScale;
                            break;
                        }
                        case 3: // IRON — x/sqrt(1+x²) : genou plus dur que tanh (transformateur)
                        {
                            float x2 = satIn * satIn;
                            satOut = satIn / std::sqrt(1.0f + x2) / driveScale;
                            break;
                        }
                        default:
                            satOut = std::tanh(satIn) / driveScale;
                            break;
                    }

                    heatSignal = (double)((satOut - (float)hpOut) * driveAmt * 1.5f);
                }

                // === SHELF AIR : coefficients précomputés avant la boucle ===
                if (knobAir > 0.001f)
                {
                    double airOut = airShelfb0n * x + airShelfZ1[channel];
                    airShelfZ1[channel] = airShelfb1n * x - airShelfa1n * airOut + airShelfZ2[channel];
                    airShelfZ2[channel] = airShelfb2n * x - airShelfa2n * airOut;
                    x = airOut;
                }

                // Injection Heat après le shelf (harmoniques s'ajoutent au signal final)
                x += heatSignal;

                // === TRANSIENT SHAPER AIR (bipolar SHAPE knob) ===
                if (std::abs(advAirShape) > 0.001f)
                {
                    float xAbs = (float)std::abs(x);
                    // Enveloppe rapide (transitoires) — atk 1ms / rel 10ms
                    float fCoef = (xAbs > airFastEnv[channel]) ? airShapeFastAtkCoef : airShapeFastRelCoef;
                    airFastEnv[channel] = fCoef * airFastEnv[channel] + (1.0f - fCoef) * xAbs;
                    // Enveloppe lente (sustain) — atk 15ms / rel 80ms
                    float sCoef = (xAbs > airSlowEnv[channel]) ? airShapeSlowAtkCoef : airShapeSlowRelCoef;
                    airSlowEnv[channel] = sCoef * airSlowEnv[channel] + (1.0f - sCoef) * xAbs;
                    // Contenu transitoire normalisé [0..1]
                    float transient = (airFastEnv[channel] - airSlowEnv[channel])
                                    / (airSlowEnv[channel] + 1e-6f);
                    transient = juce::jlimit(0.0f, 1.0f, transient);
                    // + SHAPE → boost transitoires, - SHAPE → atténue transitoires
                    float gain = 1.0f + advAirShape * transient * 1.5f;
                    x *= (double)juce::jlimit(0.1f, 4.0f, gain);
                }
            }

            // === AIR DELTA : sortie = différence AIR uniquement ===
            if (airDelta) {
                x = preAirX - x;
                data[sample] = (float)(x * (double)smoothedOutputGain[channel]);
                continue;
            }

            smoothedOutputGain[channel] += smoothCoefOut * (targetOutputGain - smoothedOutputGain[channel]);
            data[sample] = (float)(x * (double)smoothedOutputGain[channel]);
        }
    }

    // Mesure du niveau de sortie (peak mono, après tout traitement)
    float outputPeak = 0.f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* d = buffer.getReadPointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
            outputPeak = std::max(outputPeak, std::abs(d[s]));
    }
    {
        float prev = outputLevelDisplay.load();
        float decay = std::exp(-1.f / (0.200f * (float)currentSampleRate / (float)buffer.getNumSamples()));
        outputLevelDisplay.store(std::max(outputPeak, prev * decay));
    }
}

bool InvisibleFixAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* InvisibleFixAudioProcessor::createEditor()
{
    return new InvisibleFixAudioProcessorEditor(*this);
}

// =============================================================================
//  PRESET MANAGER
// =============================================================================

// Chaque preset est une liste de {paramID, valeur réelle (même échelle que le paramètre)}.
// Les paramètres absents restent à leur valeur par défaut.
// ─── Tu peux modifier ces valeurs directement pour affiner chaque preset ──────

using ParamOverride = std::pair<juce::String, float>;
using ParamList     = std::vector<ParamOverride>;

static ParamList getFactoryPresetParams(int index)
{
    switch (index)
    {
    // ── 0 : Init ─────────────────────────────────────────────────────────────
    case 0:
        return {}; // tout par défaut

    // ── 1 : Podcast Voix Proche ───────────────────────────────────────────────
    // Correction équilibrée pour micro de table/headset en voix de près
    case 1:
        return {
            { "clean",              0.50f }, { "deess",            0.42f },
            { "room",               0.20f }, { "reverb",           0.18f },
            { "air",                0.18f },
            { "cleanGateThreshold",-32.0f }, { "cleanGateFloor",  -42.0f },
            { "cleanGateAttack",    0.004f}, { "cleanGateHold",    0.040f},
            { "cleanRelease",       0.18f },
            { "nrThreshold",       -52.0f }, { "nrRatio",           2.5f },
            { "nrFloor",           -14.0f }, { "nrSmooth",          0.025f},
            { "deEssFreq",       7200.0f  }, { "deEssThreshold",  -36.0f },
            { "deEssIntensity",     1.0f  }, { "deEssDetectRatio",  0.15f },
            { "airFreq",        11000.0f  },
        };

    // ── 2 : Live Streaming ────────────────────────────────────────────────────
    // Gate serré et réactif, NR fort, de-ess agressif pour conditions live
    case 2:
        return {
            { "clean",              0.68f }, { "deess",            0.58f },
            { "room",               0.10f }, { "reverb",           0.32f },
            { "air",                0.28f },
            { "cleanGateThreshold",-26.0f }, { "cleanGateFloor",  -46.0f },
            { "cleanGateAttack",    0.002f}, { "cleanGateHold",    0.020f},
            { "cleanRelease",       0.12f },
            { "nrThreshold",       -45.0f }, { "nrRatio",           4.0f },
            { "nrFloor",           -16.0f }, { "nrSmooth",          0.015f},
            { "deEssFreq",       7000.0f  }, { "deEssThreshold",  -32.0f },
            { "deEssIntensity",     1.2f  }, { "deEssDetectRatio",  0.18f },
            { "airFreq",        10000.0f  },
        };

    // ── 3 : Voix Room ─────────────────────────────────────────────────────────
    // Correction pièce prononcée : room notch + de-rev poussés
    case 3:
        return {
            { "clean",              0.32f }, { "deess",            0.22f },
            { "room",               0.80f }, { "reverb",           0.72f },
            { "air",                0.12f },
            { "cleanGateThreshold",-36.0f }, { "cleanGateFloor",  -50.0f },
            { "cleanGateAttack",    0.005f}, { "cleanGateHold",    0.050f},
            { "cleanRelease",       0.25f },
            { "nrThreshold",       -58.0f }, { "nrRatio",           2.0f },
            { "nrFloor",           -12.0f }, { "nrSmooth",          0.030f},
            { "deEssFreq",       6500.0f  }, { "deEssThreshold",  -40.0f },
            { "deEssIntensity",     0.8f  }, { "deEssDetectRatio",  0.12f },
            { "roomNotchFreq",    440.0f  }, { "roomNotchQ",         2.5f },
            { "airFreq",        11000.0f  },
        };

    // ── 4 : Voix Chuchotée / ASMR ────────────────────────────────────────────
    // Gate lent et très sensible, NR délicat, de-ess minimal
    case 4:
        return {
            { "clean",              0.58f }, { "deess",            0.12f },
            { "room",               0.18f }, { "reverb",           0.22f },
            { "air",                0.22f },
            { "cleanGateThreshold",-48.0f }, { "cleanGateFloor",  -62.0f },
            { "cleanGateAttack",    0.008f}, { "cleanGateHold",    0.080f},
            { "cleanRelease",       0.45f },
            { "nrThreshold",       -42.0f }, { "nrRatio",           2.0f },
            { "nrFloor",           -10.0f }, { "nrSmooth",          0.050f},
            { "deEssFreq",       8000.0f  }, { "deEssThreshold",  -44.0f },
            { "deEssIntensity",     0.5f  }, { "deEssDetectRatio",  0.10f },
            { "airFreq",        12000.0f  },
        };

    default:
        return {};
    }
}

static const char* getFactoryPresetName(int i)
{
    switch (i) {
    case 0:  return "Init";
    case 1:  return "Podcast Voix Proche";
    case 2:  return "Live Streaming";
    case 3:  return "Voix Room";
    case 4:  return "Voix Chuchotee / ASMR";
    default: return "Preset";
    }
}

juce::File InvisibleFixAudioProcessor::getUserPresetsDir() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("InvisibleFix").getChildFile("Presets");
}

void InvisibleFixAudioProcessor::buildPresetList()
{
    presetNames.clear();
    presetIsFactory.clear();
    userPresetFiles.clear();

    for (int i = 0; i < NUM_FACTORY_PRESETS; ++i)
    {
        presetNames.push_back(getFactoryPresetName(i));
        presetIsFactory.push_back(true);
    }

    auto dir = getUserPresetsDir();
    if (dir.isDirectory())
    {
        for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.ifx"))
        {
            presetNames.push_back(f.getFileNameWithoutExtension());
            presetIsFactory.push_back(false);
            userPresetFiles.push_back(f);
        }
    }
}

void InvisibleFixAudioProcessor::loadPreset(int index)
{
    if (index < 0 || index >= (int)presetNames.size()) return;

    if (presetIsFactory[index])
    {
        // Remettre tous les paramètres audio à leur valeur par défaut d'abord
        for (auto* p : apvts.processor.getParameters())
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
                rp->setValueNotifyingHost(rp->getDefaultValue());
        }

        // Appliquer les overrides du preset (valeurs réelles → normalisées)
        auto params = getFactoryPresetParams(index);
        for (auto& [id, val] : params)
        {
            if (auto* p = apvts.getParameter(id))
            {
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
                {
                    auto& r   = rp->getNormalisableRange();
                    float norm = juce::jlimit(0.0f, 1.0f, (val - r.start) / (r.end - r.start));
                    rp->setValueNotifyingHost(norm);
                }
            }
        }
    }
    else
    {
        // Preset utilisateur : charger depuis le fichier XML
        int userIdx = index - NUM_FACTORY_PRESETS;
        if (userIdx >= 0 && userIdx < (int)userPresetFiles.size())
        {
            auto xmlStr = userPresetFiles[userIdx].loadFileAsString();
            if (auto xml = juce::XmlDocument::parse(xmlStr))
            {
                auto vt = juce::ValueTree::fromXml(*xml);
                if (vt.isValid())
                    apvts.replaceState(vt);
            }
        }
    }

    currentPresetIndex    = index;
    currentPresetName     = presetNames[index];
    currentPresetIsFactory = presetIsFactory[index];
}

void InvisibleFixAudioProcessor::savePreset(const juce::String& name)
{
    auto dir = getUserPresetsDir();
    dir.createDirectory();
    auto file = dir.getChildFile(name + ".ifx");

    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        xml->writeToFile(file, {});

    buildPresetList();

    // Trouver le preset qu'on vient de sauvegarder et le marquer courant
    for (int i = 0; i < (int)presetNames.size(); ++i)
        if (presetNames[i] == name) { currentPresetIndex = i; break; }

    currentPresetName      = name;
    currentPresetIsFactory = false;
}

void InvisibleFixAudioProcessor::deleteUserPreset(int index)
{
    if (index < NUM_FACTORY_PRESETS || index >= (int)presetNames.size()) return;
    int userIdx = index - NUM_FACTORY_PRESETS;
    if (userIdx < (int)userPresetFiles.size())
        userPresetFiles[userIdx].deleteFile();
    buildPresetList();
    if (currentPresetIndex >= (int)presetNames.size())
        currentPresetIndex = 0;
}

// Surcharge des méthodes programme JUCE (navigation DAW)
int            InvisibleFixAudioProcessor::getNumPrograms()        { return (int)presetNames.size(); }
int            InvisibleFixAudioProcessor::getCurrentProgram()     { return currentPresetIndex; }
void           InvisibleFixAudioProcessor::setCurrentProgram(int i){ loadPreset(i); }
const juce::String InvisibleFixAudioProcessor::getProgramName(int i)
{
    if (i >= 0 && i < (int)presetNames.size()) return presetNames[i];
    return {};
}
void InvisibleFixAudioProcessor::changeProgramName(int, const juce::String&) {}

void InvisibleFixAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("editorScale", (double)editorScale.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void InvisibleFixAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto vt = juce::ValueTree::fromXml(*xmlState);
        editorScale.store((float)(double)vt.getProperty("editorScale", 1.0));
        apvts.replaceState(vt);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout InvisibleFixAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "clean", 1 }, "Clean NR", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "deess", 1 }, "De-Ess", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "room", 1 }, "Room Notch", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "reverb", 1 }, "Reverb Absorber", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "air", 1 }, "Air", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "inputGain", 1 }, "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    // Bypass modules (5)
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "cleanBypass", 1 }, "Clean Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "roomBypass", 1 }, "Room Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "reverbBypass", 1 }, "Reverb Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "deessBypass", 1 }, "De-Ess Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "airBypass", 1 }, "Air Bypass", false));

    // Delta modes
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "cleanDelta", 1 }, "Clean Delta", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "roomDelta", 1 }, "Room Delta", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "reverbDelta", 1 }, "Reverb Delta", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "deessDelta", 1 }, "De-Ess Delta", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "airDelta", 1 }, "Air Delta", false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "airFreq", 1 }, "Air Frequency", 7000.0f, 16000.0f, 11000.0f));

    // De-Ess Advanced
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "deEssFreq", 1 }, "De-Ess Frequency", 2000.0f, 9000.0f, 5500.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "deEssThreshold", 1 }, "De-Ess Threshold",
        juce::NormalisableRange<float>(-60.0f, -20.0f, 0.5f), -42.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "deEssIntensity", 1 }, "De-Ess Intensity Multiplier", 0.0f, 2.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "deEssAttack", 1 }, "De-Ess Attack", 0.001f, 0.010f, 0.003f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "deEssRelease", 1 }, "De-Ess Release", 0.005f, 0.060f, 0.015f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "deEssFilterMode", 1 }, "De-Ess Bell Mode", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "deEssDetectRatio", 1 }, "De-Ess Detect Ratio", 0.05f, 0.40f, 0.15f));

    // Gate Advanced
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanGateThreshold", 1 }, "Gate Threshold", -60.0f, 0.0f, -40.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanGateFloor", 1 }, "Gate Floor", -80.0f, 0.0f, -24.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanGateAttack", 1 }, "Gate Attack", 0.0005f, 0.010f, 0.002f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanGateHold", 1 }, "Gate Hold", 0.005f, 0.100f, 0.030f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanRelease", 1 }, "Gate Release", 0.05f, 0.60f, 0.22f));

    // Air Advanced
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "airDrive", 1 }, "Air Drive Saturation", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ "airSatMode", 1 }, "Air Sat Mode", 0, 3, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "airShape", 1 }, "Air Transient Shape",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    // Room Advanced
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "roomNotchFreq", 1 }, "Room Notch Freq", 150.0f, 1200.0f, 440.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "roomNotchQ", 1 }, "Room Notch Q", 0.8f, 7.0f, 2.2f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "roomDepthBoost", 1 }, "Room Depth Boost", 0.5f, 2.5f, 1.0f));

    // NR — Noise Reduction 3-bandes
    // De-Rev Floor
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "deRevFloor", 1 }, "De-Rev Floor", 0.0f, 0.65f, 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "deRevBandLF", 1 }, "De-Rev Band LF", 0.0f, 1.0f, 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "deRevBandMF", 1 }, "De-Rev Band MF", 0.0f, 1.0f, 0.75f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "deRevBandHF", 1 }, "De-Rev Band HF", 0.0f, 1.0f, 1.0f));
    // Seuils temporels par bande (0=traiter tout, 1=zone directe épargnée, queue profonde seulement)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "deRevThreshLF", 1 }, "De-Rev Thresh LF", 0.0f, 1.0f, 0.95f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "deRevThreshMF", 1 }, "De-Rev Thresh MF", 0.0f, 1.0f, 0.80f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "deRevThreshHF", 1 }, "De-Rev Thresh HF", 0.0f, 1.0f, 0.65f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrThreshold", 1 }, "NR Threshold",
        juce::NormalisableRange<float>(-80.0f, -10.0f, 0.1f), -40.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrRatio", 1 }, "NR Ratio", 1.0f, 8.0f, 3.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrFloor", 1 }, "NR Floor",
        juce::NormalisableRange<float>(-80.0f, 0.0f, 0.1f), -12.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrSmooth", 1 }, "NR Smooth",
        juce::NormalisableRange<float>(0.001f, 0.100f, 0.001f), 0.020f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrArtifactBlend", 1 }, "NR Artifact Blend", 0.0f, 1.0f, 0.0f));

    // Global LIVE/HQ mode
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "liveMode", 1 }, "Live Mode", false));

    // Bypass indépendants NR et Gate (dans le module Clean)
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "nrBypass",   1 }, "NR Bypass",   false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "gateBypass", 1 }, "Gate Bypass", false));
    // Protection plosives — maintient le gate ouvert sur les transitoires BF
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanPlosiveProtect", 1 }, "Plosive Protect", 0.0f, 1.0f, 0.25f));

    return layout;
}

void InvisibleFixAudioProcessor::copyNRBandGains(float* dest, int numBands)
{
    int n = juce::jmin(numBands, 6);
    for (int b = 0; b < n; ++b)
        dest[b] = (nrBandGain[b][0] + nrBandGain[b][1]) * 0.5f;
}

void InvisibleFixAudioProcessor::copyFftMagnitudes(float* dest, int numBins)
{
    juce::ScopedLock lock(fftMutex);
    int count = juce::jmin(numBins, fftSize / 2);
    std::copy(fftMagnitudes, fftMagnitudes + count, dest);
}

void InvisibleFixAudioProcessor::copyInputFftMagnitudes(float* dest, int numBins)
{
    juce::ScopedLock lock(fftMutex);
    int count = juce::jmin(numBins, fftSize / 2);
    std::copy(inputFftMagnitudes, inputFftMagnitudes + count, dest);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InvisibleFixAudioProcessor();
}