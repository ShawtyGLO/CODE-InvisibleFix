# InvisibleFix — Vocal Repair Audio Plugin

JUCE 7+ audio plugin (VST3/AU/Standalone), Projucer → Visual Studio 2022 only. 4 source files in `Source/`.

## Build (no CLI script)
- Open `Builds/VisualStudio2022/InvisibleFix.sln` in VS2022
- Release x64 VST3 → `Builds/VisualStudio2022/x64/Release/VST3/InvisibleFix.dll`
- Edit `.jucer` in Projucer to change modules/exports, then regenerate VS projects
- `JuceLibraryCode/` is auto-generated — never edit

## DSP chain (fixed order in processBlock, vérifié 2026-07-08 contre le code)
```
Input Gain → Clean (Gate → NR 6-bandes) → [cleanDelta] → Room (notch + compensation) → [roomDelta] → De-Rev 3-bandes → [reverbDelta] → De-Ess → [deessDelta] → Air (Heat exciter → Shelf → Transient Shape) → [airDelta] → Output Gain
```
- **Il n'y a plus de filtre "Plosive HPF" séparé.** `cleanPlosiveProtect` (knob "PLOSIVE", tab CLEAN) ne filtre rien : il détecte les plosives (LP 250Hz + ratio d'enveloppes rapide/lente) et force le Gate à rester ouvert pendant le transitoire, pour éviter qu'il les coupe.
- **Gate** : RMS sidechain 200–3000Hz, hystérésis adaptative (calculée depuis le noise floor tracker, pas de réglage manuel), lookahead ~3ms (sauté en Live Mode).
- **NR 6-bandes** : expandeur adaptatif soft-knee par bande, noise floor tracker lent. `nrArtifactBlend` (tab CLEAN) mélange vers une réduction à 25% de profondeur pour limiter le pompage.
- `nrBypass` et `gateBypass` sont des bypass indépendants, imbriqués dans `cleanBypass`.
- **Room** : notch biquad unique (pas multi-notch) + shelf de compensation. La profondeur se module déjà dynamiquement via un détecteur de proéminence spectrale (`prominence = bandEnergy/wideEnergy` à la fréquence du notch, lissage asymétrique attaque rapide/release lente) — mais la fréquence reste réglée manuellement (`roomNotchFreq`), pas d'auto-détection de fréquence. `roomDepthBoost` multiplie la profondeur en dB obtenue (plancher passif 28%).
- **De-Rev 3-bandes** : implémentation réelle multi-bande (LF <500Hz / MF 500–4000Hz / HF >4000Hz), filtres Butterworth cascadés, enveloppes rapide/lente indépendantes par bande avec constantes de temps physiques (aigus décroissent plus vite que graves). Sidechain de présence vocale (bandpass 200–3000Hz) gèle la réduction 20ms sur les transitoires/attaques. `deRevThreshLF/MF/HF` = seuil sur le ratio transitoire/queue (pas un seuil de niveau dB) ; `deRevBandLF/MF/HF` = intensité de réduction par bande ; `deRevFloor` = plancher global de gain (toutes bandes). Piloté visuellement par le widget `DeRevCanvas` (drag horizontal = Thresh, drag vertical = Band, mode LINK = les 3 bandes à la fois).
- **De-Ess** : PAS un HPF fixe 4kHz (obsolète). Détection par bande étroite réglable (`deEssFreq` 2–9kHz, Q via `deEssDetectRatio` mappé 1–10) comparée à une bande de référence large → ratio spectral (doit dépasser ~2.5 pour déclencher), combiné à un seuil de niveau adaptatif (`deEssThreshold`, +6dB si la voix est naturellement sifflante, style Orban 764A). `deEssIntensity` = ratio de compression, `deEssFilterMode` choisit Bell ou Shelf pour le filtre de réduction appliqué au signal look-ahead (~2ms, sauté en Live Mode).
- **Air** : exciter harmonique "Heat" (HPF à `airFreq` → drive → saturation → injection du résidu harmonique seul) actif si `airDrive > 0`, avec 4 modes de saturation (`airSatMode`) : 0 TRIODE (tanh biaisé, harmoniques paires), 1 CLASS-A (cubique symétrique, harmoniques impaires), 2 TAPE (atan doux, quasi transparent), 3 IRON (x/√(1+x²), coude dur). `airShape` (bipolaire) est un shaper de transitoires indépendant appliqué en sortie du module.
- **Live/HQ Mode** (`liveMode`) : supprime le lookahead (et la latence reportée) du Gate et du De-Ess uniquement (~5ms cumulés). N'affecte pas NR, Room, De-Rev, Air.
- All biquads computed manually per-sample (no `juce::dsp::`), `ScopedNoDenormals` in processBlock
- Stereo only (`isBusesLayoutSupported` rejects non-stereo)
- FFT buffer (4096, Hann window) pour le heatmap Room — capturé après le module Room (avant De-Rev)
- Bypass buttons (BYP) en façade top-left, delta buttons (Δ) en façade top-right pour chaque module
- Tous les curseurs avancés affichent leur valeur numérique (TextBoxBelow)
- **Tabs avancés** (5 — pas un `juce::TabbedComponent`, un enum `AdvancedTab` fait main) : CLEAN (Gate + NR + Plosive Protect + Artifact Blend + NR/Gate bypass) ; ROOM (Notch + Depth Boost) ; DE-ESS (De-Esser, canvas de détection) ; DE-REV (canvas 3-bandes + Floor) ; AIR (Enhancer + Sat Mode + Shape)

## Problèmes connus (à corriger ou clarifier)
- **`inputGain`** : paramètre APVTS fonctionnel (-24..24dB, appliqué avant tout le reste du traitement) mais **sans contrôle UI accessible** — le `SliderAttachment` existe mais le slider est positionné hors écran (1×1px). Automatable host-only pour l'instant ; `inputLevelDisplay` (meter associé) n'est jamais lu côté UI non plus.
- **Version** : trois sources se contredisent — `PluginProcessor.h` dit V0.9, `PluginProcessor.cpp` dit V1.00, le texte UI codé en dur dit "V0.8 LAB STUDIO", et le `.jucer` n'a pas d'attribut `version` (défaut Projucer → 1.0.0). À trancher.

## Pre-push
Avant chaque `git push`, mets à jour `AGENTS.md` et `opencode.json` si le code ou la structure du projet a changé (nouveaux paramètres, modules, conventions, bugs).

## Conventions
- All comments and UI strings in French
- **Thème (couleurs/polices)** : couleurs **entièrement centralisées**. Toutes les couleurs de `PluginEditor.cpp` passent par le namespace `Theme::` (ex. `Theme::accent`, `Theme::textPrimary`, `Theme::danger`…) — **0 littéral `0x...` restant**. Point d'entrée `Source/Theme.h` ; les 46 valeurs vivent dans `Source/Generated/ThemeColors.g.h` (GÉNÉRÉ depuis `tools/theme.json` — ne pas éditer à la main). Helper `Theme::font(size, style)` en place (polices pas encore migrées). Pour changer une couleur : éditer `tools/theme.json` → régénérer le header. Nouveaux `.h` inclus par un `.cpp` existant → pas besoin de les ajouter au `.jucer`.
- **Panneaux peints pilotés par les données** : les 7 cadres `drawAdvPanel` de `paint()` (NR, GATE, DE-ESSER, NOTCH ANALYSIS, NOTCH PARAMETERS, DE-REV, AIR) lisent position+titre depuis `Source/Generated/UILayout.h` (namespace `UI::panel::`, GÉNÉRÉ depuis `tools/layout.json` zones `kind:panel`). Pour bouger/renommer un panneau : éditer `layout.json` → régénérer `UILayout.h` → rebuild. Le reste du layout (`setBounds` dans `resized()`) n'est **pas encore** migré ici (à venir).
- Outil de conception UI : `tools/plugin-factory.html` (**Shawty's Plugin Factory** : layout de toutes les fenêtres + mode Graphique pour les panneaux peints, guides d'alignement, aperçu, variantes, undo/redo) + sources de vérité `tools/layout.json` et `tools/theme.json`.
- `#pragma once`, `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` on every class
- State: `apvts.copyState().createXml()` / `ValueTree::fromXml()` (binary XML), plus `editorScale` (zoom UI, `std::atomic<float>`) stocké à part dans le state

## Exact parameter IDs and ranges (from `createParameterLayout()`, vérifié 2026-07-08)
| ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `clean` | float | 0–1 | 0 | Gate + NR amount |
| `deess` | float | 0–1 | 0 | De-esser amount |
| `room` | float | 0–1 | 0 | |
| `reverb` | float | 0–1 | 0 | "Reverb Absorber" (De-Rev) amount |
| `air` | float | 0–1 | 0 | |
| `outputGain` | float | –24 – +24 dB | 0 | |
| `inputGain` | float | –24 – +24 dB | 0 | Sans contrôle UI visible (voir Problèmes connus) |
| `cleanBypass` / `roomBypass` / `reverbBypass` / `deessBypass` / `airBypass` | bool | — | false | |
| `cleanDelta` / `roomDelta` / `reverbDelta` / `deessDelta` / `airDelta` | bool | — | false | Difference solo par module |
| `nrBypass` / `gateBypass` | bool | — | false | Bypass indépendants, imbriqués dans `cleanBypass` |
| `cleanPlosiveProtect` | float | 0–1 | 0.25 | Protection gate (pas un filtre), voir chaîne DSP |
| `airFreq` | float | 7000–16000 Hz | 11000 | |
| `airDrive` | float | 0–1 | 0 | Drive de l'exciter Heat |
| `airSatMode` | int | 0–3 | 0 | 0 Triode / 1 Class-A / 2 Tape / 3 Iron |
| `airShape` | float | –1 – +1 | 0 | Transient shaper bipolaire, indépendant du drive |
| `deEssFreq` | float | 2000–9000 Hz | 5500 | |
| `deEssThreshold` | float | –60 – –20 dB | –42 | Accessible via drag sur le canvas uniquement (slider classique caché) |
| `deEssIntensity` | float | 0–2.0 | 1.0 | Multiplicateur de ratio |
| `deEssDetectRatio` | float | 0.05–0.40 | 0.15 | "Q" — Q de la bande de détection (mappé 1–10) |
| `deEssAttack` | float | 1–10 ms | 3 | |
| `deEssRelease` | float | 0.005–0.060 s | 0.015 | |
| `deEssFilterMode` | bool | Bell/Shelf | true (Bell) | |
| `cleanGateThreshold` | float | –60 – 0 dB | –40 | |
| `cleanGateFloor` | float | –80 – 0 dB | –24 | |
| `cleanGateAttack` | float | 0.5–10 ms | 2 | |
| `cleanGateHold` | float | 5–100 ms | 30 | |
| `cleanRelease` | float | 0.05–0.60 s | 0.22 | Gate release |
| `roomNotchFreq` | float | 150–1200 Hz | 440 | |
| `roomNotchQ` | float | 0.8–7.0 | 2.2 | |
| `roomDepthBoost` | float | 0.5–2.5 | 1.0 | Multiplie la profondeur auto-détectée (voir chaîne DSP) |
| `deRevFloor` | float | 0–0.65 | 0.35 | Plancher global de gain (toutes bandes) |
| `deRevBandLF` / `deRevBandMF` / `deRevBandHF` | float | 0–1 | 0.35 / 0.75 / 1.0 | Intensité de réduction par bande |
| `deRevThreshLF` / `deRevThreshMF` / `deRevThreshHF` | float | 0–1 | 0.95 / 0.80 / 0.65 | Seuil ratio transitoire/queue (pas un niveau dB) |
| `nrThreshold` | float | –80 – –10 dB | –40 | Seuil expandeur |
| `nrRatio` | float | 1.0–8.0 | 3.0 | |
| `nrFloor` | float | –80 – 0 dB | –12 | Plancher |
| `nrSmooth` | float | 0.001–0.100 | 0.020 | Lissage global expandeur |
| `nrArtifactBlend` | float | 0–1 | 0 | Mélange vers réduction douce (anti-pompage) |
| `liveMode` | bool | Live/HQ | false (HQ) | Retire ~5ms de lookahead cumulé (Gate+De-Ess) |
