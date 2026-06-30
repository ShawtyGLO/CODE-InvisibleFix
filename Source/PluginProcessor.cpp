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
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

InvisibleFixAudioProcessor::~InvisibleFixAudioProcessor() {}

const juce::String InvisibleFixAudioProcessor::getName() const { return JucePlugin_Name; }
bool InvisibleFixAudioProcessor::acceptsMidi() const { return false; }
bool InvisibleFixAudioProcessor::producesMidi() const { return false; }
bool InvisibleFixAudioProcessor::isMidiEffect() const { return false; }
double InvisibleFixAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int InvisibleFixAudioProcessor::getNumPrograms() { return 1; }
int InvisibleFixAudioProcessor::getCurrentProgram() { return 0; }
void InvisibleFixAudioProcessor::setCurrentProgram(int) {}
const juce::String InvisibleFixAudioProcessor::getProgramName(int) { return {}; }
void InvisibleFixAudioProcessor::changeProgramName(int, const juce::String&) {}

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

        // De-Ess look-ahead + adaptatif
        deEssLookaheadWrite[ch] = 0;
        deEssAdaptEnv[ch] = 0.0f;
        std::memset(deEssLookahead[ch], 0, sizeof(float) * DEESS_LOOKAHEAD_MAX);

        rmsWritePos[ch] = 0;
        rmsSumSq[ch]    = 0.0f;
        for (int i = 0; i < RMS_WINDOW_SIZE; ++i)
            rmsBuffer[ch][i] = 0.0f;
        gateSidechainZ1[ch]  = 0.0; gateSidechainZ2[ch]  = 0.0;
        gateSidechainZ1b[ch] = 0.0; gateSidechainZ2b[ch] = 0.0;

        airShelfZ1[ch] = 0.0; airShelfZ2[ch] = 0.0;
        heatHPZ1[ch]  = 0.0; heatHPZ2[ch]  = 0.0;
        smoothedOutputGain[ch] = 1.0f;

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
    nrWindowPeriod = juce::jmax(1024, (int)(0.5f * (float)currentSampleRate));
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

    // --- LECTURE DES PARAMÈTRES DE FAÇADE ---
    float knobClean = *apvts.getRawParameterValue("clean");
    float knobDeEss = *apvts.getRawParameterValue("deess");
    float knobRoom = *apvts.getRawParameterValue("room");
    float knobReverb = *apvts.getRawParameterValue("reverb");
    float knobAir = *apvts.getRawParameterValue("air");
    float knobOutputGain = *apvts.getRawParameterValue("outputGain");

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
    float advDeEssThreshDB = *apvts.getRawParameterValue("deEssThreshold");
    float advDeEssDetectRatio = *apvts.getRawParameterValue("deEssDetectRatio");
    float advAirDrive = *apvts.getRawParameterValue("airDrive");

    float currentRoomFreq = *apvts.getRawParameterValue("roomNotchFreq");
    float currentRoomQ = *apvts.getRawParameterValue("roomNotchQ");
    if ((knobRoom > 0.001f || roomDelta) &&
        (knobRoom != lastRoomValue || currentRoomFreq != lastRoomFreq || currentRoomQ != lastRoomQ))
    {
        updateRoomCoefficients(knobRoom);
        lastRoomValue = knobRoom;
        lastRoomFreq = currentRoomFreq;
        lastRoomQ = currentRoomQ;
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
    float rvSlowR[3]={ 1.f-std::exp(-1.f/(0.500f*rvSr)), 1.f-std::exp(-1.f/(0.200f*rvSr)), 1.f-std::exp(-1.f/(0.080f*rvSr)) };

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
    bool nrActive = (nrAmount > 0.001f && !cleanBypass);

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

    // Détecteur De-Ess : passe-bande étroit centré sur deEssFreq
    double deDetB0{ 1.0 }, deDetB1{ 0.0 }, deDetB2{ 0.0 }, deDetA1{ 0.0 }, deDetA2{ 0.0 };
    if (knobDeEss > 0.001f && !deessBypass) {
        double sideFreq = juce::jlimit(2000.0, 9000.0, (double)advDeEssFreq);
        double w0d = juce::MathConstants<double>::twoPi * sideFreq / currentSampleRate;
        double cd = std::cos(w0d), sd = std::sin(w0d);
        double ad = sd / (2.0 * 5.0); // Q=5.0, bande étroite isole la sibilante
        double b0d = ad, b1d = 0.0, b2d = -ad;
        double a0d = 1.0 + ad;
        deDetB0 = b0d / a0d; deDetB1 = b1d / a0d; deDetB2 = b2d / a0d;
        deDetA1 = -2.0 * cd / a0d; deDetA2 = (1.0 - ad) / a0d;
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
                if (knobClean > 0.001f)
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

                    if (channel == 0) gateIsOpenForDisplay = gateIsOpen[0];

                    float targetGateGain = 1.0f;
                    if (!gateIsOpen[channel]) {
                        float floor = juce::Decibels::decibelsToGain(advGateFloorDB);
                        float ratio = currentEnv / gateThresholdLin;
                        targetGateGain = floor + (ratio * ratio) * (1.0f - floor);
                        targetGateGain = juce::jlimit(floor, 1.0f, targetGateGain);
                    }

                    float cCoef = (targetGateGain > smoothedGateGain[channel]) ? fastGateAttackCoef : fastGateReleaseCoef;
                    smoothedGateGain[channel] += cCoef * (targetGateGain - smoothedGateGain[channel]);
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

                        float g = 1.0f + (nrBandGain[b][channel] - 1.0f) * nrAmount;
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
                    if (revBandSlow[b][channel] > 1e-5f)
                    {
                        float transRatio = juce::jlimit(0.0f, 1.5f,
                            revBandFast[b][channel] / (revBandSlow[b][channel] + 1e-6f));
                        float tailDepth = juce::jmax(0.0f, 1.0f - transRatio);
                        // Protection vocale : sidechain élevé → réduction d'aggressivité
                        float voiceGuard = juce::jmin(0.7f,
                            scLevel / (revBandSlow[b][channel] + 1e-6f) * 0.5f);
                        gain = 1.0f - knobReverb * 0.65f * tailDepth * (1.0f - voiceGuard);
                        gain = juce::jlimit(0.35f, 1.0f, gain);
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
                // Lecture du signal retardé de 2ms
                int rdPos = (deEssLookaheadWrite[channel] - deEssLookaheadLen - 1 + DEESS_LOOKAHEAD_MAX) % DEESS_LOOKAHEAD_MAX;
                double xDelayed = (double)deEssLookahead[channel][rdPos];
                preDeEssX = xDelayed; // delta aligné sur le signal retardé
                // Sidechain détecte sur x COURANT (non retardé) → look-ahead effectif
                float rawEnv = std::abs((float)x);
                double hpDet = deDetB0 * x + deEssHpZ1[channel];
                deEssHpZ1[channel] = deDetB1 * x - deDetA1 * hpDet + deEssHpZ2[channel];
                deEssHpZ2[channel] = deDetB2 * x - deDetA2 * hpDet;
                float deEssEnv = std::abs((float)hpDet);

                // Seuil adaptatif : slow follower du niveau sibilant (~2s, style Orban 764A)
                // Si la voix est naturellement très sibilante → threshold monte légèrement
                deEssAdaptEnv[channel] += deEssAdaptCoef * (deEssEnv - deEssAdaptEnv[channel]);
                float baseThreshLin = juce::Decibels::decibelsToGain(advDeEssThreshDB);
                float adaptRatio = deEssAdaptEnv[channel] / (baseThreshLin + 1e-7f);
                float adaptOffsetDB = juce::jlimit(0.0f, 6.0f,
                    20.0f * std::log10(juce::jmax(1.0f, adaptRatio)) * 0.35f);
                float deEssThreshLin = juce::Decibels::decibelsToGain(advDeEssThreshDB + adaptOffsetDB);

                float targetCleanGain = 1.0f;
                if (deEssEnv > 0.0002f && deEssEnv > (rawEnv * advDeEssDetectRatio))
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
                double safeDeEssFreq = juce::jlimit(4500.0f, 9500.0f, advDeEssFreq);
                double w0c = juce::MathConstants<double>::twoPi * safeDeEssFreq / currentSampleRate;
                double cosW = std::cos(w0c), sinW = std::sin(w0c);
                double cb0, cb1, cb2, ca0, ca1, ca2;
                if (isBellMode) {
                    double alphaBell = sinW / (2.0 * 3.5);
                    cb0 = 1.0 + alphaBell * Acl; cb1 = -2.0 * cosW; cb2 = 1.0 - alphaBell * Acl;
                    ca0 = 1.0 + alphaBell / Acl; ca1 = -2.0 * cosW; ca2 = 1.0 - alphaBell / Acl;
                } else {
                    double alph = sinW / (2.0 * 0.707) * std::sqrt((Acl + 1.0 / Acl) * (1.0 / 0.707 - 1.0) + 2.0);
                    cb0 = Acl * ((Acl + 1.0) + (Acl - 1.0) * cosW + 2.0 * std::sqrt(Acl) * alph);
                    cb1 = -2.0 * Acl * ((Acl - 1.0) + (Acl + 1.0) * cosW);
                    cb2 = Acl * ((Acl + 1.0) + (Acl - 1.0) * cosW - 2.0 * std::sqrt(Acl) * alph);
                    ca0 = (Acl + 1.0) - (Acl - 1.0) * cosW + 2.0 * std::sqrt(Acl) * alph;
                    ca1 = 2.0 * ((Acl - 1.0) - (Acl + 1.0) * cosW);
                    ca2 = (Acl + 1.0) - (Acl - 1.0) * cosW - 2.0 * std::sqrt(Acl) * alph;
                }
                // Le filtre est appliqué sur xDelayed (signal retardé de 2ms)
                // La réduction de gain est déjà calculée avant l'arrivée de la sibilance → look-ahead
                double aDeEssOut = (cb0 / ca0) * xDelayed + cleanShelfZ1[channel];
                cleanShelfZ1[channel] = (cb1 / ca0) * xDelayed - (ca1 / ca0) * aDeEssOut + cleanShelfZ2[channel];
                cleanShelfZ2[channel] = (cb2 / ca0) * xDelayed - (ca2 / ca0) * aDeEssOut;
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
                    double w0h = juce::MathConstants<double>::twoPi * (double)advAirFreq / currentSampleRate;
                    double cosH = std::cos(w0h), sinH = std::sin(w0h);
                    double alpH = sinH / (2.0 * 0.707);
                    double hpb0 = (1.0 + cosH) * 0.5,  hpb1 = -(1.0 + cosH), hpb2 = (1.0 + cosH) * 0.5;
                    double hpa0 = 1.0 + alpH,           hpa1 = -2.0 * cosH,   hpa2 = 1.0 - alpH;
                    double hpOut = (hpb0 / hpa0) * x + heatHPZ1[channel];
                    heatHPZ1[channel] = (hpb1 / hpa0) * x - (hpa1 / hpa0) * hpOut + heatHPZ2[channel];
                    heatHPZ2[channel] = (hpb2 / hpa0) * x - (hpa2 / hpa0) * hpOut;
                    // Saturation du contenu haute fréquence isolé
                    float driveScale = 1.0f + driveAmt * 5.0f;
                    float satIn  = (float)hpOut * driveScale;
                    float satOut = std::tanh(satIn) / driveScale; // compenser le gain
                    // Injection des harmoniques générées (différence sat - linéaire = distorsion pure)
                    heatSignal = (double)((satOut - (float)hpOut) * driveAmt * 0.7f);
                }

                // === SHELF AIR : boost haute fréquence, indépendant du Heat ===
                if (knobAir > 0.001f)
                {
                    float boost = juce::jlimit(0.0f, 9.5f, 9.5f * knobAir);
                    double Aa = std::pow(10.0, (double)boost / 40.0);
                    double w0a = juce::MathConstants<double>::twoPi * (double)advAirFreq / currentSampleRate;
                    double cosA = std::cos(w0a);
                    double alpA = std::sin(w0a) / (2.0 * 0.707) * std::sqrt((Aa + 1.0 / Aa) * (1.0 / 0.707 - 1.0) + 2.0);
                    double ab0 = Aa * ((Aa + 1.0) + (Aa - 1.0) * cosA + 2.0 * std::sqrt(Aa) * alpA);
                    double ab1 = -2.0 * Aa * ((Aa - 1.0) + (Aa + 1.0) * cosA);
                    double ab2 = Aa * ((Aa + 1.0) + (Aa - 1.0) * cosA - 2.0 * std::sqrt(Aa) * alpA);
                    double aa0 = (Aa + 1.0) - (Aa - 1.0) * cosA + 2.0 * std::sqrt(Aa) * alpA;
                    double aa1 = 2.0 * ((Aa - 1.0) - (Aa + 1.0) * cosA);
                    double aa2 = (Aa + 1.0) - (Aa - 1.0) * cosA - 2.0 * std::sqrt(Aa) * alpA;
                    double airOut = (ab0 / aa0) * x + airShelfZ1[channel];
                    airShelfZ1[channel] = (ab1 / aa0) * x - (aa1 / aa0) * airOut + airShelfZ2[channel];
                    airShelfZ2[channel] = (ab2 / aa0) * x - (aa2 / aa0) * airOut;
                    x = airOut;
                }

                // Injection Heat après le shelf (harmoniques s'ajoutent au signal final)
                x += heatSignal;
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
}

bool InvisibleFixAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* InvisibleFixAudioProcessor::createEditor()
{
    return new InvisibleFixAudioProcessorEditor(*this);
}

void InvisibleFixAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void InvisibleFixAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
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
        juce::NormalisableRange<float>(-3.0f, 3.0f, 0.01f), 0.0f));

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

    // Room Advanced
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "roomNotchFreq", 1 }, "Room Notch Freq", 150.0f, 1200.0f, 440.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "roomNotchQ", 1 }, "Room Notch Q", 0.8f, 7.0f, 2.2f));

    // NR — Noise Reduction 3-bandes
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrThreshold", 1 }, "NR Threshold",
        juce::NormalisableRange<float>(-80.0f, -10.0f, 0.1f), -40.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrRatio", 1 }, "NR Ratio", 1.0f, 8.0f, 3.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrFloor", 1 }, "NR Floor",
        juce::NormalisableRange<float>(-80.0f, 0.0f, 0.1f), -12.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrSmooth", 1 }, "NR Smooth",
        juce::NormalisableRange<float>(0.001f, 0.100f, 0.001f), 0.020f));

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