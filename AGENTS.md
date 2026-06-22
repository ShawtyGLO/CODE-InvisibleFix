# InvisibleFix — Vocal Repair Audio Plugin

JUCE 7+ audio plugin (VST3/AU/Standalone), Projucer → Visual Studio 2022 only. 4 source files in `Source/`.

## Build (no CLI script)
- Open `Builds/VisualStudio2022/InvisibleFix.sln` in VS2022
- Release x64 VST3 → `Builds/VisualStudio2022/x64/Release/VST3/InvisibleFix.dll`
- Edit `.jucer` in Projucer to change modules/exports, then regenerate VS projects
- `JuceLibraryCode/` is auto-generated — never edit

## DSP chain (fixed order in processBlock)
```
Plosive HPF → Clean (Gate + NR 3-Bandes) → [cleanDelta] → Room Notch + Compensation → [roomDelta] → Dereverb → [reverbDelta] → De-Ess → [deessDelta] → Air Enhancer + Saturation → [airDelta] → Output Gain
```
- **NR 3-bandes** : LPF 250 Hz / BPF 250–4 kHz / HPF 4 kHz, crossovers LR4, expander adaptatif soft-knee par bande, noise floor tracker lent
- **Delta mode**: `cleanDelta` → `preCleanX - x` (Gate+NR). `roomDelta` → passe-bande résonant BP(fc,Q) appliqué à x (entendre la bande notchée). `reverbDelta` → `preReverbX - x` (Dereverb). `deessDelta` → `preDeEssX - x` (De-Ess). `airDelta` → `preAirX - x` (Enhancer+Sat). Each delta skips everything after its capture point. roomDelta force le module Room à s'exécuter même si bypassé.
- **Plosive** filter only activates if Clean knob > 0.001
- **Clean knob** = Gate amount + NR blend amount (no separate `noiseRedux` param)
- **Gate hysteresis** adaptatif (auto-calculated from noise floor tracker, no manual param)
- **De-esser** sidechain : HPF 4 kHz fixe (no broadband option, no sidechain param)
- All biquads computed manually per-sample (no `juce::dsp::`), `ScopedNoDenormals` in processBlock
- Stereo only (`isBusesLayoutSupported` rejects non-stereo)
- FFT buffer (4096, Hann window) for Room visualizer heatmap — capturé après le module Room (avant De-Rev)
- Bypass buttons (BYP) en façade top-left, delta buttons (Δ) en façade top-right pour chaque module
- Tous les curseurs avancés affichent leur valeur numérique (TextBoxBelow)
- **Tabs avancés** : CLEAN → Gate + NR ; DE-ESS → De-Esser + Plosive ; ROOM → Notch ; AIR → Enhancer

## Pre-push
Avant chaque `git push`, mets à jour `AGENTS.md` et `opencode.json` si le code ou la structure du projet a changé (nouveaux paramètres, modules, conventions, bugs).

## Conventions
- All comments and UI strings in French
- `#pragma once`, `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` on every class
- State: `apvts.copyState().createXml()` / `ValueTree::fromXml()` (binary XML)

## Exact parameter IDs and ranges (from `createParameterLayout()`)
| ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `clean` | float | 0–1 | 0 | Gate + NR amount |
| `deess` | float | 0–1 | 0 | De-esser amount |
| `plosive` | float | 0–1 | 0 | |
| `room` | float | 0–1 | 0 | |
| `reverb` | float | 0–1 | 0 | |
| `air` | float | 0–1 | 0 | |
| `outputGain` | float | –3 – +3 dB | 0 | |
| `cleanBypass` | bool | — | false | |
| `roomBypass` | bool | — | false | |
| `reverbBypass` | bool | — | false | |
| `deessBypass` | bool | — | false | |
| `airBypass` | bool | — | false | |
| `cleanDelta` | bool | — | false | Clean difference solo |
| `roomDelta` | bool | — | false | Room difference solo |
| `reverbDelta` | bool | — | false | Dereverb difference solo |
| `deessDelta` | bool | — | false | De-Ess difference solo |
| `airDelta` | bool | — | false | Air difference solo |
| `airFreq` | float | 7000–16000 Hz | 11000 | |
| `deEssIntensity` | float | 0–2.0 | 1.0 | Intensité (panel DE-ESS) |
| `deEssFreq` | float | 2000–9000 Hz | 5500 | |
| `deEssFilterMode` | bool | Bell/Shelf | true (Bell) | |
| `deEssRelease` | float | 0.005–0.060 s | 0.015 | |
| `deEssAttack` | float | 1–10 ms | 3 | |
| `cleanGateFloor` | float | –45 – 0 dB | –24 | Range floor |
| `cleanGateThreshold` | float | –60 – 0 dB | –40 | |
| `cleanGateAttack` | float | 0.5–10 ms | 2 | |
| `cleanGateHold` | float | 5–100 ms | 30 | |
| `cleanRelease` | float | 0.05–0.60 s | 0.22 | Gate release |
| `plosiveFreq` | float | 80–300 Hz | 140 | |
| `roomNotchFreq` | float | 150–1200 Hz | 440 | |
| `roomNotchQ` | float | 0.8–7.0 | 2.2 | Dans le tiroir ROOM |
| `airDrive` | float | 0–1 | 0 | Saturation drive |
| `nrThreshold` | float | –80 – –10 dB | –40 | Seuil expandeur |
| `nrRatio` | float | 1.0–8.0 | 3.0 | |
| `nrFloor` | float | –30 – 0 dB | –12 | Plancher |
| `nrSmooth` | float | 0.001–0.100 | 0.020 | Lissage global expandeur |
