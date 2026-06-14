#include "PluginProcessor.h"
#include "PluginEditor.h"

// =============================================================================
//  InvisibleFix — PluginProcessor.cpp
//  V1.00 — Séparation Totale Clean (Gate) / DeEss & Delta Dynamique Pur
// =============================================================================

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
    currentBlockSize = samplesPerBlock;

    for (int ch = 0; ch < 2; ++ch)
    {
        notchZ1[ch] = 0.0;  notchZ2[ch] = 0.0;
        shelfZ1[ch] = 0.0;  shelfZ2[ch] = 0.0;

        cleanShelfZ1[ch] = 0.0;  cleanShelfZ2[ch] = 0.0;
        smoothedCleanGain[ch] = 1.0f;
        smoothedGateGain[ch] = 1.0f;

        reverbEnv[ch] = 0.0f;
        gateIsOpen[ch] = true;

        plosiveZ1[ch] = 0.0f; plosiveZ2[ch] = 0.0f;

        deEssSidechainZ1[ch] = 0.0; deEssSidechainZ2[ch] = 0.0;

        airShelfZ1[ch] = 0.0;  airShelfZ2[ch] = 0.0;
        smoothedOutputGain[ch] = 1.0f;
    }

    lastRoomValue = -999.0f;
    lastFocusValue = -999.0f;
}

void InvisibleFixAudioProcessor::releaseResources() {}

bool InvisibleFixAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void InvisibleFixAudioProcessor::updateRoomCoefficients(float room, float focus)
{
    double targetNotchFreq = (double)*apvts.getRawParameterValue("roomNotchFreq");
    double targetNotchQ = (double)*apvts.getRawParameterValue("roomNotchQ");

    double fc = targetNotchFreq + (1.0 - (double)focus) * 50.0;
    fc = juce::jlimit(100.0, 2000.0, fc);

    double Q = targetNotchQ * (1.0 + (double)focus * 1.5);
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
    float knobClean = *apvts.getRawParameterValue("clean");      // Devient le Noise Gate Pur
    float knobDeEss = *apvts.getRawParameterValue("deess");      // Nouveau bouton De-Esser en façade
    float knobPlosive = *apvts.getRawParameterValue("plosive");
    float knobRoom = *apvts.getRawParameterValue("room");
    float knobReverb = *apvts.getRawParameterValue("reverb");
    float knobAir = *apvts.getRawParameterValue("air");
    float knobOutputGain = *apvts.getRawParameterValue("outputGain");
    float knobFocus = *apvts.getRawParameterValue("focus");

    // --- PARAMÈTRES AVANCÉS MODULE CLEAN / DEESS ---
    float advAirFreq = *apvts.getRawParameterValue("airFreq");
    bool  isDeltaActive = *apvts.getRawParameterValue("cleanDelta") > 0.5f;

    float advDeEssInten = *apvts.getRawParameterValue("cleanDeEss");
    float advDeEssFreq = *apvts.getRawParameterValue("cleanDeEssFreq");
    bool  isBellMode = *apvts.getRawParameterValue("cleanDeEssFilterMode") > 0.5f;
    float advDeEssRel = *apvts.getRawParameterValue("cleanDeEssRelease");
    float advGateFloorDB = *apvts.getRawParameterValue("cleanGateFloor");
    float advCleanRelease = *apvts.getRawParameterValue("cleanRelease");

    if (knobRoom > 0.001f) updateRoomCoefficients(knobRoom, knobFocus);

    // --- CONFIGURATION SÉPARÉE DES ENVELOPPES (AJUSTÉE POUR SENSIBILITÉ ACCRUE) ---
    // 1. Noise Gate : Seuil max augmenté à 0.08f pour une détection franche et puissante
    float gateThreshold = 0.0001f + (knobClean * 0.08f);
    float fastGateAttack = 1.0f - std::exp(-1.0f / (0.002f * (float)currentSampleRate));
    float fastGateRelease = 1.0f - std::exp(-1.0f / (advCleanRelease * (float)currentSampleRate));

    // 2. De-Esser : Plus de polyvalence
    float deEssRatio = 1.0f + (knobDeEss * 15.0f * advDeEssInten);

    float revRelease = 1.0f - std::exp(-1.0f / (0.15f * (float)currentSampleRate));
    float revAttack = 1.0f - std::exp(-1.0f / (0.002f * (float)currentSampleRate));

    float targetOutputGain = juce::Decibels::decibelsToGain(knobOutputGain);
    float smoothCoefOut = 1.0f - std::exp(-1.0f / (0.01f * (float)currentSampleRate));

    // --- COEFFICIENTS ANTI-PLOSIVE (140 Hz HPF) ---
    double w0p = juce::MathConstants<double>::twoPi * 140.0 / currentSampleRate;
    double cosW0p = std::cos(w0p);
    double alphaP = std::sin(w0p) / (2.0 * 0.707);
    double pB0 = (1.0 + cosW0p) / 2.0;
    double pB1 = -(1.0 + cosW0p);
    double pB2 = (1.0 + cosW0p) / 2.0;
    double pA0 = 1.0 + alphaP;
    double pA1 = -2.0 * cosW0p;
    double pA2 = 1.0 - alphaP;
    double plB0 = pB0 / pA0, plB1 = pB1 / pA0, plB2 = pB2 / pA0;
    double plA1 = pA1 / pA0, plA2 = pA2 / pA0;

    // --- FILTRE DETECTEUR DU DE-ESSER (Passe-Haut strict 6kHz anti-respirations) ---
    double w0sb = juce::MathConstants<double>::twoPi * 6000.0 / currentSampleRate;
    double cosW0sb = std::cos(w0sb);
    double alphaSb = std::sin(w0sb) / (2.0 * 0.707);
    double sbB0 = (1.0 + cosW0sb) / 2.0;
    double sbB1 = -(1.0 + cosW0sb);
    double sbB2 = (1.0 + cosW0sb) / 2.0;
    double sbA0 = 1.0 + alphaSb;
    double sbA1 = -2.0 * cosW0sb;
    double sbA2 = 1.0 - alphaSb;
    double sideB0 = sbB0 / sbA0, sideB1 = sbB1 / sbA0, sideB2 = sbB2 / sbA0;
    double sideA1 = sbA1 / sbA0, sideA2 = sbA2 / sbA0;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            double originalX = (double)data[sample];
            double x = originalX;

            float rawEnv = std::abs((float)originalX);

            // Sidechain hautes fréquences
            double hpSideSample = sideB0 * originalX + deEssSidechainZ1[channel];
            deEssSidechainZ1[channel] = sideB1 * originalX - sideA1 * hpSideSample + deEssSidechainZ2[channel];
            deEssSidechainZ2[channel] = sideB2 * originalX - sideA2 * hpSideSample;
            float deEssEnv = std::abs((float)hpSideSample);

            // -----------------------------------------------------------------
            // 1. DSP : PLOSIVE MODULE
            // -----------------------------------------------------------------
            double highPassSample = plB0 * x + plosiveZ1[channel];
            plosiveZ1[channel] = plB1 * x - plA1 * highPassSample + plosiveZ2[channel];
            plosiveZ2[channel] = plB2 * x - plA2 * highPassSample;

            if ((knobClean > 0.001f || knobDeEss > 0.001f) && knobPlosive > 0.01f)
            {
                static float smoothedPlosiveMix[2] = { 0.0f, 0.0f };
                float targetMix = (rawEnv > 0.005f) ? knobPlosive : 0.0f;
                smoothedPlosiveMix[channel] += 0.005f * (targetMix - smoothedPlosiveMix[channel]);
                x = (highPassSample * smoothedPlosiveMix[channel]) + (x * (1.0f - smoothedPlosiveMix[channel]));
            }

            // Guard pour stocker l'état pré-dynamique (Utile pour le mode Delta)
            double preDynamicX = x;

            // -----------------------------------------------------------------
            // 2. DSP : DE-ESSER INDÉPENDANT (Géré par knobDeEss)
            // -----------------------------------------------------------------
            if (knobDeEss > 0.001f)
            {
                float targetCleanGain = 1.0f;

                // Déclenchement intelligent assoupli (20% au lieu de 32% pour attraper les sibilances discrètes)
                if (deEssEnv > 0.0002f && deEssEnv > (rawEnv * 0.20f))
                {
                    float deEssThreshold = 0.0010f; // Seuil abaissé pour plus de mordant
                    if (deEssEnv > deEssThreshold) {
                        targetCleanGain = std::pow(deEssThreshold / deEssEnv, deEssRatio);
                    }
                }

                float safeRelease = juce::jlimit(0.010f, 0.080f, advDeEssRel);
                float safeDeEssSmooth = 1.0f - std::exp(-1.0f / (safeRelease * (float)currentSampleRate));
                smoothedCleanGain[channel] += safeDeEssSmooth * (targetCleanGain - smoothedCleanGain[channel]);

                float attenDB = juce::jlimit(-20.0f, 0.0f, -20.0f * (1.0f - smoothedCleanGain[channel]) * knobDeEss);
                double Acl = std::pow(10.0, attenDB / 40.0);

                double safeDeEssFreq = juce::jlimit(4500.0f, 9500.0f, advDeEssFreq);
                double w0c = juce::MathConstants<double>::twoPi * safeDeEssFreq / currentSampleRate;
                double cosW = std::cos(w0c);
                double sinW = std::sin(w0c);

                double cb0, cb1, cb2, ca0, ca1, ca2;

                if (isBellMode) {
                    double alphaBell = sinW / (2.0 * 1.8);
                    cb0 = 1.0 + alphaBell * Acl; cb1 = -2.0 * cosW; cb2 = 1.0 - alphaBell * Acl;
                    ca0 = 1.0 + alphaBell / Acl; ca1 = -2.0 * cosW; ca2 = 1.0 - alphaBell / Acl;
                }
                else {
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

            // -----------------------------------------------------------------
            // 3. DSP : NOISE GATE INDÉPENDANT (Géré par knobClean)
            // -----------------------------------------------------------------
            if (knobClean > 0.001f)
            {
                float currentEnv = std::abs((float)x);
                float openThreshold = gateThreshold;
                float closeThreshold = gateThreshold * 0.80f;

                if (gateIsOpen[channel]) {
                    if (currentEnv < closeThreshold) gateIsOpen[channel] = false;
                }
                else {
                    if (currentEnv > openThreshold) gateIsOpen[channel] = true;
                }

                float targetGateGain = 1.0f;
                if (!gateIsOpen[channel]) {
                    float floor = juce::Decibels::decibelsToGain(advGateFloorDB);
                    float ratio = currentEnv / openThreshold;
                    targetGateGain = floor + (ratio * ratio) * (1.0f - floor);
                    targetGateGain = juce::jlimit(floor, 1.0f, targetGateGain);
                }

                float cCoef = (targetGateGain > smoothedGateGain[channel]) ? fastGateAttack : fastGateRelease;
                smoothedGateGain[channel] += cCoef * (targetGateGain - smoothedGateGain[channel]);

                x *= (double)smoothedGateGain[channel];
            }

            // === LE BLOC DELTA EST ICI : À SA PLACE LÉGITIME ===
            // Écoute exclusive de la réduction dynamique globale (Gate + De-Esser)
            if ((knobClean > 0.001f || knobDeEss > 0.001f) && isDeltaActive)
            {
                x = preDynamicX - x;
            }

            // -----------------------------------------------------------------
            // 4. DSP : ROOM ENGINE (Ignoré si Delta pour ne pas écouter le notch)
            // -----------------------------------------------------------------
            if (knobRoom > 0.001f && !isDeltaActive) {
                double notchOut = notchB0 * x + notchZ1[channel];
                notchZ1[channel] = notchB1 * x - notchA1 * notchOut + notchZ2[channel];
                notchZ2[channel] = notchB2 * x - notchA2 * notchOut;
                x = notchOut;

                double shelfOut = shelfB0 * x + shelfZ1[channel];
                shelfZ1[channel] = shelfB1 * x - shelfA1 * shelfOut + shelfZ2[channel];
                shelfZ2[channel] = shelfB2 * x - shelfA2 * shelfOut;
                x = shelfOut;
            }

            // -----------------------------------------------------------------
            // 5. DSP : DEREVERB ENGINE
            // -----------------------------------------------------------------
            if (knobReverb > 0.001f && !isDeltaActive) {
                float absX = std::abs((float)x);
                if (absX > reverbEnv[channel])
                    reverbEnv[channel] += revAttack * (absX - reverbEnv[channel]);
                else
                    reverbEnv[channel] += revRelease * (absX - reverbEnv[channel]);

                if (reverbEnv[channel] > 0.00001f) {
                    float deReverbGain = 1.0f - (knobReverb * 0.45f * (1.0f - std::tanh(reverbEnv[channel] * 10.0f)));
                    deReverbGain = juce::jlimit(0.40f, 1.0f, deReverbGain);
                    x *= (double)deReverbGain;
                }
            }

            // -----------------------------------------------------------------
            // 6. DSP : AIR ENGINE
            // -----------------------------------------------------------------
            if (knobAir > 0.001f && !isDeltaActive) {
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

                double aOut = (ab0 / aa0) * x + airShelfZ1[channel];
                airShelfZ1[channel] = (ab1 / aa0) * x - (aa1 / aa0) * aOut + airShelfZ2[channel];
                airShelfZ2[channel] = (ab2 / aa0) * x - (aa2 / aa0) * aOut;
                x = aOut;
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

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "clean", 1 }, "Clean (Gate)", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "deess", 1 }, "De-Ess", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "plosive", 1 }, "Anti-Plosive", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "room", 1 }, "Room Notch", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "reverb", 1 }, "Reverb Absorber", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "air", 1 }, "Air", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float>(-3.0f, 3.0f, 0.01f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "focus", 1 }, "Focus Intensity", 0.2f, 2.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "airFreq", 1 }, "Air Frequency", 7000.0f, 16000.0f, 11000.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "cleanDelta", 1 }, "Clean Delta", false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanDeEss", 1 }, "De-Ess Intensity Multiplier", 0.0f, 2.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanDeEssFreq", 1 }, "De-Ess Frequency", 2000.0f, 9000.0f, 5500.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ "cleanDeEssFilterMode", 1 }, "De-Ess Bell Mode", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanDeEssRelease", 1 }, "De-Ess Release", 0.005f, 0.060f, 0.015f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanGateFloor", 1 }, "Advanced Gate Floor", -45.0f, 0.0f, -24.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "cleanRelease", 1 }, "Advanced Clean Release", 0.05f, 0.60f, 0.22f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "roomNotchFreq", 1 }, "Advanced Room Freq", 150.0f, 1200.0f, 440.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "roomNotchQ", 1 }, "Advanced Room Q", 0.8f, 7.0f, 2.2f));

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InvisibleFixAudioProcessor();
}