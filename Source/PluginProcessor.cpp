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
        gateIsOpen[ch] = true;
        gateHoldCounter[ch] = 0;

        reverbEnvSlow[ch] = 0.0f;

        rmsWritePos[ch] = 0;
        rmsSumSq[ch]    = 0.0f;
        for (int i = 0; i < RMS_WINDOW_SIZE; ++i)
            rmsBuffer[ch][i] = 0.0f;
        gateSidechainZ1[ch]  = 0.0; gateSidechainZ2[ch]  = 0.0;
        gateSidechainZ1b[ch] = 0.0; gateSidechainZ2b[ch] = 0.0;

        airShelfZ1[ch] = 0.0;  airShelfZ2[ch] = 0.0;
        smoothedOutputGain[ch] = 1.0f;

        nrLPLowZ1[0][ch] = 0.0; nrLPLowZ2[0][ch] = 0.0;
        nrLPLowZ1[1][ch] = 0.0; nrLPLowZ2[1][ch] = 0.0;
        nrHPHighZ1[0][ch] = 0.0; nrHPHighZ2[0][ch] = 0.0;
        nrHPHighZ1[1][ch] = 0.0; nrHPHighZ2[1][ch] = 0.0;

        for (int b = 0; b < 3; ++b) {
            nrEnv[b][ch] = 0.0f;
            nrFloor[b][ch] = 0.0f;
            nrSmoothGain[b][ch] = 1.0f;
        }
    }

    lastRoomValue = -999.0f;
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

    // Passe-bande résonant (delta Room) — même fréquence/Q
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

    // De-Rev : enveloppe rapide (transitoires) et lente (queue de réverb)
    float revFastAtk = 1.0f - std::exp(-1.0f / (0.0005f * (float)currentSampleRate));
    float revFastRel = 1.0f - std::exp(-1.0f / (0.030f  * (float)currentSampleRate));
    float revSlowAtk = 1.0f - std::exp(-1.0f / (0.003f  * (float)currentSampleRate));
    float revSlowRel = 1.0f - std::exp(-1.0f / (0.200f  * (float)currentSampleRate));

    float targetOutputGain = juce::Decibels::decibelsToGain(knobOutputGain);
    float smoothCoefOut = 1.0f - std::exp(-1.0f / (0.01f * (float)currentSampleRate));

    // NR amount = facade knob clean
    float nrAmount = knobClean;
    float nrThreshDB = *apvts.getRawParameterValue("nrThreshold");
    float nrRatioVal = *apvts.getRawParameterValue("nrRatio");
    float nrFloorDB = *apvts.getRawParameterValue("nrFloor");
    float nrSmoothVal = *apvts.getRawParameterValue("nrSmooth");
    bool nrActive = (nrAmount > 0.001f && !cleanBypass);

    // Coefficients LR4 (recaclulés à chaque bloc)
    double nrLPFreq = 250.0, nrHPFreq = 8000.0;
    double nrQ = 0.707;
    {
        double w0L = juce::MathConstants<double>::twoPi * nrLPFreq / currentSampleRate;
        double cL = std::cos(w0L), sL = std::sin(w0L);
        double aL = sL / (2.0 * nrQ);
        double b0L = (1.0 - cL) / 2.0, b1L = 1.0 - cL, b2L = (1.0 - cL) / 2.0;
        double a0L = 1.0 + aL, a1L = -2.0 * cL, a2L = 1.0 - aL;
        for (int i = 0; i < 2; ++i) {
            nrLPLowB0[i] = b0L / a0L; nrLPLowB1[i] = b1L / a0L; nrLPLowB2[i] = b2L / a0L;
            nrLPLowA1[i] = a1L / a0L; nrLPLowA2[i] = a2L / a0L;
        }
        double w0H = juce::MathConstants<double>::twoPi * nrHPFreq / currentSampleRate;
        double cH = std::cos(w0H), sH = std::sin(w0H);
        double aH = sH / (2.0 * nrQ);
        double b0H = (1.0 + cH) / 2.0, b1H = -(1.0 + cH), b2H = (1.0 + cH) / 2.0;
        double a0H = 1.0 + aH, a1H = -2.0 * cH, a2H = 1.0 - aH;
        for (int i = 0; i < 2; ++i) {
            nrHPHighB0[i] = b0H / a0H; nrHPHighB1[i] = b1H / a0H; nrHPHighB2[i] = b2H / a0H;
            nrHPHighA1[i] = a1H / a0H; nrHPHighA2[i] = a2H / a0H;
        }
    }

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
                    float noiseFloorEst = nrFloor[1][channel];
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
                    double low1 = nrLPLowB0[0] * x + nrLPLowZ1[0][channel];
                    nrLPLowZ1[0][channel] = nrLPLowB1[0] * x - nrLPLowA1[0] * low1 + nrLPLowZ2[0][channel];
                    nrLPLowZ2[0][channel] = nrLPLowB2[0] * x - nrLPLowA2[0] * low1;
                    double low2 = nrLPLowB0[1] * low1 + nrLPLowZ1[1][channel];
                    nrLPLowZ1[1][channel] = nrLPLowB1[1] * low1 - nrLPLowA1[1] * low2 + nrLPLowZ2[1][channel];
                    nrLPLowZ2[1][channel] = nrLPLowB2[1] * low1 - nrLPLowA2[1] * low2;
                    double lowBand = low2;
                    double high1 = nrHPHighB0[0] * x + nrHPHighZ1[0][channel];
                    nrHPHighZ1[0][channel] = nrHPHighB1[0] * x - nrHPHighA1[0] * high1 + nrHPHighZ2[0][channel];
                    nrHPHighZ2[0][channel] = nrHPHighB2[0] * x - nrHPHighA2[0] * high1;
                    double high2 = nrHPHighB0[1] * high1 + nrHPHighZ1[1][channel];
                    nrHPHighZ1[1][channel] = nrHPHighB1[1] * high1 - nrHPHighA1[1] * high2 + nrHPHighZ2[1][channel];
                    nrHPHighZ2[1][channel] = nrHPHighB2[1] * high1 - nrHPHighA2[1] * high2;
                    double highBand = high2;
                    double midBand = x - lowBand - highBand;
                    float bands[3] = { (float)lowBand, (float)midBand, (float)highBand };

                    for (int b = 0; b < 3; ++b)
                    {
                        float absBand = std::abs(bands[b]);
                        float nrEnvCoef = (absBand > nrEnv[b][channel]) ? 0.1f : 0.005f;
                        nrEnv[b][channel] += nrEnvCoef * (absBand - nrEnv[b][channel]);
                        // Apprentissage rapide pendant les silences du gate, lent sinon
                        float nrLearnCoef = gateIsOpen[channel] ? 0.00005f : 0.004f;
                        if (nrEnv[b][channel] < nrFloor[b][channel] * 2.0f)
                            nrFloor[b][channel] += nrLearnCoef * (nrEnv[b][channel] - nrFloor[b][channel]);
                        // Seuil auto = 3× le plancher appris, ou le seuil manuel si plus haut
                        float autoThreshold = nrFloor[b][channel] * 3.0f;
                        float thresholdLin = juce::Decibels::decibelsToGain(nrThreshDB);
                        float threshold = juce::jmax(autoThreshold, thresholdLin);
                        float ratio = nrRatioVal;
                        float targetGain = 1.0f;
                        if (nrEnv[b][channel] < threshold && threshold > 0.00001f)
                        {
                            float reduction = 1.0f - (1.0f - juce::Decibels::decibelsToGain(nrFloorDB))
                                * std::pow(1.0f - nrEnv[b][channel] / threshold, ratio);
                            targetGain = juce::jlimit(juce::Decibels::decibelsToGain(nrFloorDB), 1.0f, reduction);
                        }
                        float nrSmoothCoef = (targetGain < nrSmoothGain[b][channel]) ? (nrSmoothVal * 2.0f) : (nrSmoothVal * 0.5f);
                        nrSmoothGain[b][channel] += nrSmoothCoef * (targetGain - nrSmoothGain[b][channel]);
                        float g = 1.0f + (nrSmoothGain[b][channel] - 1.0f) * nrAmount;
                        bands[b] *= g;
                    }
                    x = (double)(bands[0] + bands[1] + bands[2]);
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
                // ROOM DELTA : passe-bande résonant (entendre la bande notchée)
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

            // === DE-REV (dual-envelope : fast=transitoires, slow=queue de reverb) ===
            if (knobReverb > 0.001f && !reverbBypass && !cleanDelta) {
                float absX = std::abs((float)x);
                float cfFast = (absX > reverbEnv[channel])     ? revFastAtk : revFastRel;
                reverbEnv[channel]     += cfFast * (absX - reverbEnv[channel]);
                float cfSlow = (absX > reverbEnvSlow[channel]) ? revSlowAtk : revSlowRel;
                reverbEnvSlow[channel] += cfSlow * (absX - reverbEnvSlow[channel]);
                if (reverbEnvSlow[channel] > 0.00001f) {
                    // Ratio < 1 = on est dans la queue de réverb → on réduit
                    float transientRatio = reverbEnv[channel] / (reverbEnvSlow[channel] + 1e-6f);
                    transientRatio = juce::jlimit(0.0f, 1.5f, transientRatio);
                    float tailDepth = juce::jmax(0.0f, 1.0f - transientRatio);
                    float deReverbGain = 1.0f - (knobReverb * 0.65f * tailDepth);
                    deReverbGain = juce::jlimit(0.35f, 1.0f, deReverbGain);
                    x *= (double)deReverbGain;
                }
            }

            // === REVERB DELTA : sortie = différence DE-REV uniquement ===
            if (reverbDelta) {
                x = preReverbX - x;
                data[sample] = (float)(x * (double)smoothedOutputGain[channel]);
                continue;
            }

            // Capture pré-DE-ESS (pour deessDelta)
            double preDeEssX = x;

            // === DE-ESS ===
            if (knobDeEss > 0.001f && !deessBypass && !cleanDelta)
            {
                float rawEnv = std::abs((float)x);
                double hpDet = deDetB0 * x + deEssHpZ1[channel];
                deEssHpZ1[channel] = deDetB1 * x - deDetA1 * hpDet + deEssHpZ2[channel];
                deEssHpZ2[channel] = deDetB2 * x - deDetA2 * hpDet;
                float deEssEnv = std::abs((float)hpDet);
                float targetCleanGain = 1.0f;
                float deEssThreshLin = juce::Decibels::decibelsToGain(advDeEssThreshDB);
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
                double aDeEssOut = (cb0 / ca0) * x + cleanShelfZ1[channel];
                cleanShelfZ1[channel] = (cb1 / ca0) * x - (ca1 / ca0) * aDeEssOut + cleanShelfZ2[channel];
                cleanShelfZ2[channel] = (cb2 / ca0) * x - (ca2 / ca0) * aDeEssOut;
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
            if (knobAir > 0.001f && !airBypass && !cleanDelta) {
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
                if (advAirDrive > 0.001f) {
                    float driveAmt = juce::jlimit(0.0f, 1.0f, advAirDrive);
                    float satIn = (float)x * (1.0f + driveAmt * 8.0f);
                    float satOut = std::tanh(satIn);
                    x = (double)((float)x * (1.0f - driveAmt * 0.7f) + satOut * (driveAmt * 0.7f));
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
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanGateFloor", 1 }, "Gate Floor", -45.0f, 0.0f, -24.0f));
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
        juce::NormalisableRange<float>(-30.0f, 0.0f, 0.1f), -12.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "nrSmooth", 1 }, "NR Smooth",
        juce::NormalisableRange<float>(0.001f, 0.100f, 0.001f), 0.020f));

    return layout;
}

void InvisibleFixAudioProcessor::copyFftMagnitudes(float* dest, int numBins)
{
    juce::ScopedLock lock(fftMutex);
    int count = juce::jmin(numBins, fftSize / 2);
    std::copy(fftMagnitudes, fftMagnitudes + count, dest);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InvisibleFixAudioProcessor();
}