# InvisibleFix — Vocal Repair Audio Plugin

JUCE 7+ audio plugin (VST3/AU/Standalone), Projucer → Visual Studio 2022 only. ~1000 lines, 4 source files in `Source/`.

## Build (no CLI script)
- Open `Builds/VisualStudio2022/InvisibleFix.sln` in VS2022
- Release x64 VST3 → `Builds/VisualStudio2022/x64/Release/VST3/InvisibleFix.dll`
- Edit `.jucer` in Projucer to change modules/exports, then regenerate VS projects
- `JuceLibraryCode/` is auto-generated — never edit

## DSP chain (fixed order in processBlock)
```
Plosive HPF (140 Hz) → De-Esser → Noise Gate → [Delta] → Room Notch + Shelf → Dereverb → Air Enhancer → Output Gain
```
- **Delta mode** (`cleanDelta` toggle): outputs `preDynamicX - x` (gate+de-ess difference). Skips Room, Dereverb, Air entirely.
- **Plosive** filter only activates if Clean or De-Ess knob > 0.001
- All biquads computed manually per-sample (no `juce::dsp::`), `ScopedNoDenormals` in processBlock
- Stereo only (`isBusesLayoutSupported` rejects non-stereo)

## Pre-push
Avant chaque `git push`, mets à jour `AGENTS.md` et `opencode.json` si le code ou la structure du projet a changé (nouveaux paramètres, modules, conventions, bugs).

## Known bugs
- Editor attaches `cleanPlosive` (`PluginEditor.cpp:258`) but no such param exists in `createParameterLayout()` — runtime crash

## Conventions
- All comments and UI strings in French
- `#pragma once`, `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` on every class
- State: `apvts.copyState().createXml()` / `ValueTree::fromXml()` (binary XML)

## Exact parameter IDs and ranges (from `createParameterLayout()`)
| ID | Type | Range | Default |
|---|---|---|---|
| `clean` | float | 0–1 | 0 |
| `deess` | float | 0–1 | 0 |
| `plosive` | float | 0–1 | 0 |
| `room` | float | 0–1 | 0 |
| `reverb` | float | 0–1 | 0 |
| `air` | float | 0–1 | 0 |
| `outputGain` | float | –3 – +3 dB | 0 |
| `focus` | float | 0.2–2.0 | 1.0 |
| `airFreq` | float | 7000–16000 Hz | 11000 |
| `cleanDelta` | bool | — | false |
| `cleanDeEss` | float | 0–2.0 | 1.0 |
| `cleanDeEssFreq` | float | 2000–9000 Hz | 5500 |
| `cleanDeEssFilterMode` | bool | Bell/Shelf | true (Bell) |
| `cleanDeEssRelease` | float | 0.005–0.060 s | 0.015 |
| `cleanGateFloor` | float | –45 – 0 dB | –24 |
| `cleanRelease` | float | 0.05–0.60 s | 0.22 |
| `roomNotchFreq` | float | 150–1200 Hz | 440 |
| `roomNotchQ` | float | 0.8–7.0 | 2.2 |
