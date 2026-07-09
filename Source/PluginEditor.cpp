#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Theme.h"
#include "Generated/UILayout.h"

// =============================================================================
//  ARC FIN — LookAndFeel pour knobs (grand façade 94px + petit avancé 50px)
//  Arc 270° de 135° à 45° (sens horaire depuis 12h), dégradé cyan→teal
// =============================================================================
class ArcFinLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float startAngle, float endAngle, juce::Slider&) override
    {
        bool   isLarge   = (width >= 70);
        float  arcThick  = isLarge ? 5.5f : 4.0f;
        float  inset     = isLarge ? 8.0f : 5.0f;

        float  cx  = (float)x + (float)width  * 0.5f;
        float  cy  = (float)y + (float)height * 0.5f;
        float  outerR = (float)juce::jmin(width, height) * 0.5f;
        float  arcR   = outerR - arcThick * 0.5f;

        float  valAngle = startAngle + sliderPos * (endAngle - startAngle);

        // ── 1. Track arc (gris clair, plein 270°) ──────────────────────────
        juce::Path track;
        track.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour(Theme::textPrimary);
        g.strokePath(track, juce::PathStrokeType(arcThick,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ── 2. Arc rempli (gradient cyan→teal jusqu'à la valeur) ────────────
        if (sliderPos > 0.005f)
        {
            juce::Path filled;
            filled.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, valAngle, true);
            juce::ColourGradient grad(
                Theme::accent, cx - arcR, cy,
                Theme::teal, cx + arcR, cy + arcR,
                false);
            g.setGradientFill(grad);
            g.strokePath(filled, juce::PathStrokeType(arcThick,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // ── 3. Cercle central (fond verre + ring) ───────────────────────────
        float inR = outerR - inset;
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.fillEllipse(cx - inR, cy - inR, inR * 2.0f, inR * 2.0f);
        g.setColour(Theme::accentAlt.withAlpha(0.20f));
        g.drawEllipse(cx - inR + 0.75f, cy - inR + 0.75f,
                      (inR - 0.75f) * 2.0f, (inR - 0.75f) * 2.0f, 1.0f);

        // ── 4. Aiguille (rectangle arrondi gradient cyan→teal) ─────────────
        float needleW  = isLarge ? 3.0f  : 2.5f;
        float needleH  = isLarge ? 10.0f : 8.0f;
        float needleTopY = isLarge ? 6.0f  : 4.0f;  // depuis le haut du cercle interne

        // Dessin avant rotation : aiguille pointant vers 12h (y négatif = haut)
        float needleTopFromCenter = -(inR - needleTopY);
        juce::Path needle;
        needle.addRoundedRectangle(-needleW * 0.5f, needleTopFromCenter,
                                    needleW, needleH, 1.0f);
        needle.applyTransform(
            juce::AffineTransform::rotation(valAngle).translated(cx, cy));

        juce::ColourGradient ng(
            Theme::accent, cx, cy - inR,
            Theme::teal, cx, cy - inR + needleH,
            false);
        g.setGradientFill(ng);
        g.fillPath(needle);
    }

    juce::Font getLabelFont(juce::Label& lbl) override
    {
        // Valeur numérique dans la textbox → léger
        return juce::Font(juce::FontOptions(lbl.getHeight() > 12 ? 13.0f : 10.0f));
    }
};

static ArcFinLookAndFeel arcFinLAF;

// =============================================================================
//  BYPASS BUTTON — clignote rouge quand actif, sinon muted gris
// =============================================================================
class BypassButtonLAF : public juce::LookAndFeel_V4
{
public:
    bool blinkOn = true;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool, bool) override {}

    void drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        if (btn.getToggleState())
        {
            auto col = blinkOn ? Theme::danger
                               : Theme::danger.withAlpha(0.20f);
            g.setColour(col);
        }
        else
        {
            g.setColour(Theme::textMuted);
        }
        g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
        g.drawText(btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred);
    }
};
static BypassButtonLAF bypassBtnLAF;

// =============================================================================
//  DELTA BUTTON — triangle plus grand, transparent, cyan clignotant quand actif
// =============================================================================
class DeltaButtonLAF : public juce::LookAndFeel_V4
{
public:
    bool blinkOn = true;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool, bool) override {}

    void drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        if (btn.getToggleState())
        {
            auto col = blinkOn ? Theme::accent
                               : Theme::accent.withAlpha(0.20f);
            g.setColour(col);
        }
        else
        {
            g.setColour(Theme::textMuted);
        }
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred);
    }
};
static DeltaButtonLAF deltaButtonLAF;

// =============================================================================
//  FLAT BUTTON — générique (mode De-Ess), fond uniquement si actif
// =============================================================================
class FlatButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
                               const juce::Colour&, bool, bool) override
    {
        if (b.getToggleState())
        {
            g.setColour(b.findColour(juce::TextButton::buttonOnColourId));
            g.fillRoundedRectangle(b.getLocalBounds().toFloat().reduced(1.0f), 3.0f);
        }
    }
};
static FlatButtonLookAndFeel flatButtonLAF;

// =============================================================================
//  AIR FREQ SLIDER — piste slim + thumb cercle teal
// =============================================================================
class AirFreqSliderLAF : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float /*minPos*/, float /*maxPos*/,
        const juce::Slider::SliderStyle, juce::Slider&) override
    {
        float cy  = (float)y + (float)height * 0.5f;
        float trackH = 4.0f;

        // Piste inactive grise
        g.setColour(Theme::textPrimary);
        g.fillRoundedRectangle((float)x, cy - trackH * 0.5f,
                                (float)width, trackH, 2.0f);

        // Remplissage gauche cyan
        float fillW = sliderPos - (float)x;
        if (fillW > 0.0f) {
            juce::ColourGradient grad(Theme::accent.withAlpha(0.50f), (float)x, cy,
                                      Theme::accent, sliderPos, cy, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle((float)x, cy - trackH * 0.5f, fillW, trackH, 2.0f);
        }

        // Thumb cercle blanc avec ring teal
        float r = 8.0f;
        g.setColour(juce::Colours::white);
        g.fillEllipse(sliderPos - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(Theme::accentAlt);
        g.drawEllipse(sliderPos - r + 1.0f, cy - r + 1.0f,
                      (r - 1.0f) * 2.0f, (r - 1.0f) * 2.0f, 1.5f);
    }
};
static AirFreqSliderLAF airFreqLAF;

// =============================================================================
//  COMPOSANT : DualRailComponent — double thumb vertical + barre VU live
// =============================================================================
class DualRailComponent : public juce::Component
{
public:
    DualRailComponent() = default;
    juce::RangedAudioParameter* upperParam = nullptr;
    juce::RangedAudioParameter* lowerParam = nullptr;
    float vuLevel = 0.0f;
    float dbMin   = -60.0f;
    float dbMax   =   0.0f;
    juce::String upperLabel = "THRESH";
    juce::String lowerLabel = "FLOOR";
    // Auto-threshold display
    float signalLevelDB  = -80.0f;
    float autoZoneLowDB  = -80.0f;
    float autoZoneHighDB = -80.0f;
    bool  autoZoneValid  = false;

    void paint(juce::Graphics& g) override
    {
        float W = (float)getWidth(), H = (float)getHeight();
        float railX = 28.0f, railW = 7.0f;
        float vuX   = railX + railW + 5.0f, vuW = 8.0f;
        float railTop = 8.0f, railBot = H - 22.0f;
        float railH = railBot - railTop;

        // Convertir valeurs normalisées → dB réels → position Y dans [dbMin, dbMax]
        auto dbToY = [&](float db) {
            return railTop + (1.0f - (db - dbMin) / (dbMax - dbMin)) * railH;
        };
        float upperDB = upperParam ? upperParam->convertFrom0to1(upperParam->getValue())
                                   : dbMin + (dbMax - dbMin) * 0.65f;
        float lowerDB = lowerParam ? lowerParam->convertFrom0to1(lowerParam->getValue())
                                   : dbMin + (dbMax - dbMin) * 0.30f;
        float upperY = dbToY(juce::jlimit(dbMin, dbMax, upperDB));
        float lowerY = dbToY(juce::jlimit(dbMin, dbMax, lowerDB));

        // dB labels (axe Y) — calibrés sur dbMin/dbMax
        g.setFont(juce::Font(juce::FontOptions(7.5f)));
        g.setColour(Theme::textMuted);
        for (int di = 0; di < 5; ++di) {
            float dbVal = dbMax - (float)di * (dbMax - dbMin) / 4.0f;
            float yp = railTop + (float)di * railH / 4.0f;
            g.drawText(juce::String((int)dbVal), 0, (int)(yp - 5), 26, 10,
                       juce::Justification::centredRight);
        }

        // Rail background
        g.setColour(Theme::surfaceLight);
        g.fillRoundedRectangle(railX, railTop, railW, railH, 3.5f);

        // Bande auto-zone (à gauche du rail, bande teal lisible)
        if (autoZoneValid)
        {
            float zoneTopY = dbToY(juce::jlimit(dbMin, dbMax, autoZoneHighDB));
            float zoneBotY = dbToY(juce::jlimit(dbMin, dbMax, autoZoneLowDB));
            float bx = railX - 12.0f;
            float bw = 8.0f;
            // Fond
            g.setColour(Theme::teal.withAlpha(0.25f));
            g.fillRoundedRectangle(bx, zoneTopY, bw, zoneBotY - zoneTopY, 2.5f);
            // Bords lumineux haut et bas
            g.setColour(Theme::teal.withAlpha(0.85f));
            g.fillRoundedRectangle(bx, zoneTopY, bw, 2.0f, 1.0f);
            g.fillRoundedRectangle(bx, zoneBotY - 2.0f, bw, 2.0f, 1.0f);
            // Étiquette dB de la zone optimale (centre de la vallée)
            float centerY = (zoneTopY + zoneBotY) * 0.5f;
            float centerDB = (autoZoneHighDB + autoZoneLowDB) * 0.5f;
            g.setFont(juce::Font(juce::FontOptions(6.5f).withStyle("Bold")));
            g.setColour(Theme::teal);
            g.drawText(juce::String((int)centerDB) + "dB",
                       (int)(bx - 18), (int)(centerY - 5), 18, 10,
                       juce::Justification::centredRight);
        }

        // Zone active (cyan entre les deux thumbs, quel que soit leur ordre)
        {
            float zoneTop = juce::jmin(upperY, lowerY);
            float zoneBot = juce::jmax(upperY, lowerY);
            if (zoneBot > zoneTop + 0.5f) {
                g.setColour(Theme::accent.withAlpha(0.18f));
                g.fillRoundedRectangle(railX, zoneTop, railW, zoneBot - zoneTop, 2.0f);
            }
        }

        // VU bar background
        g.setColour(Theme::surfaceLight);
        g.fillRoundedRectangle(vuX, railTop, vuW, railH, 3.5f);

        // VU bar fill — vuLevel est 0-1 pour -60 dB à 0 dB ; on recalibre sur [dbMin, dbMax]
        {
            float vuDB   = -60.0f + vuLevel * 60.0f;
            float vuFrac = juce::jlimit(0.0f, 1.0f, (vuDB - dbMin) / (dbMax - dbMin));
            float vuBarH = vuFrac * railH;
            if (vuBarH > 0.5f) {
                juce::ColourGradient vuGrad(
                    Theme::teal, vuX, railBot,
                    Theme::accent, vuX, railBot - vuBarH, false);
                g.setGradientFill(vuGrad);
                g.fillRoundedRectangle(vuX, railBot - vuBarH, vuW, vuBarH, 3.5f);
            }
        }

        // Lignes de référence sur VU
        g.setColour(Theme::accent.withAlpha(0.35f));
        g.fillRect(vuX, upperY, vuW, 1.5f);
        g.setColour(Theme::teal.withAlpha(0.25f));
        g.fillRect(vuX, lowerY, vuW, 1.5f);

        // Ligne de niveau signal temps réel (trait blanc épais + halo + valeur dB)
        if (signalLevelDB > dbMin - 1.0f)
        {
            float sigY  = dbToY(juce::jlimit(dbMin, dbMax, signalLevelDB));
            float lineX = vuX - 3.0f;
            float lineW = vuW + 6.0f;
            // Halo discret
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.fillRect(lineX, sigY - 3.0f, lineW, 6.0f);
            // Trait principal
            g.setColour(juce::Colours::white.withAlpha(0.92f));
            g.fillRect(lineX, sigY - 1.5f, lineW, 3.0f);
            // Valeur dB à droite
            float labelX2 = vuX + vuW + 4.0f;
            float labelW2 = W - labelX2;
            g.setFont(juce::Font(juce::FontOptions(7.0f).withStyle("Bold")));
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.drawText(juce::String((int)signalLevelDB) + "dB",
                       (int)labelX2, (int)(sigY - 5), (int)juce::jmax(1.0f, labelW2 - 2.0f), 10,
                       juce::Justification::centredLeft);
        }

        // Thumb upper (threshold)
        float tW = railW + 10.0f, tH = 7.0f, tX = railX - 5.0f;
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(tX, upperY - tH * 0.5f, tW, tH, 2.5f);
        g.setColour(Theme::accent);
        g.drawRoundedRectangle(tX, upperY - tH * 0.5f, tW, tH, 2.5f, 1.5f);

        // Thumb lower (floor)
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(tX, lowerY - tH * 0.5f, tW, tH, 2.5f);
        g.setColour(Theme::teal);
        g.drawRoundedRectangle(tX, lowerY - tH * 0.5f, tW, tH, 2.5f, 1.5f);

        // Valeurs numériques dB à droite des thumbs
        float labelX = vuX + vuW + 6.0f;
        float labelW = W - labelX;
        g.setFont(juce::Font(juce::FontOptions(8.0f)));

        if (upperParam) {
            g.setColour(Theme::accent);
            g.drawText(juce::String(upperDB, 1) + " dB",
                       (int)labelX, (int)(upperY - 5), (int)labelW, 10,
                       juce::Justification::centredLeft);
        }
        if (lowerParam) {
            g.setColour(Theme::textMuted);
            g.drawText(juce::String(lowerDB, 1) + " dB",
                       (int)labelX, (int)(lowerY - 5), (int)labelW, 10,
                       juce::Justification::centredLeft);
        }

        // Labels sous le composant
        g.setFont(juce::Font(juce::FontOptions(7.0f).withStyle("Bold")));
        g.setColour(Theme::accent);
        g.drawText(upperLabel, (int)railX - 6, (int)(H - 18), 60, 10,
                   juce::Justification::centredLeft);
        g.setColour(Theme::textMuted);
        g.drawText(lowerLabel, (int)railX - 6, (int)(H - 9), 60, 10,
                   juce::Justification::centredLeft);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        float railTop = 8.0f, railH = (float)getHeight() - 30.0f;
        auto dbToY = [&](float db) {
            return railTop + (1.0f - (db - dbMin) / (dbMax - dbMin)) * railH;
        };
        float uDB = upperParam ? upperParam->convertFrom0to1(upperParam->getValue())
                               : dbMin + (dbMax - dbMin) * 0.65f;
        float lDB = lowerParam ? lowerParam->convertFrom0to1(lowerParam->getValue())
                               : dbMin + (dbMax - dbMin) * 0.30f;
        draggingUpper = (std::fabs((float)e.y - dbToY(uDB)) <= std::fabs((float)e.y - dbToY(lDB)));
        if (draggingUpper && upperParam) upperParam->beginChangeGesture();
        if (!draggingUpper && lowerParam) lowerParam->beginChangeGesture();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        float railTop = 8.0f, railH = (float)getHeight() - 30.0f;
        float yNorm = juce::jlimit(0.0f, 1.0f, ((float)e.y - railTop) / railH);
        float newDB = dbMax - yNorm * (dbMax - dbMin);

        // Convertit dB → normalisé pour un paramètre à plage linéaire
        auto toNorm = [](juce::RangedAudioParameter* p, float db) -> float {
            if (!p) return 0.5f;
            auto& r = p->getNormalisableRange();
            return juce::jlimit(0.0f, 1.0f, (db - r.start) / (r.end - r.start));
        };

        // Chaque thumb bouge librement dans sa propre plage de paramètre
        if (draggingUpper && upperParam)
            upperParam->setValueNotifyingHost(toNorm(upperParam, newDB));
        else if (lowerParam)
            lowerParam->setValueNotifyingHost(toNorm(lowerParam, newDB));
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (draggingUpper && upperParam) upperParam->endChangeGesture();
        if (!draggingUpper && lowerParam) lowerParam->endChangeGesture();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        float railTop = 8.0f, railH = (float)getHeight() - 30.0f;
        auto dbToY = [&](float db) {
            return railTop + (1.0f - (db - dbMin) / (dbMax - dbMin)) * railH;
        };
        float uDB = upperParam ? upperParam->convertFrom0to1(upperParam->getValue())
                               : dbMin + (dbMax - dbMin) * 0.65f;
        float lDB = lowerParam ? lowerParam->convertFrom0to1(lowerParam->getValue())
                               : dbMin + (dbMax - dbMin) * 0.30f;
        bool hitUpper = (std::fabs((float)e.y - dbToY(uDB)) <= std::fabs((float)e.y - dbToY(lDB)));
        if (hitUpper && upperParam) {
            upperParam->beginChangeGesture();
            upperParam->setValueNotifyingHost(upperParam->getDefaultValue());
            upperParam->endChangeGesture();
        } else if (lowerParam) {
            lowerParam->beginChangeGesture();
            lowerParam->setValueNotifyingHost(lowerParam->getDefaultValue());
            lowerParam->endChangeGesture();
        }
        repaint();
    }

    juce::MouseCursor getMouseCursor() override
    {
        return juce::MouseCursor::UpDownResizeCursor;
    }

private:
    bool draggingUpper = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DualRailComponent)
};

// =============================================================================
//  COMPOSANT : AHREnvelopeEditor — forme glissable Attack / Hold / Release
//              (holdParam peut être nullptr → mode AR à 2 segments)
// =============================================================================
class AHREnvelopeEditor : public juce::Component
{
public:
    AHREnvelopeEditor() = default;
    juce::RangedAudioParameter* atkParam  = nullptr;
    juce::RangedAudioParameter* holdParam = nullptr;
    juce::RangedAudioParameter* relParam  = nullptr;

    void paint(juce::Graphics& g) override
    {
        float W = (float)getWidth(), H = (float)getHeight() - 26.0f;
        bool hasHold = (holdParam != nullptr);

        // Largeurs visuelles : sqrt(ms réels) → transitions douces même aux extrêmes
        float atkMs  = atkParam  ? atkParam->convertFrom0to1(atkParam->getValue()) * 1000.0f  : 2.0f;
        float holdMs = hasHold   ? holdParam->convertFrom0to1(holdParam->getValue()) * 1000.0f : 30.0f;
        float relMs  = relParam  ? relParam->convertFrom0to1(relParam->getValue()) * 1000.0f  : 220.0f;

        float atkSq  = std::sqrt(juce::jmax(0.0f, atkMs));
        float holdSq = hasHold ? std::sqrt(juce::jmax(0.0f, holdMs)) : 0.0f;
        float relSq  = std::sqrt(juce::jmax(0.0f, relMs));
        float total  = atkSq + holdSq + relSq;
        if (total < 0.01f) total = 0.01f;

        float atkW  = (atkSq  / total) * (W - 12.0f);
        float holdW = (holdSq / total) * (W - 12.0f);
        float relW  = (relSq  / total) * (W - 12.0f);

        float x0 = 6.0f, x1 = x0 + atkW;
        float x2 = x1 + holdW, x3 = x2 + relW;
        float yBot = H - 4.0f, yTop = H * 0.12f;

        // Canvas background
        g.setColour(Theme::surfaceLight.withAlpha(0.60f));
        g.fillRoundedRectangle(0, 0, W, H, 5.0f);
        g.setColour(Theme::textPrimary.withAlpha(0.40f));
        g.drawRoundedRectangle(0.5f, 0.5f, W - 1, H - 1, 5.0f, 0.5f);

        // Baseline
        g.setColour(Theme::textMuted.withAlpha(0.20f));
        g.fillRect(x0, yBot, x3 - x0, 1.0f);

        // Forme enveloppe
        juce::Path env;
        env.startNewSubPath(x0, yBot);
        env.lineTo(x1, yTop);
        if (hasHold) env.lineTo(x2, yTop);
        env.lineTo(x3, yBot);

        juce::Path envFill = env;
        envFill.lineTo(x3, yBot); envFill.lineTo(x0, yBot); envFill.closeSubPath();
        g.setColour(Theme::accent.withAlpha(0.10f));
        g.fillPath(envFill);

        g.setColour(Theme::accent.withAlpha(0.90f));
        g.strokePath(env, juce::PathStrokeType(1.8f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Points de contrôle
        auto drawCtrl = [&](float cx, float cy, juce::Colour col) {
            g.setColour(juce::Colours::white);
            g.fillEllipse(cx - 4.5f, cy - 4.5f, 9.0f, 9.0f);
            g.setColour(col);
            g.drawEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f, 1.5f);
        };
        drawCtrl(x1, yTop, Theme::accent);
        if (hasHold) drawCtrl(x2, yTop, Theme::accent);
        drawCtrl(x3, yBot, Theme::teal);

        // Flèches ↕ indiquant la direction de drag
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(Theme::accent.withAlpha(0.55f));
        g.drawText(juce::String::fromUTF8("\xE2\x86\x95"), (int)(x1 - 6), (int)(yTop - 14), 12, 12, juce::Justification::centred);
        if (hasHold) g.drawText(juce::String::fromUTF8("\xE2\x86\x95"), (int)(x2 - 6), (int)(yTop - 14), 12, 12, juce::Justification::centred);
        g.setColour(Theme::teal.withAlpha(0.55f));
        g.drawText(juce::String::fromUTF8("\xE2\x86\x95"), (int)(x3 - 6), (int)(yBot - 12), 12, 12, juce::Justification::centred);

        // Labels segments + valeurs ms — positions fixes (toujours visibles même si segment=0)
        float labY = H + 2.0f;
        float valY = H + 13.0f;
        int nSegs = hasHold ? 3 : 2;
        float slotW = (W - 12.0f) / (float)nSegs;

        auto drawSegFixed = [&](const char* name, juce::RangedAudioParameter* p, int idx, juce::Colour col) {
            float midX = 6.0f + ((float)idx + 0.5f) * slotW;
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(Theme::textMuted);
            g.drawText(name, (int)(midX - 22), (int)labY, 44, 10, juce::Justification::centred);
            if (p) {
                // Les paramètres gate/deess sont stockés en secondes → convertir en ms
                float ms = p->convertFrom0to1(p->getValue()) * 1000.0f;
                juce::String valStr = (ms < 10.0f)
                    ? juce::String(ms, 1) + " ms"
                    : juce::String((int)ms) + " ms";
                g.setFont(juce::Font(juce::FontOptions(7.5f)));
                g.setColour(col);
                g.drawText(valStr, (int)(midX - 22), (int)valY, 44, 10, juce::Justification::centred);
            }
        };
        drawSegFixed("ATK",  atkParam,  0, Theme::accent);
        if (hasHold) drawSegFixed("HOLD", holdParam, 1, Theme::accent);
        drawSegFixed("REL",  relParam,  hasHold ? 2 : 1, Theme::teal);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        float W = (float)getWidth();
        bool hasHold = (holdParam != nullptr);
        int nSegs = hasHold ? 3 : 2;
        // Zones fixes égales — indépendantes de la valeur des paramètres
        float slotW = (W - 12.0f) / (float)nSegs;
        float ex = (float)e.x - 6.0f;

        if (!hasHold || ex < slotW) {
            dragSeg = 0; if (atkParam)  atkParam->beginChangeGesture();
        } else if (ex < 2.0f * slotW) {
            dragSeg = 1; if (holdParam) holdParam->beginChangeGesture();
        } else {
            dragSeg = 2; if (relParam)  relParam->beginChangeGesture();
        }

        dragStartY   = e.y;
        dragStartVal = (dragSeg == 0 && atkParam)  ? atkParam->getValue()  :
                       (dragSeg == 1 && holdParam) ? holdParam->getValue() :
                       (relParam ? relParam->getValue() : 0.5f);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        // Utiliser toute la hauteur du composant pour le drag (canvas + zone labels)
        float H = juce::jmax(1.0f, (float)getHeight());
        float delta = -(float)(e.y - dragStartY) / H;
        float newN = juce::jlimit(0.01f, 1.0f, dragStartVal + delta);
        if      (dragSeg == 0 && atkParam)  atkParam->setValueNotifyingHost(newN);
        else if (dragSeg == 1 && holdParam) holdParam->setValueNotifyingHost(newN);
        else if (relParam)                  relParam->setValueNotifyingHost(newN);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if      (dragSeg == 0 && atkParam)  atkParam->endChangeGesture();
        else if (dragSeg == 1 && holdParam) holdParam->endChangeGesture();
        else if (relParam)                  relParam->endChangeGesture();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        float W = (float)getWidth();
        bool hasHold = (holdParam != nullptr);
        int nSegs = hasHold ? 3 : 2;
        float slotW = (W - 12.0f) / (float)nSegs;
        float ex = (float)e.x - 6.0f;

        auto reset = [](juce::RangedAudioParameter* p) {
            if (!p) return;
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->getDefaultValue());
            p->endChangeGesture();
        };
        if (!hasHold || ex < slotW)       reset(atkParam);
        else if (ex < 2.0f * slotW)       reset(holdParam);
        else                               reset(relParam);
        repaint();
    }

    juce::MouseCursor getMouseCursor() override
    {
        return juce::MouseCursor::UpDownResizeCursor;
    }

private:
    int   dragSeg = 0;
    int   dragStartY = 0;
    float dragStartVal = 0.5f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AHREnvelopeEditor)
};

// =============================================================================
//  COMPOSANT : GateWaveformDisplay — waveform RMS défilante + barre GR
//              Remplace le DualRail dans le panneau Gate Advanced
// =============================================================================
class GateWaveformDisplay : public juce::Component
{
public:
    static constexpr int HIST = 240; // ~8s à 30 Hz

    GateWaveformDisplay()
    {
        std::fill(rmsDB,   rmsDB   + HIST, -80.0f);
        std::fill(isOpen,  isOpen  + HIST, false);
        std::fill(grHist,  grHist  + HIST, 0.0f);
    }

    // Paramètres liés (pointeurs bruts, pas d'ownership)
    juce::RangedAudioParameter* threshParam = nullptr;
    juce::RangedAudioParameter* floorParam  = nullptr;

    // Mise à jour depuis le timerCallback de l'éditeur (thread UI, 30 Hz)
    void pushSample(float levelDB, bool open, float grNorm)
    {
        writePos = (writePos + 1) % HIST;
        rmsDB [writePos] = levelDB;
        isOpen[writePos] = open;
        grHist[writePos] = grNorm;
        grSmooth = grSmooth * 0.88f + grNorm * 0.12f;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const float W = (float)getWidth(), H = (float)getHeight();
        const float GR_H     = 7.0f;
        const float WAVE_TOP = GR_H + 2.0f;
        const float WAVE_H   = H - WAVE_TOP - 10.0f; // 10px = labels bas

        // ── Fond ──────────────────────────────────────────────────────────
        g.setColour(Theme::surfaceMint3.withAlpha(0.55f));
        g.fillRoundedRectangle(0, 0, W, H, 4.0f);

        // ── Barre GR (haut, 7px) ──────────────────────────────────────────
        g.setColour(Theme::surfaceMuted.withAlpha(0.55f));
        g.fillRoundedRectangle(0, 0, W, GR_H, 2.0f);
        if (grSmooth > 0.02f)
        {
            g.setColour(Theme::curve.withAlpha(0.60f));
            g.fillRoundedRectangle(0, 0, grSmooth * W, GR_H, 2.0f);
            g.setFont(juce::Font(juce::FontOptions(6.0f).withStyle("Bold")));
            g.setColour(Theme::curveBright);
            g.drawText("GR \xe2\x88\x92" + juce::String((int)(grSmooth * 40)) + " dB",
                       3, 0, (int)(grSmooth * W + 2), (int)GR_H, juce::Justification::centredLeft);
        }
        // séparateur
        g.setColour(Theme::textMuted.withAlpha(0.18f));
        g.fillRect(0.0f, GR_H, W, 1.0f);

        // ── dB range, helpers ─────────────────────────────────────────────
        const float DB_MIN = -60.0f, DB_MAX = 0.0f;
        auto dbToY = [&](float db) -> float {
            float n = (db - DB_MIN) / (DB_MAX - DB_MIN);
            return WAVE_TOP + (1.0f - juce::jlimit(0.0f, 1.0f, n)) * WAVE_H;
        };

        float thDB = threshParam
            ? threshParam->convertFrom0to1(threshParam->getValue()) : -26.0f;
        float flDB = floorParam
            ? floorParam->convertFrom0to1(floorParam->getValue())  : -40.0f;

        float thY = dbToY(thDB);
        float flY = dbToY(flDB);

        // ── Floor (pointillés subtils) ────────────────────────────────────
        g.setColour(Theme::textMuted.withAlpha(0.22f));
        for (float px = 2.0f; px < W; px += 7.0f)
            g.fillRect(px, flY, 4.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(Theme::textMuted.withAlpha(0.45f));
        g.drawText("F " + juce::String((int)flDB) + "dB",
                   3, (int)(flY + 2), 42, 8, juce::Justification::centredLeft);

        // ── Waveform (barres verticales par pixel) ────────────────────────
        int nPx = (int)W;
        for (int px = 0; px < nPx; ++px)
        {
            int age = nPx - 1 - px;
            int idx = (writePos - age % HIST + HIST) % HIST;
            float db  = rmsDB [idx];
            bool  op  = isOpen[idx];
            if (db < DB_MIN - 1.0f) continue;

            float y    = dbToY(juce::jlimit(DB_MIN, DB_MAX, db));
            float barH = WAVE_TOP + WAVE_H - y;
            if (barH < 1.0f) barH = 1.0f;

            // Corps de la barre
            g.setColour(op ? Theme::teal.withAlpha(0.50f)
                           : Theme::textMuted.withAlpha(0.18f));
            g.fillRect((float)px, y, 1.0f, barH);
            // Bord lumineux au sommet
            g.setColour(op ? Theme::teal.withAlpha(0.90f)
                           : Theme::textMuted.withAlpha(0.40f));
            g.fillRect((float)px, y, 1.0f, 1.5f);
        }

        // ── Ligne threshold (draggable, pointillés cyan) ──────────────────
        // Zone drag visible
        g.setColour(Theme::accent.withAlpha(0.07f));
        g.fillRect(0.0f, thY - 5.0f, W, 10.0f);
        // Pointillés
        g.setColour(Theme::accent.withAlpha(0.90f));
        for (float px = 0.0f; px < W; px += 8.0f)
            g.fillRect(px, thY, 5.0f, 1.5f);
        // Label valeur
        g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
        g.setColour(Theme::accent);
        g.drawText(juce::String((int)thDB) + " dB", 3, (int)(thY - 10), 36, 9,
                   juce::Justification::centredLeft);

        // ── Ligne RMS moyenne (3s) quand AUTO actif ────────────────────────
        if (showAutoLine)
        {
            float sum = 0.0f; int cnt = 0;
            for (int i = 0; i < juce::jmin(HIST, 90); ++i) {
                int idx = (writePos - i + HIST) % HIST;
                if (rmsDB[idx] > DB_MIN) { sum += rmsDB[idx]; cnt++; }
            }
            if (cnt > 0) {
                float avgDB = sum / (float)cnt;
                float avgY  = dbToY(juce::jlimit(DB_MIN, DB_MAX, avgDB));
                g.setColour(Theme::curve.withAlpha(0.80f));
                for (float px = 4.0f; px < W; px += 7.0f)
                    g.fillRect(px, avgY, 4.0f, 1.5f);
                g.setFont(juce::Font(juce::FontOptions(7.0f).withStyle("Bold")));
                g.setColour(Theme::curve);
                g.drawText("~" + juce::String((int)avgDB) + " dB",
                           3, (int)(avgY + 2), 48, 9, juce::Justification::centredLeft);
            }
        }

        // ── Indicateur OPEN / CLOSED ──────────────────────────────────────
        bool latestOpen = isOpen[writePos];
        g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
        g.setColour(latestOpen ? Theme::teal
                               : Theme::textMuted.withAlpha(0.50f));
        g.drawText(latestOpen ? "\xe2\x97\x8f OPEN" : "\xe2\x97\x8b CLOSED",
                   (int)(W - 50), (int)(WAVE_TOP + 2), 48, 10,
                   juce::Justification::centredRight);

        // ── Échelle de temps ──────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(Theme::textMuted.withAlpha(0.35f));
        g.drawText("8s",  3,           (int)(H - 9), 16, 8, juce::Justification::centredLeft);
        g.drawText("now", (int)(W-20), (int)(H - 9), 20, 8, juce::Justification::centredRight);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (threshParam) threshParam->beginChangeGesture();
        applyThreshDrag(e.y);
    }
    void mouseDrag(const juce::MouseEvent& e) override { applyThreshDrag(e.y); }
    void mouseUp  (const juce::MouseEvent&) override
    {
        if (threshParam) threshParam->endChangeGesture();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (!threshParam) return;
        threshParam->beginChangeGesture();
        threshParam->setValueNotifyingHost(threshParam->getDefaultValue());
        threshParam->endChangeGesture();
        repaint();
    }
    juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::UpDownResizeCursor; }

    bool showAutoLine = false;

private:
    void applyThreshDrag(int mouseY)
    {
        if (!threshParam) return;
        const float GR_H = 7.0f, WAVE_TOP = 9.0f;
        float waveH = (float)getHeight() - WAVE_TOP - 10.0f;
        float norm  = 1.0f - juce::jlimit(0.0f, 1.0f, ((float)mouseY - WAVE_TOP) / waveH);
        float db    = -60.0f + norm * 60.0f;
        auto& r     = threshParam->getNormalisableRange();
        threshParam->setValueNotifyingHost(
            juce::jlimit(0.0f, 1.0f, (db - r.start) / (r.end - r.start)));
        repaint();
    }

    float rmsDB [HIST]{};
    bool  isOpen[HIST]{};
    float grHist[HIST]{};
    int   writePos = 0;
    float grSmooth = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GateWaveformDisplay)
};

// =============================================================================
//  COMPOSANT : AHRTrizone — rectangle arrondi 3 zones (A|H|R) ou 2 zones (A|R)
//              Courbes live, drag horizontal, valeur ms, double-clic reset
// =============================================================================
class AHRTrizone : public juce::Component
{
public:
    AHRTrizone() = default;
    juce::RangedAudioParameter* atkParam  = nullptr;
    juce::RangedAudioParameter* holdParam = nullptr; // nullptr → mode 2 zones (A/R)
    juce::RangedAudioParameter* relParam  = nullptr;

    void paint(juce::Graphics& g) override
    {
        const float W  = (float)getWidth();
        const float H  = (float)getHeight();
        const int   nZ = holdParam ? 3 : 2;
        const float zW = W / (float)nZ;
        const float pad      = 5.f;
        const float labelH   = 14.f;           // zone labels en bas
        const float curveTop = 5.f;
        const float curveBt  = H - labelH - 4.f;

        const juce::Colour curveCol = Theme::curve;
        const juce::Colour tealCol = Theme::teal;
        const juce::Colour dimCol  = Theme::textMuted;

        // Séparateurs de zones (sans fond)
        g.setColour(tealCol.withAlpha(0.18f));
        for (int i = 1; i < nZ; ++i)
            g.fillRect(zW * (float)i - 0.5f, curveTop + 2.f, 1.f, curveBt - curveTop - 2.f);

        juce::RangedAudioParameter* zP[3] = { nullptr, nullptr, nullptr };
        if (nZ == 3) { zP[0] = atkParam; zP[1] = holdParam; zP[2] = relParam; }
        else         { zP[0] = atkParam; zP[1] = relParam; }

        const char* lbls[3] = { "ATTACK", (nZ == 3 ? "HOLD" : "RELEASE"), "RELEASE" };

        for (int z = 0; z < nZ; ++z)
        {
            float x0 = zW * (float)z;
            float x1 = x0 + zW;
            float xl  = x0 + pad + 3.f;
            float xr  = x1 - pad - 3.f;
            float yt  = curveTop + 2.f;
            float yb  = curveBt  - 2.f;
            float cx  = (x0 + x1) * 0.5f;

            float norm = zP[z] ? zP[z]->getValue() : 0.3f;
            float ms   = zP[z] ? zP[z]->convertFrom0to1(norm) * 1000.f : 0.f;

            bool isHold = (nZ == 3 && z == 1);
            bool isRel  = (z == nZ - 1);

            if (!isHold)
            {
                juce::Path curve;
                if (!isRel) // ATTACK: monte de bas-gauche vers haut-droite
                {
                    float cpX = xl + norm * (xr - xl) * 0.65f;
                    curve.startNewSubPath(xl, yb);
                    curve.quadraticTo(cpX, yt, xr, yt);
                }
                else // RELEASE: descend de haut-gauche vers bas-droite
                {
                    float cpX = xr - norm * (xr - xl) * 0.65f;
                    curve.startNewSubPath(xl, yt);
                    curve.quadraticTo(cpX, yb, xr, yb);
                }
                g.setColour(curveCol.withAlpha(0.82f));
                g.strokePath(curve, juce::PathStrokeType(2.0f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                // Valeur ms (toujours visible, pas d'ovale)
                juce::String msStr = (ms < 10.f)
                    ? juce::String(ms, 1) + "ms"
                    : juce::String((int)ms) + "ms";
                float valY = (yt + yb) * 0.5f - 5.f;
                g.setFont(juce::Font(juce::FontOptions(7.5f)));
                g.setColour(dimCol.withAlpha(0.72f));
                g.drawText(msStr, (int)(x0 + 2.f), (int)valY, (int)(zW - 4.f), 12,
                           juce::Justification::centred);
            }
            else // HOLD: ligne plate au niveau du haut des courbes
            {
                float lineY = yt;
                g.setColour(curveCol.withAlpha(0.82f));
                g.drawLine(xl, lineY, xr, lineY, 2.0f);

                juce::String msStr = (ms < 10.f)
                    ? juce::String(ms, 1) + "ms"
                    : juce::String((int)ms) + "ms";
                float oy = (yt + yb) * 0.5f - 7.f;

                bool draggingThis = isDragging && (dragZone == z);
                if (draggingThis)
                {
                    // Bulle glass pendant le drag
                    juce::ColourGradient glassFill(
                        juce::Colours::white.withAlpha(0.32f), cx - 10.f, oy,
                        tealCol.withAlpha(0.14f),              cx + 10.f, oy + 14.f, false);
                    g.setGradientFill(glassFill);
                    g.fillEllipse(cx - 22.f, oy, 44.f, 14.f);
                    g.setColour(tealCol.withAlpha(0.55f));
                    g.drawEllipse(cx - 22.f, oy, 44.f, 14.f, 1.2f);
                    g.setColour(juce::Colours::white.withAlpha(0.18f));
                    g.drawEllipse(cx - 20.f, oy + 1.f, 40.f, 5.f, 0.7f);
                }
                g.setFont(juce::Font(juce::FontOptions(draggingThis ? 8.0f : 7.5f)
                    .withStyle(draggingThis ? "Bold" : "")));
                g.setColour(draggingThis ? tealCol : dimCol.withAlpha(0.72f));
                g.drawText(msStr, (int)(x0 + 2.f), (int)oy, (int)(zW - 4.f), 14,
                           juce::Justification::centred);
            }

            // Label de zone — sous la zone des courbes (en dehors)
            g.setFont(juce::Font(juce::FontOptions(6.5f).withStyle("Bold")));
            g.setColour(tealCol.withAlpha(0.52f));
            g.drawText(lbls[z], (int)x0, (int)(H - labelH + 1.f), (int)zW, 12,
                       juce::Justification::centred);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragZone     = zoneAt(e.x);
        dragStartY   = e.y;
        auto* p      = zoneParam(dragZone);
        dragStartVal = p ? p->getValue() : 0.f;
        isDragging   = true;
        if (p) p->beginChangeGesture();
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragZone < 0) return;
        auto* p = zoneParam(dragZone);
        if (!p) return;
        float delta = (float)(dragStartY - e.y) / (float)(getHeight() * 3.0f);
        p->setValueNotifyingHost(juce::jlimit(0.f, 1.f, dragStartVal + delta));
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (auto* p = zoneParam(dragZone)) p->endChangeGesture();
        isDragging = false;
        dragZone   = -1;
        repaint();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        auto* p = zoneParam(zoneAt(e.x));
        if (!p) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->getDefaultValue());
        p->endChangeGesture();
        repaint();
    }

    juce::MouseCursor getMouseCursor() override
    {
        return juce::MouseCursor::UpDownResizeCursor;
    }

private:
    int zoneAt(int mx) const
    {
        int nZ = holdParam ? 3 : 2;
        return juce::jlimit(0, nZ - 1, (int)((float)mx / (float)getWidth() * (float)nZ));
    }

    juce::RangedAudioParameter* zoneParam(int z) const
    {
        if (holdParam) { if (z == 0) return atkParam; if (z == 1) return holdParam; return relParam; }
        return (z == 0) ? atkParam : relParam;
    }

    int   dragZone     = -1;
    int   dragStartY   = 0;
    float dragStartVal = 0.f;
    bool  isDragging   = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AHRTrizone)
};

// (anciens DeEssARSliders et GateAHRSliders remplacés par AHRTrizone)
class DeEssARSliders : public juce::Component
{
public:
    DeEssARSliders() = default;
    juce::RangedAudioParameter* atkParam = nullptr;
    juce::RangedAudioParameter* relParam = nullptr;

    void paint(juce::Graphics& g) override
    {
        float W = (float)getWidth(), H = (float)getHeight();
        const float sliderAreaW = W * 0.55f;
        const float ghostX      = sliderAreaW + 10.0f;
        const float ghostW      = W - ghostX - 4.0f;
        const float trackX      = 22.0f;
        const float trackW      = sliderAreaW - trackX - 44.0f;
        const float rowH        = (H - 16.0f) / 2.0f;

        const char* labels[2] = { "A", "R" };
        juce::RangedAudioParameter* params[2] = { atkParam, relParam };

        // ── Sliders A et R ────────────────────────────────────────────────
        for (int i = 0; i < 2; ++i)
        {
            float cy = 10.0f + rowH * i + rowH * 0.5f;

            // Lettre
            g.setFont(juce::Font(juce::FontOptions(8.5f).withStyle("Bold")));
            g.setColour(Theme::textMuted);
            g.drawText(labels[i], 4, (int)(cy - 7), 16, 14, juce::Justification::centred);

            // Track de fond
            float ty = cy - 3.0f;
            g.setColour(Theme::surfaceMuted.withAlpha(0.55f));
            g.fillRoundedRectangle(trackX, ty, trackW, 6.0f, 3.0f);

            // Fill teal proportionnel
            float norm = params[i] ? params[i]->getValue() : 0.5f;
            g.setGradientFill(juce::ColourGradient(
                Theme::tealBright, trackX, 0,
                Theme::teal, trackX + trackW, 0, false));
            g.fillRoundedRectangle(trackX, ty, trackW * norm, 6.0f, 3.0f);

            // Thumb blanc
            float tx = trackX + norm * trackW - 5.0f;
            g.setColour(juce::Colours::white);
            g.fillEllipse(tx, cy - 5.5f, 11.0f, 11.0f);
            g.setColour(Theme::teal.withAlpha(0.55f));
            g.drawEllipse(tx + 0.5f, cy - 5.0f, 10.0f, 10.0f, 1.0f);

            // Valeur numérique
            float ms = params[i]
                ? params[i]->convertFrom0to1(params[i]->getValue()) * 1000.0f
                : 0.0f;
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(Theme::teal);
            g.drawText(juce::String(ms, 1) + " ms",
                       (int)(trackX + trackW + 4), (int)(cy - 7), 38, 14,
                       juce::Justification::centredLeft);
        }

        // ── Séparateur ────────────────────────────────────────────────────
        g.setColour(Theme::textMuted.withAlpha(0.12f));
        g.fillRect(ghostX - 6.0f, 6.0f, 1.0f, H - 12.0f);

        // ── Ghost AR envelope ─────────────────────────────────────────────
        float atkN = atkParam ? atkParam->getValue() : 0.3f;
        float relN = relParam ? relParam->getValue() : 0.3f;
        float gH   = H - 24.0f;
        float gBot = H - 10.0f;

        float atkPx = atkN * ghostW * 0.45f;
        float holdPx = ghostW * 0.12f;
        float relPx  = relN * ghostW * 0.43f;
        float sumPx  = atkPx + holdPx + relPx;
        if (sumPx > ghostW) {
            float sc = ghostW / sumPx;
            atkPx *= sc; holdPx *= sc; relPx *= sc;
        }

        juce::Path ghost;
        ghost.startNewSubPath(ghostX, gBot);
        ghost.lineTo(ghostX + atkPx, gBot - gH);
        ghost.lineTo(ghostX + atkPx + holdPx, gBot - gH);
        ghost.lineTo(ghostX + atkPx + holdPx + relPx, gBot);

        g.setColour(Theme::teal.withAlpha(0.12f));
        juce::Path fill = ghost;
        fill.lineTo(ghostX, gBot); fill.closeSubPath();
        g.fillPath(fill);
        g.setColour(Theme::teal.withAlpha(0.60f));
        g.strokePath(ghost, juce::PathStrokeType(1.5f));

        // Labels A / R dans le ghost
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(Theme::teal.withAlpha(0.45f));
        g.drawText("A", (int)(ghostX + atkPx * 0.35f), (int)(gBot - gH - 14), 14, 10,
                   juce::Justification::centred);
        g.drawText("R", (int)(ghostX + atkPx + holdPx + relPx * 0.5f), (int)(gBot - gH * 0.5f), 14, 10,
                   juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragSeg = segAt(e.x, e.y);
        dragStartX = e.x;
        if (dragSeg == 0 && atkParam) { atkParam->beginChangeGesture(); dragStartVal = atkParam->getValue(); }
        if (dragSeg == 1 && relParam) { relParam->beginChangeGesture(); dragStartVal = relParam->getValue(); }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        float trackW = (float)getWidth() * 0.55f - 22.0f - 44.0f;
        float delta  = (float)(e.x - dragStartX) / trackW;
        if (dragSeg == 0 && atkParam)
            atkParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartVal + delta));
        if (dragSeg == 1 && relParam)
            relParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartVal + delta));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragSeg == 0 && atkParam) atkParam->endChangeGesture();
        if (dragSeg == 1 && relParam) relParam->endChangeGesture();
        dragSeg = -1;
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        int seg = segAt(e.x, e.y);
        juce::RangedAudioParameter* p = (seg == 0) ? atkParam : relParam;
        if (!p) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->getDefaultValue());
        p->endChangeGesture();
        repaint();
    }
    juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::LeftRightResizeCursor; }

private:
    int segAt(int /*mx*/, int my)
    {
        float rowH = (getHeight() - 16.0f) / 2.0f;
        if ((float)my < 10.0f + rowH) return 0; // A
        if ((float)my < 10.0f + rowH * 2.0f) return 1; // R
        return -1;
    }

    int   dragSeg    = -1;
    int   dragStartX = 0;
    float dragStartVal = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEssARSliders)
};

// =============================================================================
//  COMPOSANT : GateAHRSliders — Attack / Hold / Release en sliders horizontaux
//              + aperçu enveloppe ghost (style B+)
// =============================================================================
class GateAHRSliders : public juce::Component
{
public:
    GateAHRSliders() = default;
    juce::RangedAudioParameter* atkParam  = nullptr;
    juce::RangedAudioParameter* holdParam = nullptr;
    juce::RangedAudioParameter* relParam  = nullptr;

    void paint(juce::Graphics& g) override
    {
        float W = (float)getWidth(), H = (float)getHeight();

        // Zone sliders (gauche 58 %) et ghost envelope (droite 38 %)
        const float sliderAreaW = W * 0.575f;
        const float ghostX      = sliderAreaW + 10.0f;
        const float ghostW      = W - ghostX - 4.0f;

        // Dimensions des sliders
        const float trackX  = 22.0f;
        const float trackW  = sliderAreaW - trackX - 44.0f;
        const float rowH    = (H - 18.0f) / 3.0f;

        struct Seg { const char* lbl; juce::RangedAudioParameter* p; float maxMs; };
        Seg segs[3] = { {"A", atkParam, 50.0f}, {"H", holdParam, 500.0f}, {"R", relParam, 2000.0f} };

        for (int i = 0; i < 3; ++i)
        {
            float ty  = 10.0f + i * rowH + rowH * 0.5f;
            float val = segs[i].p ? segs[i].p->getValue() : 0.3f;
            float tx  = trackX + val * trackW;

            // Lettre A / H / R
            g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
            g.setColour(Theme::textMuted);
            g.drawText(segs[i].lbl, 2, (int)(ty - 6), (int)(trackX - 4), 12,
                       juce::Justification::centredRight);

            // Piste inactive
            g.setColour(Theme::textPrimary);
            g.fillRoundedRectangle(trackX, ty - 2.0f, trackW, 4.0f, 2.0f);

            // Remplissage gauche (gradient cyan)
            if (val > 0.01f)
            {
                juce::ColourGradient grad(
                    Theme::accent.withAlpha(0.45f), trackX, ty,
                    Theme::accent, tx, ty, false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(trackX, ty - 2.0f, tx - trackX, 4.0f, 2.0f);
            }

            // Thumb blanc + ring teal (même taille que DeEssAR : 11px)
            g.setColour(juce::Colours::white);
            g.fillEllipse(tx - 5.5f, ty - 5.5f, 11.0f, 11.0f);
            g.setColour(Theme::accentAlt);
            g.drawEllipse(tx - 4.5f, ty - 4.5f, 9.0f, 9.0f, 1.5f);

            // Valeur en ms
            float ms = segs[i].p
                ? segs[i].p->convertFrom0to1(val) * 1000.0f
                : val * segs[i].maxMs;
            juce::String valStr = (ms < 10.0f)
                ? juce::String(ms, 1) + " ms"
                : juce::String((int)ms) + " ms";
            g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
            g.setColour(Theme::accent);
            g.drawText(valStr, (int)(trackX + trackW + 4.0f), (int)(ty - 6), 42, 12,
                       juce::Justification::centredLeft);
        }

        // Séparateur vertical
        g.setColour(Theme::textMuted.withAlpha(0.12f));
        g.fillRect(sliderAreaW + 2.0f, 8.0f, 1.0f, H - 16.0f);

        // ── Ghost envelope (zone droite) ────────────────────────────────────
        float gx  = ghostX, gy = 10.0f, gh = H - 20.0f;
        float vA  = atkParam  ? atkParam->getValue()  : 0.15f;
        float vH  = holdParam ? holdParam->getValue() : 0.30f;
        float vR  = relParam  ? relParam->getValue()  : 0.55f;

        float aw   = ghostW * vA * 0.52f;
        float hx   = gx + aw;
        float hEnd = juce::jmin(hx + ghostW * vH * 0.24f, gx + ghostW * 0.76f);
        float rx   = juce::jmin(hEnd + ghostW * vR * 0.22f, gx + ghostW - 2.0f);
        float top  = gy + 3.0f, bot = gy + gh - 3.0f;

        // Baseline
        g.setColour(Theme::textMuted.withAlpha(0.14f));
        g.fillRect(gx, bot, ghostW, 1.0f);

        // Fill sous la courbe
        juce::Path fill;
        fill.startNewSubPath(gx, bot);
        fill.lineTo(hx, top); fill.lineTo(hEnd, top); fill.lineTo(rx, bot);
        fill.lineTo(gx + ghostW, bot); fill.lineTo(gx, bot); fill.closeSubPath();
        g.setColour(Theme::accent.withAlpha(0.09f));
        g.fillPath(fill);

        // Courbe
        juce::Path curve;
        curve.startNewSubPath(gx, bot);
        curve.lineTo(hx, top); curve.lineTo(hEnd, top); curve.lineTo(rx, bot);
        g.setColour(Theme::accent.withAlpha(0.55f));
        g.strokePath(curve, juce::PathStrokeType(1.8f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Labels zones A H R sous la courbe
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(Theme::textMuted.withAlpha(0.50f));
        if (hx   - gx   > 8.0f) g.drawText("A", (int)((gx + hx)     * 0.5f - 5), (int)(bot + 2), 10, 8, juce::Justification::centred);
        if (hEnd  - hx  > 6.0f) g.drawText("H", (int)((hx + hEnd)   * 0.5f - 5), (int)(bot + 2), 10, 8, juce::Justification::centred);
        if (rx   - hEnd > 8.0f) g.drawText("R", (int)((hEnd + rx)   * 0.5f - 5), (int)(bot + 2), 10, 8, juce::Justification::centred);

        // Petit label en haut de la zone ghost
        g.setFont(juce::Font(juce::FontOptions(6.5f).withStyle("Bold")));
        g.setColour(Theme::textMuted.withAlpha(0.35f));
        g.drawText("AHR", (int)gx, 1, (int)ghostW, 8, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        float H = (float)getHeight();
        const float rowH = (H - 18.0f) / 3.0f;
        juce::RangedAudioParameter* ps[3] = { atkParam, holdParam, relParam };

        dragSeg = -1;
        for (int i = 0; i < 3; ++i)
        {
            float ty = 10.0f + i * rowH + rowH * 0.5f;
            if (std::fabs((float)e.y - ty) < 14.0f)
            {
                dragSeg    = i;
                dragStartX = e.x;
                dragStartVal = ps[i] ? ps[i]->getValue() : 0.3f;
                if (ps[i]) ps[i]->beginChangeGesture();
                break;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragSeg < 0) return;
        float W = (float)getWidth();
        const float trackX = 22.0f;
        const float trackW = W * 0.575f - trackX - 44.0f;
        juce::RangedAudioParameter* ps[3] = { atkParam, holdParam, relParam };

        float delta  = ((float)e.x - (float)dragStartX) / trackW;
        float newVal = juce::jlimit(0.0f, 1.0f, dragStartVal + delta);
        if (ps[dragSeg]) ps[dragSeg]->setValueNotifyingHost(newVal);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        juce::RangedAudioParameter* ps[3] = { atkParam, holdParam, relParam };
        if (dragSeg >= 0 && ps[dragSeg]) ps[dragSeg]->endChangeGesture();
        dragSeg = -1;
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        float H = (float)getHeight();
        const float rowH = (H - 18.0f) / 3.0f;
        juce::RangedAudioParameter* ps[3] = { atkParam, holdParam, relParam };
        for (int i = 0; i < 3; ++i)
        {
            float ty = 10.0f + i * rowH + rowH * 0.5f;
            if (std::fabs((float)e.y - ty) < 14.0f && ps[i])
            {
                ps[i]->beginChangeGesture();
                ps[i]->setValueNotifyingHost(ps[i]->getDefaultValue());
                ps[i]->endChangeGesture();
                repaint();
                break;
            }
        }
    }

    juce::MouseCursor getMouseCursor() override
    {
        return juce::MouseCursor::LeftRightResizeCursor;
    }

private:
    int   dragSeg      = -1;
    int   dragStartX   = 0;
    float dragStartVal = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GateAHRSliders)
};

// =============================================================================
//  COMPOSANT : DeEssDetectDisplay — threshold + zone de détection sur spectre
// =============================================================================
class DeEssDetectDisplay : public juce::Component
{
public:
    DeEssDetectDisplay() = default;
    juce::RangedAudioParameter* threshParam  = nullptr;
    juce::RangedAudioParameter* detectParam  = nullptr;
    juce::RangedAudioParameter* freqParam    = nullptr;
    juce::RangedAudioParameter* modeParam    = nullptr; // deEssFilterMode (bool)
    InvisibleFixAudioProcessor* processor    = nullptr;

    void paint(juce::Graphics& g) override
    {
        float W = (float)getWidth(), H = (float)getHeight() - 20.0f;
        float threshN = threshParam ? threshParam->getValue() : 0.55f;
        float detectN = detectParam ? detectParam->getValue() : 0.50f;
        float freqN   = freqParam   ? freqParam->getValue()   : 0.60f;

        const float barLeft = 32.0f; // marge gauche pour le label THRESH
        // Fréquence de détection (2k–16k → log-scale)
        auto normAtFreq = [](float f) { return std::log(f / 2000.0f) / std::log(8.0f); };
        auto xAtFreq    = [&](float f) { return barLeft + normAtFreq(juce::jlimit(2000.0f, 16000.0f, f)) * (W - barLeft - 6.0f); };
        float fc = 2000.0f * std::pow(8.0f, freqN);

        // Zone detect : centrée sur fc, largeur proportionnelle à detectN
        float zoneOctaves = 0.5f + detectN * 2.5f;
        float fLow  = juce::jlimit(2000.0f, 16000.0f, fc * std::pow(2.0f, -zoneOctaves * 0.5f));
        float fHigh = juce::jlimit(2000.0f, 16000.0f, fc * std::pow(2.0f,  zoneOctaves * 0.5f));
        float xLow  = xAtFreq(fLow),  xHigh = xAtFreq(fHigh), fcX = xAtFreq(fc);

        // Threshold Y position
        float threshY = 6.0f + (1.0f - threshN) * (H - 16.0f);

        // Canvas background
        g.setColour(Theme::surfaceLighter.withAlpha(0.80f));
        g.fillRoundedRectangle(0, 0, W, H, 5.0f);
        g.setColour(Theme::accent.withAlpha(0.10f));
        g.drawRoundedRectangle(0.5f, 0.5f, W - 1, H - 1, 5.0f, 0.5f);
        // Séparateur marge label / zone fréquences
        g.setColour(Theme::teal.withAlpha(0.12f));
        g.fillRect(barLeft - 0.5f, 2.0f, 1.0f, H - 4.0f);

        // === BARRES FFT LIVE (2k–16kHz) ===
        if (processor != nullptr)
        {
            float sr = (float)processor->getSampleRate();
            if (sr < 1.0f) sr = 44100.0f;
            float fftMags[2048]{};
            processor->copyInputFftMagnitudes(fftMags, 2048);

            int nBarW = (int)(W - barLeft - 6.0f);
            for (int px = 0; px < nBarW; ++px)
            {
                float fn   = (float)px / (float)nBarW;
                float freq = 2000.0f * std::pow(8.0f, fn);
                int   bin  = juce::jlimit(1, 2047, (int)(freq * 2048.0f / sr));
                float magDB = 20.0f * std::log10(fftMags[bin] / 2048.0f + 1e-9f);
                // Compensation de pente spectrale : +3 dB/octave au-dessus de 2 kHz
                magDB += 10.0f * std::log10(freq / 2000.0f);
                // Plage plus sensible : -80 dB → 0, -20 dB → 1.0
                float barNorm = juce::jlimit(0.0f, 1.0f, (magDB + 80.0f) / 60.0f);
                if (barNorm < 0.01f) continue;

                float barH  = barNorm * (H - 10.0f);
                float barY  = H - barH;
                bool aboveThresh = (barY < threshY);
                g.setColour(aboveThresh ? Theme::accent.withAlpha(0.65f)
                                        : Theme::textMuted.withAlpha(0.30f));
                g.fillRect(barLeft + (float)px, barY, 1.0f, barH);
            }
        }

        // Zone de détection : Band = rectangle autour de fc, Shelf = dégradé vers la droite
        bool isBellMode = modeParam ? (modeParam->getValue() > 0.5f) : true;
        if (isBellMode) {
            // BAND : zone centrée sur fc avec bords pointillés
            g.setColour(Theme::accent.withAlpha(0.10f));
            g.fillRect(xLow, 0.0f, xHigh - xLow, H);
            g.setColour(Theme::accent.withAlpha(0.50f));
            for (float py = 4.0f; py < H - 2.0f; py += 7.0f) {
                g.fillRect(xLow,  py, 1.5f, 4.0f);
                g.fillRect(xHigh, py, 1.5f, 4.0f);
            }
        } else {
            // SHELF : dégradé cyan de fc vers le bord droit, symbolisant le shelf
            juce::ColourGradient shelfGrad(
                Theme::teal.withAlpha(0.22f), fcX, 0.0f,
                Theme::accent.withAlpha(0.08f), W - 6.0f, 0.0f,
                false);
            g.setGradientFill(shelfGrad);
            g.fillRect(fcX, 0.0f, W - 6.0f - fcX, H);
            // Bord gauche shelf (ligne solide depuis fc)
            g.setColour(Theme::teal.withAlpha(0.60f));
            g.fillRect(fcX, 0.0f, 1.5f, H);
        }

        // Ligne fc (centre de zone)
        g.setColour(Theme::accent.withAlpha(0.65f));
        g.fillRect(fcX - 0.75f, 0.0f, 1.5f, H);

        // Ligne threshold — pointillés teal + triangle indicateur
        g.setColour(Theme::teal.withAlpha(0.85f));
        for (float px = barLeft; px < W - 6.0f; px += 8.0f)
            g.fillRect(px, threshY, 5.0f, 1.5f);

        // Poignée drag threshold (triangle dans la marge gauche, pointant vers les barres)
        juce::Path tri;
        tri.startNewSubPath(barLeft - 11.0f, threshY - 5.0f);
        tri.lineTo(barLeft, threshY);
        tri.lineTo(barLeft - 11.0f, threshY + 5.0f);
        tri.closeSubPath();
        g.setColour(Theme::teal);
        g.fillPath(tri);

        // Label THRESH + valeur dB collés à la ligne, dans la marge gauche
        if (threshParam) {
            float dB = threshParam->convertFrom0to1(threshN);
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(Theme::teal);
            g.drawText("THRESH", 1, (int)(threshY - 13), (int)barLeft - 2, 10,
                       juce::Justification::centredLeft);
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.drawText(juce::String(dB, 1) + "dB", 1, (int)(threshY + 2), (int)barLeft - 2, 9,
                       juce::Justification::centredLeft);
        }

        // Label fc centré sur la ligne de fréquence
        juce::String fcLbl = (fc >= 1000.0f)
            ? juce::String(fc / 1000.0f, 1) + " kHz"
            : juce::String((int)fc) + " Hz";
        g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
        g.setColour(Theme::accent);
        g.drawText(fcLbl, (int)(fcX - 20), 2, 42, 10, juce::Justification::centred);

        // Pill BAND / SHELF — overlay top-right du canvas
        {
            bool isBell = modeParam ? (modeParam->getValue() > 0.5f) : true;
            float pillW = 72.0f, pillH = 14.0f;
            float pillX = W - pillW - 6.0f, pillY = 4.0f;
            float halfW = pillW * 0.5f;

            // Fond pill
            g.setColour(Theme::surfaceLight.withAlpha(0.85f));
            g.fillRoundedRectangle(pillX, pillY, pillW, pillH, 4.0f);
            g.setColour(Theme::accent.withAlpha(0.30f));
            g.drawRoundedRectangle(pillX + 0.5f, pillY + 0.5f, pillW - 1.0f, pillH - 1.0f, 4.0f, 0.8f);

            // Côté actif (highlight cyan)
            if (isBell) {
                g.setColour(Theme::accent.withAlpha(0.22f));
                g.fillRoundedRectangle(pillX, pillY, halfW, pillH, 4.0f);
            } else {
                g.setColour(Theme::teal.withAlpha(0.22f));
                g.fillRoundedRectangle(pillX + halfW, pillY, halfW, pillH, 4.0f);
            }

            // Labels
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(isBell ? Theme::accent : Theme::textMuted);
            g.drawText("BAND", (int)(pillX), (int)pillY, (int)halfW, (int)pillH, juce::Justification::centred);
            g.setColour(!isBell ? Theme::teal : Theme::textMuted);
            g.drawText("SHELF", (int)(pillX + halfW), (int)pillY, (int)halfW, (int)pillH, juce::Justification::centred);
        }

        // Axes fréquences sous le canvas
        const float labFreqs[] = { 2000, 4000, 8000, 16000 };
        const char* labStrs[]  = { "2k", "4k", "8k", "16k" };
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        for (int li = 0; li < 4; ++li) {
            float lx = xAtFreq(labFreqs[li]);
            g.setColour(Theme::textMuted);
            g.drawText(labStrs[li], (int)(lx - 10), (int)(H + 4), 22, 10,
                       juce::Justification::centred);
        }

        // Légende bas — Q uniquement (THRESH est maintenant sur la ligne)
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(Theme::accent);
        g.drawText("Q", (int)(W - 20), (int)(H + 10), 20, 10, juce::Justification::centredRight);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        float W = (float)getWidth(), H = (float)getHeight() - 22.0f;
        float threshN = threshParam ? threshParam->getValue() : 0.55f;
        float threshY = 4.0f + (1.0f - threshN) * (H - 12.0f);

        // Clic sur la pill BAND/SHELF (top-right)
        float pillW = 72.0f, pillH = 14.0f;
        float pillX = W - pillW - 6.0f, pillY = 4.0f;
        if ((float)e.x >= pillX && (float)e.x <= pillX + pillW
            && (float)e.y >= pillY && (float)e.y <= pillY + pillH)
        {
            if (modeParam) {
                modeParam->beginChangeGesture();
                modeParam->setValueNotifyingHost(modeParam->getValue() > 0.5f ? 0.0f : 1.0f);
                modeParam->endChangeGesture();
            }
            repaint();
            return;
        }

        dragThreshold = (std::fabs((float)e.y - threshY) < 12.0f);
        if (dragThreshold && threshParam) threshParam->beginChangeGesture();
        if (!dragThreshold && freqParam)  freqParam->beginChangeGesture();
        dragStartX = e.x;
        dragStartY = e.y;
        dragStartVal = dragThreshold ? (threshParam ? threshParam->getValue() : 0.55f)
                                     : (freqParam   ? freqParam->getValue()   : 0.60f);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragThreshold && threshParam) {
            float H = (float)getHeight() - 22.0f - 12.0f;
            float delta = -(float)(e.y - dragStartY) / juce::jmax(1.0f, H);
            threshParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartVal + delta));
        } else if (freqParam) {
            float delta = (float)(e.x - dragStartX) / juce::jmax(1.0f, (float)getWidth() - 12.0f);
            freqParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartVal + delta));
        }
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragThreshold && threshParam) threshParam->endChangeGesture();
        if (!dragThreshold && freqParam)  freqParam->endChangeGesture();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        float H = (float)getHeight() - 22.0f;
        float threshN = threshParam ? threshParam->getValue() : 0.55f;
        float threshY = 4.0f + (1.0f - threshN) * (H - 12.0f);
        bool hitThresh = (std::fabs((float)e.y - threshY) < 12.0f);
        auto reset = [](juce::RangedAudioParameter* p) {
            if (!p) return;
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->getDefaultValue());
            p->endChangeGesture();
        };
        if (hitThresh) reset(threshParam);
        else           reset(freqParam);
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        // Molette ajuste le Q (detectParam) uniquement en mode Band
        bool isBell = modeParam ? (modeParam->getValue() > 0.5f) : true;
        if (!isBell || !detectParam) return;
        float newVal = juce::jlimit(0.0f, 1.0f, detectParam->getValue() - wheel.deltaY * 0.12f);
        detectParam->beginChangeGesture();
        detectParam->setValueNotifyingHost(newVal);
        detectParam->endChangeGesture();
        repaint();
    }

    juce::MouseCursor getMouseCursor() override
    {
        return juce::MouseCursor::CrosshairCursor;
    }

private:
    bool  dragThreshold = true;
    int   dragStartX = 0, dragStartY = 0;
    float dragStartVal = 0.5f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEssDetectDisplay)
};

// =============================================================================
//  COMPOSANT : RoomVisualizer (Écran Graphique pour le Notch)
// =============================================================================
class RoomVisualizer : public juce::Component, private juce::Timer
{
public:
    RoomVisualizer(InvisibleFixAudioProcessor& p) : processor(p)
    {
        startTimerHz(30);
    }

    ~RoomVisualizer() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        int W = getWidth(), H = getHeight();
        float wf = (float)W, hf = (float)H;

        auto freqAtX = [&](float x) { return 50.0f * std::pow(100.0f, x / wf); };
        auto xAtFreq = [&](float f) { return wf * std::log(f / 50.0f) / std::log(100.0f); };

        // === HEATMAP — TRAITS VERTICAUX THERMIQUES ===========================
        // Utilise le FFT d'entrée brute : reste actif même en mode Delta
        processor.copyInputFftMagnitudes(displayMags, 2048);
        float sr = (float)processor.getSampleRate();
        if (sr < 1.0f) sr = 44100.0f;

        auto lerp8 = [](float t, int a, int b) -> juce::uint8
            { return (juce::uint8)juce::jlimit(0, 255, (int)(a + t * (b - a))); };

        g.fillAll(Theme::surfacePale);

        for (int px = 0; px < W; ++px)
        {
            float freq  = freqAtX((float)px);
            int   bin   = juce::jlimit(0, 2047, (int)(freq * 2048.0f / sr));
            float magDB = 20.0f * std::log10(displayMags[bin] / 2048.0f + 1e-7f);
            float e = juce::jlimit(0.0f, 1.0f, (magDB + 65.0f) / 50.0f);

            if (e < 0.04f) continue;

            int barH = juce::jlimit(1, H, (int)(e * hf));
            int barY = H - barH;

            juce::uint8 r, gg, b;
            if      (e < 0.10f) { float t = e / 0.10f;             r=lerp8(t,160,100); gg=lerp8(t,180,120); b=lerp8(t,200,210); }
            else if (e < 0.28f) { float t = (e - 0.10f) / 0.18f;   r=lerp8(t,100,40);  gg=lerp8(t,120,160); b=lerp8(t,210,180); }
            else if (e < 0.52f) { float t = (e - 0.28f) / 0.24f;   r=lerp8(t,40,250);  gg=lerp8(t,160,130); b=lerp8(t,180,0);   }
            else if (e < 0.76f) { float t = (e - 0.52f) / 0.24f;   r=lerp8(t,250,253); gg=lerp8(t,130,80);  b=0;                }
            else                { float t = (e - 0.76f) / 0.24f;   r=lerp8(t,253,220); gg=lerp8(t,80,30);   b=lerp8(t,0,10);    }

            g.setColour(juce::Colour(r, gg, b).withAlpha(0.75f));
            g.fillRect(px, barY, 1, barH);
        }

        // Grille subtile
        g.setColour(Theme::black.withAlpha(0.07f));
        for (float fx : { 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f })
            g.drawVerticalLine((int)xAtFreq(fx), 0.0f, hf);
        for (float fy = 0.25f; fy < 1.0f; fy += 0.25f)
            g.drawHorizontalLine((int)(hf * fy), 0.0f, wf);

        // === COURBE NOTCH — sigma corrigé =====================================
        auto& apvts = processor.apvts;
        double fc   = juce::jlimit(100.0, 2000.0,
                        (double)*apvts.getRawParameterValue("roomNotchFreq"));
        double Q    = juce::jlimit(0.5, 15.0,
                        (double)*apvts.getRawParameterValue("roomNotchQ"));
        float knobRoom      = *apvts.getRawParameterValue("room");
        float roomBoost     = *apvts.getRawParameterValue("roomDepthBoost");
        float dynDepthLive  = processor.getRoomDynDepth(); // 0..1 live
        float fcX   = xAtFreq((float)fc);

        float sigma = wf * (float)(std::log(1.0 + 1.0 / (2.0 * Q)) / std::log(100.0));
        sigma = juce::jmax(2.0f, sigma);

        float baseY   = hf * 0.15f;
        float maxDipY = hf * 0.72f;
        // Profondeur visuelle = ce que fait vraiment le DSP (avec boost)
        float visualDepth = juce::jmin(1.0f, knobRoom * dynDepthLive * roomBoost);

        juce::Path curve;
        bool first = true;
        for (int px = 0; px < W; ++px)
        {
            float dx  = (float)px - fcX;
            float dip = std::exp(-0.5f * (dx / sigma) * (dx / sigma));
            float py  = baseY + dip * maxDipY * juce::jmax(knobRoom * 0.2f, visualDepth);
            if (first) { curve.startNewSubPath((float)px, py); first = false; }
            else         curve.lineTo((float)px, py);
        }

        // Zone retirée : remplissage rouge proportionnel à la réduction active
        {
            juce::Path fill = curve;
            fill.lineTo(wf, baseY);
            fill.lineTo(0.0f, baseY);
            fill.closeSubPath();
            // Couche rouge = ce qui est supprimé (dynamique live)
            g.setColour(Theme::roomHot.withAlpha(0.08f + dynDepthLive * knobRoom * 0.22f));
            g.fillPath(fill);
            // Légère teinte cyan par-dessus (empreinte du filtre)
            g.setColour(Theme::accent.withAlpha(0.05f));
            g.fillPath(fill);
        }

        // Courbe principale : cyan si peu de réduction, orange/rouge si forte
        float hue = juce::jlimit(0.0f, 1.0f, dynDepthLive * knobRoom * roomBoost);
        juce::Colour curveCol = Theme::accent.interpolatedWith(Theme::warmRed, hue * 0.7f);
        g.setColour(curveCol.withAlpha(0.95f));
        g.strokePath(curve, juce::PathStrokeType(2.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Indicateur numérique de réduction effective (dB)
        {
            float reductDB = -24.0f * knobRoom * dynDepthLive * roomBoost;
            reductDB = juce::jlimit(-30.0f, 0.0f, reductDB);
            if (reductDB < -0.5f)
            {
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.setColour(Theme::warmRed.withAlpha(0.75f));
                g.drawText(juce::String(reductDB, 1) + " dB",
                    juce::Rectangle<float>(fcX - 24.f, baseY - 13.f, 48.f, 11.f),
                    juce::Justification::centred);
            }
        }

        // Marqueurs ±1σ (bande passante réelle du filtre)
        float xLeft  = fcX - sigma;
        float xRight = fcX + sigma;
        g.setColour(Theme::accent.withAlpha(0.45f));
        for (float marker : { xLeft, xRight })
        {
            if (marker < 0.0f || marker > wf) continue;
            for (float py = 0.0f; py < hf; py += 6.0f)
                g.fillRect(marker, py, 1.5f, 2.5f);
        }

        g.setColour(Theme::teal.withAlpha(0.55f));
        for (float py = 0.0f; py < hf; py += 7.0f)
            g.fillRect(fcX, py, 1.8f, 3.0f);

        g.setColour(Theme::slate.withAlpha(0.40f));
        g.drawRect(getLocalBounds(), 1);
    }

private:
    void timerCallback() override { repaint(); }
    InvisibleFixAudioProcessor& processor;
    float displayMags[2048]{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoomVisualizer)
};

// =============================================================================
//  NRSpectrumDisplay — spectre FFT continu + threshold/floor draggables
// =============================================================================
class NRSpectrumDisplay : public juce::Component
{
public:
    NRSpectrumDisplay() = default;
    InvisibleFixAudioProcessor* processor{ nullptr };
    juce::RangedAudioParameter* threshParam{ nullptr };
    juce::RangedAudioParameter* floorParam { nullptr };

    void paint(juce::Graphics& g) override
    {
        float W = (float)getWidth(), H = (float)getHeight();

        // Fond
        g.setColour(Theme::surfaceMint);
        g.fillRoundedRectangle(0, 0, W, H, 5.0f);
        g.setColour(Theme::accent.withAlpha(0.10f));
        g.drawRoundedRectangle(0.5f, 0.5f, W-1, H-1, 5.0f, 1.0f);

        const float specY = 6.0f, specH = H - 20.0f;
        const float specX = 4.0f, specW = W - 8.0f;

        // Spectre FFT (inputFftMagnitudes)
        if (processor != nullptr)
        {
            float mags[512]{};
            processor->copyInputFftMagnitudes(mags, 512);

            // Normalise : trouver le max
            float maxM = 1e-10f;
            for (int i = 1; i < 512; ++i) maxM = juce::jmax(maxM, mags[i]);

            // Dessine en barres
            for (int px = 0; px < (int)specW; ++px)
            {
                float t    = (float)px / specW;
                int   bin  = juce::jlimit(1, 511, (int)(1 + t * t * 510.0f));
                float norm = juce::jlimit(0.0f, 1.0f, mags[bin] / maxM);
                float barH = norm * specH;
                float barY = specY + specH - barH;
                g.setColour(Theme::textPrimary.withAlpha(0.7f));
                g.fillRect(specX + (float)px, barY, 1.0f, barH);
            }

            // GR overlay (cyan) — zone entre thresh et floor
            float thDB = threshParam ? threshParam->convertFrom0to1(threshParam->getValue()) : -40.0f;
            float flDB = floorParam  ? floorParam->convertFrom0to1(floorParam->getValue())   : -60.0f;
            float dbRange = 80.0f; // -80 to 0
            float thY = specY + specH * (1.0f - (thDB + dbRange) / dbRange);
            float flY = specY + specH * (1.0f - (flDB + dbRange) / dbRange);
            thY = juce::jlimit(specY, specY + specH, thY);
            flY = juce::jlimit(specY, specY + specH, flY);

            // Overlay NR actif
            g.setColour(Theme::teal.withAlpha(0.18f));
            g.fillRect(specX, thY, specW, flY - thY);

            // Threshold line (orange, draggable)
            g.setColour(Theme::airWarm.withAlpha(0.90f));
            g.fillRect(specX, thY - 1.0f, specW, 2.0f);
            // Thumb
            g.fillRoundedRectangle(specX + specW - 18.0f, thY - 5.0f, 16.0f, 10.0f, 3.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.drawText("T", (int)(specX + specW - 18), (int)(thY - 5), 16, 10, juce::Justification::centred);

            // Floor line (cyan, draggable)
            g.setColour(Theme::accent.withAlpha(0.80f));
            g.fillRect(specX, flY - 1.0f, specW, 1.5f);
            g.fillRoundedRectangle(specX + specW - 18.0f, flY - 5.0f, 16.0f, 10.0f, 3.0f);
            g.setColour(juce::Colours::white);
            g.drawText("F", (int)(specX + specW - 18), (int)(flY - 5), 16, 10, juce::Justification::centred);

            // Labels dB
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(Theme::airWarm.withAlpha(0.80f));
            g.drawText(juce::String((int)thDB) + "dB", 2, (int)(thY - 8), 30, 10, juce::Justification::centredLeft);
            g.setColour(Theme::accent.withAlpha(0.80f));
            g.drawText(juce::String((int)flDB) + "dB", 2, (int)(flY + 1), 30, 10, juce::Justification::centredLeft);
        }

        // Labels fréquences bas
        const float fLabels[] = { 100,500,1000,5000,10000,20000 };
        const char* fStr[]    = { "100","500","1k","5k","10k","20k" };
        float sr = processor ? (float)processor->getSampleRate() : 44100.0f;
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(Theme::textMuted.withAlpha(0.60f));
        for (int i = 0; i < 6; ++i)
        {
            float t = std::sqrt((fLabels[i] - 20.0f) / (sr / 2.0f - 20.0f));
            float px = specX + t * specW;
            g.drawText(fStr[i], (int)(px - 12), (int)(specY + specH + 4), 26, 10, juce::Justification::centred);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const float specY = 6.0f, specH = (float)getHeight() - 20.0f;
        float dbRange = 80.0f;

        auto yToDb = [&](float y) {
            float norm = juce::jlimit(0.0f, 1.0f, (y - specY) / specH);
            return (1.0f - norm) * dbRange - dbRange;
        };

        float clickDB = yToDb((float)e.y);
        if (threshParam && floorParam) {
            float thDB = threshParam->convertFrom0to1(threshParam->getValue());
            float flDB = floorParam ->convertFrom0to1(floorParam->getValue());
            dragTarget = (std::abs(clickDB - thDB) < std::abs(clickDB - flDB)) ? 0 : 1;
            auto* p = (dragTarget == 0) ? threshParam : floorParam;
            if (p) p->beginChangeGesture();
        }
        mouseDrag(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragTarget < 0) return;
        const float specY = 6.0f, specH = (float)getHeight() - 20.0f;
        float norm = juce::jlimit(0.0f, 1.0f, ((float)e.y - specY) / specH);
        float newNorm = 1.0f - norm;
        auto* p = (dragTarget == 0) ? threshParam : floorParam;
        if (p) p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, newNorm));
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        auto* p = (dragTarget == 0) ? threshParam : (dragTarget == 1 ? floorParam : nullptr);
        if (p) p->endChangeGesture();
        dragTarget = -1;
    }

    juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::UpDownResizeCursor; }

private:
    int dragTarget{ -1 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NRSpectrumDisplay)
};

// =============================================================================
// =============================================================================
//  PresetBrowserPanel — browser scrollable (favoris, tags, description)
// =============================================================================
class PresetBrowserPanel : public juce::Component
{
public:
    struct PresetMeta
    {
        bool        isFav    { false };
        juce::String tags    { "" };
        juce::String desc    { "" };
    };

    InvisibleFixAudioProcessor* processor{ nullptr };

    explicit PresetBrowserPanel(InvisibleFixAudioProcessor& p) : processor(&p)
    {
        searchBox.setTextToShowWhenEmpty("Search presets...", juce::Colours::grey);
        searchBox.setFont(juce::Font(juce::FontOptions(11.0f)));
        searchBox.setColour(juce::TextEditor::backgroundColourId, Theme::surfaceBlue);
        searchBox.setColour(juce::TextEditor::outlineColourId,    Theme::teal.withAlpha(0.3f));
        searchBox.setColour(juce::TextEditor::textColourId,       Theme::textDark);
        searchBox.onTextChange = [this] { rebuildFiltered(); repaint(); };
        addAndMakeVisible(searchBox);

        closeBtn.setButtonText("X");
        closeBtn.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
        closeBtn.setColour(juce::TextButton::textColourOffId,  Theme::textMuted);
        closeBtn.onClick = [this] { setVisible(false); };
        addAndMakeVisible(closeBtn);

        loadMetadata();
        rebuildFiltered();
    }

    void resized() override
    {
        searchBox.setBounds(8, 8, getWidth() - 44, 22);
        closeBtn .setBounds(getWidth() - 32, 8, 24, 22);
    }

    void paint(juce::Graphics& g) override
    {
        // Fond du panel
        g.setColour(Theme::surfaceMint2);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        g.setColour(Theme::teal.withAlpha(0.4f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.5f);

        // Scroll + liste
        int listY = 38;
        int rowH  = 52;
        int visH  = getHeight() - listY - 6;
        int totalH = (int)filteredIndices.size() * rowH;

        // Clip à la zone liste
        g.reduceClipRegion(0, listY, getWidth(), visH);
        g.setOrigin(0, listY - scrollOffset);

        for (int i = 0; i < (int)filteredIndices.size(); ++i)
        {
            int idx = filteredIndices[i];
            int ry  = i * rowH;
            bool isSelected = (idx == selectedIdx);
            bool isLoaded   = (idx == processor->currentPresetIndex);

            // Fond ligne
            juce::Colour rowBg = isLoaded   ? Theme::teal.withAlpha(0.18f)
                               : isSelected ? Theme::accent.withAlpha(0.10f)
                               :              juce::Colours::transparentBlack;
            g.setColour(rowBg);
            g.fillRoundedRectangle(6.0f, (float)ry + 2.0f, (float)getWidth() - 12.0f, (float)(rowH - 4), 5.0f);

            bool isFactory = processor->presetIsFactory[idx];
            auto& meta = getMeta(idx);

            // ── Étoile favoris ─────────────────────────────────────────
            g.setFont(juce::Font(juce::FontOptions(14.0f)));
            g.setColour(meta.isFav ? Theme::gold : Theme::textPrimary);
            g.drawText(meta.isFav ? "\xe2\x98\x85" : "\xe2\x98\x86",
                       8, ry + 6, 20, 20, juce::Justification::centred);

            // ── Nom preset ─────────────────────────────────────────────
            g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
            g.setColour(isLoaded ? Theme::teal : Theme::textDark);
            g.drawText(processor->presetNames[idx], 30, ry + 6, getWidth() - 80, 16,
                       juce::Justification::centredLeft);

            // ── Badge FACTORY / USER ────────────────────────────────────
            juce::Colour bc = isFactory ? Theme::teal : Theme::blueAccent;
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(bc.withAlpha(0.75f));
            g.drawText(isFactory ? "FACTORY" : "USER",
                       getWidth() - 60, ry + 6, 52, 14, juce::Justification::centredRight);

            // ── Tags ────────────────────────────────────────────────────
            if (meta.tags.isNotEmpty())
            {
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(Theme::accent.withAlpha(0.80f));
                g.drawText(meta.tags, 30, ry + 24, getWidth() - 90, 12,
                           juce::Justification::centredLeft);
            }

            // ── Description ─────────────────────────────────────────────
            if (meta.desc.isNotEmpty())
            {
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(Theme::textMuted);
                g.drawText(meta.desc, 30, ry + 36, getWidth() - 36, 12,
                           juce::Justification::centredLeft);
            }

            // Séparateur
            g.setColour(Theme::textMuted.withAlpha(0.08f));
            g.fillRect(8, ry + rowH - 2, getWidth() - 16, 1);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.y < 38) return;
        int rowH = 52;
        int relY = e.y - 38 + scrollOffset;
        int row  = relY / rowH;
        if (row >= 0 && row < (int)filteredIndices.size())
        {
            int idx = filteredIndices[row];
            // Clic étoile
            if (e.x < 30)
            {
                getMeta(idx).isFav = !getMeta(idx).isFav;
                saveMetadata();
                repaint();
                return;
            }
            selectedIdx = idx;

            // Double clic = charger le preset
            if (e.getNumberOfClicks() >= 2)
                loadSelected();

            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        int rowH    = 52;
        int maxScroll = juce::jmax(0, (int)filteredIndices.size() * rowH - (getHeight() - 38));
        scrollOffset = juce::jlimit(0, maxScroll, scrollOffset - (int)(w.deltaY * 60.0f));
        repaint();
    }

    void loadSelected()
    {
        if (selectedIdx >= 0 && selectedIdx < (int)processor->presetNames.size())
        {
            processor->loadPreset(selectedIdx);
            if (auto* ed = findParentComponentOfClass<InvisibleFixAudioProcessorEditor>())
                ed->updatePresetBar();
            repaint();
        }
    }

    // Édition inline — appelé depuis un popup externe (tags / desc)
    void editTagsForSelected()
    {
        if (selectedIdx < 0) return;
        auto& meta = getMeta(selectedIdx);
        auto alertBox = std::make_shared<juce::AlertWindow>(
            "Edit tags for \"" + processor->presetNames[selectedIdx] + "\"",
            "", juce::AlertWindow::NoIcon);
        alertBox->addTextEditor("tags", meta.tags, "Tags (comma-separated):");
        alertBox->addTextEditor("desc", meta.desc, "Description:");
        alertBox->addButton("OK", 1);
        alertBox->addButton("Cancel", 0);
        alertBox->enterModalState(true,
            juce::ModalCallbackFunction::create([this, alertBox](int r) {
                if (r == 1 && selectedIdx >= 0)
                {
                    getMeta(selectedIdx).tags = alertBox->getTextEditorContents("tags").trim();
                    getMeta(selectedIdx).desc = alertBox->getTextEditorContents("desc").trim();
                    saveMetadata();
                    rebuildFiltered();
                    repaint();
                }
            }), true);
    }

    void rebuildFiltered()
    {
        juce::String query = searchBox.getText().toLowerCase().trim();
        filteredIndices.clear();
        for (int i = 0; i < (int)processor->presetNames.size(); ++i)
        {
            auto& meta = getMeta(i);
            if (query.isEmpty()
                || processor->presetNames[i].toLowerCase().contains(query)
                || meta.tags.toLowerCase().contains(query)
                || meta.desc.toLowerCase().contains(query))
            {
                filteredIndices.push_back(i);
            }
        }
        scrollOffset = 0;
        repaint();
    }

    PresetMeta& getMeta(int idx)
    {
        while ((int)metaList.size() <= idx) metaList.emplace_back();
        return metaList[idx];
    }

private:
    juce::TextEditor searchBox;
    juce::TextButton closeBtn;
    std::vector<int>         filteredIndices;
    std::vector<PresetMeta>  metaList;
    int  selectedIdx   { -1 };
    int  scrollOffset  { 0 };

    juce::File metaFile() const
    {
        return processor->getUserPresetsDir().getChildFile("browser_metadata.xml");
    }

    void loadMetadata()
    {
        auto f = metaFile();
        if (!f.existsAsFile()) return;
        if (auto xml = juce::XmlDocument::parse(f))
        {
            int i = 0;
            for (auto* el : xml->getChildIterator())
            {
                auto& m = getMeta(i++);
                m.isFav = el->getBoolAttribute("fav", false);
                m.tags  = el->getStringAttribute("tags", "");
                m.desc  = el->getStringAttribute("desc", "");
            }
        }
    }

    void saveMetadata()
    {
        juce::XmlElement root("presets");
        for (int i = 0; i < (int)metaList.size(); ++i)
        {
            auto* el = root.createNewChildElement("preset");
            el->setAttribute("fav",  metaList[i].isFav ? 1 : 0);
            el->setAttribute("tags", metaList[i].tags);
            el->setAttribute("desc", metaList[i].desc);
        }
        metaFile().getParentDirectory().createDirectory();
        root.writeTo(metaFile());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowserPanel)
};

// =============================================================================
//  DeRevCanvas — Visualisation dual-axis interactive pour le De-Reverb 3 bandes
//  ↔ drag sur le handle blanc = seuil temporel (quand commence le traitement)
//  ↕ drag dans la zone rouge = profondeur (combien est retiré)
//  Timer 30 Hz → signal live par bande (enveloppe fast)
// =============================================================================
class DeRevCanvas : public juce::Component, private juce::Timer
{
public:
    DeRevCanvas(InvisibleFixAudioProcessor& p, juce::AudioProcessorValueTreeState& a)
        : proc(p), apvts(a)
    {
        startTimerHz(30);
    }
    ~DeRevCanvas() override { stopTimer(); }

    void setLinked(bool l) { linked = l; repaint(); }
    bool isLinked() const  { return linked; }

    void paint(juce::Graphics& g) override
    {
        const int W = getWidth(), H = getHeight();
        const int BH = H / 3;

        // Paramètres courants (threads audio + UI partagent via atomics / param cache)
        float thresh[3], depth[3];
        thresh[0] = *apvts.getRawParameterValue("deRevThreshHF");
        thresh[1] = *apvts.getRawParameterValue("deRevThreshMF");
        thresh[2] = *apvts.getRawParameterValue("deRevThreshLF");
        depth[0]  = *apvts.getRawParameterValue("deRevBandHF");
        depth[1]  = *apvts.getRawParameterValue("deRevBandMF");
        depth[2]  = *apvts.getRawParameterValue("deRevBandLF");

        // Couleurs : HF=bleu, MF=teal, LF=ambre
        const juce::Colour cols[3] = {
            Theme::accent,
            Theme::teal,
            Theme::amberWarn
        };
        const char* lbls[3] = { "HF", "MF", "LF" };

        // Clip arrondi (6px) — fond transparent, hérite du panneau parent
        {
            juce::Path roundClip;
            roundClip.addRoundedRectangle(0.f, 0.f, (float)W, (float)H, 6.f);
            g.reduceClipRegion(roundClip);
        }

        // Grille verticale (subtile sur fond clair)
        g.setColour(Theme::black.withAlpha(0.06f));
        for (int qi = 1; qi < 4; ++qi)
            g.fillRect((float)(W * qi / 4), 0.f, 1.f, (float)H);

        for (int i = 0; i < 3; ++i)
        {
            const int by = i * BH;
            const float T = juce::jmax(0.01f, thresh[i]);
            const int tx = juce::roundToInt(T * (float)W);
            const juce::Colour& col = cols[i];
            const float dep = depth[i];

            // Heatmap : modèle de décroissance exponentielle conceptuelle
            for (int px = 0; px < W; ++px)
            {
                float tNorm = (float)px / (float)W;
                float approxRatio = std::exp(-tNorm * 3.5f);
                if (px < tx)
                {
                    // Zone conservée — teinte bande (plus visible sur fond clair)
                    g.setColour(col.withAlpha(approxRatio * 0.45f));
                    g.fillRect(px, by + 1, 1, BH - 2);
                }
                else
                {
                    // Zone retirée
                    float tailD = (approxRatio < T) ? juce::jmax(0.f, 1.f - approxRatio / T) : 0.f;
                    g.setColour(col.withAlpha(approxRatio * 0.15f));
                    g.fillRect(px, by + 1, 1, BH - 2);
                    g.setColour(Theme::warmRed.withAlpha(dep * tailD * approxRatio * 0.82f));
                    g.fillRect(px, by + 1, 1, BH - 2);
                }
            }

            // Waveform live : historique de l'enveloppe rapide dans la zone conservée
            const int drawW = juce::jmin(tx - 4, HIST_LEN);
            if (drawW > 6)
            {
                const float startX = (float)(tx - drawW - 2);
                const float cy     = (float)by + (float)BH * 0.5f;

                juce::Path fill, line;
                bool first = true;
                for (int p = 0; p < drawW; ++p)
                {
                    int   hIdx = (histWriteIdx - drawW + p + HIST_LEN) % HIST_LEN;
                    float hVal = bandHistory[i][hIdx];
                    float x    = startX + (float)p;
                    float amp  = hVal * ((float)BH * 0.38f);

                    if (first) { line.startNewSubPath(x, cy - amp); fill.startNewSubPath(x, cy); first = false; }
                    line.lineTo(x, cy - amp);
                    fill.lineTo(x, cy - amp);
                }
                fill.lineTo(startX + (float)drawW, cy);
                fill.closeSubPath();

                g.setColour(col.withAlpha(0.30f));
                g.fillPath(fill);
                g.setColour(col.withAlpha(0.90f));
                g.strokePath(line, juce::PathStrokeType(1.1f));
            }

            // Barre GR live dans la zone rouge (indique la réduction active)
            const float grR = 1.0f - bandGRSmooth[i];
            if (grR > 0.02f && tx < W - 8)
            {
                const float grX  = (float)tx + (float)(W - tx) * 0.10f;
                const float grBarH = juce::jmin(grR * dep * (float)(BH - 6), (float)(BH - 6));
                const float gcy    = (float)by + (float)BH * 0.5f;
                g.setColour(Theme::warmRed.withAlpha(0.75f));
                g.fillRoundedRectangle(grX - 2.f, gcy - grBarH * 0.5f, 4.f, grBarH, 2.f);
            }

            // Label profondeur (%) + flèche dans zone rouge
            const float zW = (float)(W - tx);
            if (zW > 40.f)
            {
                const float cxL = (float)tx + zW * 0.42f;
                g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
                g.setColour(Theme::warmRed.withAlpha(0.5f));
                g.drawText(juce::String((int)(dep * 100.f)) + "%",
                           juce::Rectangle<float>(cxL - 18.f, (float)by + (float)BH * 0.5f - 3.f, 36.f, 12.f),
                           juce::Justification::centred);
                if (BH > 36)
                {
                    g.setColour(Theme::warmRed.withAlpha(0.3f));
                    g.drawText(juce::CharPointer_UTF8("\xe2\x86\x95"),
                               juce::Rectangle<float>(cxL - 8.f, (float)by + (float)BH * 0.5f - 15.f, 16.f, 12.f),
                               juce::Justification::centred);
                }
            }

            // Séparateur de bande
            if (i < 2)
            {
                g.setColour(Theme::black.withAlpha(0.09f));
                g.fillRect(0, by + BH - 1, W, 2);
            }

            // Ligne threshold (pointillés sombres)
            const float hcy = (float)by + (float)BH * 0.5f;
            g.setColour(Theme::gridDark.withAlpha(0.40f));
            for (float yp = (float)by + 3.f; yp < (float)(by + BH) - 3.f; yp += 5.f)
                g.fillRect((float)tx - 0.75f, yp, 1.5f, 3.0f);

            // Handle circulaire
            g.setColour(col.withAlpha(0.12f));
            g.fillEllipse((float)tx - 8.f, hcy - 8.f, 16.f, 16.f);
            g.setColour(Theme::gridDark.withAlpha(linked ? 0.70f : 0.45f));
            g.drawEllipse((float)tx - 8.f, hcy - 8.f, 16.f, 16.f, linked ? 2.0f : 1.5f);
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.setColour(Theme::gridDark.withAlpha(0.60f));
            g.drawText(juce::CharPointer_UTF8("\xe2\x86\x94"),
                       juce::Rectangle<float>((float)tx - 8.f, hcy - 6.f, 16.f, 12.f),
                       juce::Justification::centred);

            // Temps en ms sous le handle
            const float lblX = (tx > (int)(W * 0.65f)) ? (float)tx - 28.f : (float)tx + 5.f;
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(Theme::gridDark.withAlpha(0.38f));
            g.drawText(juce::String((int)(T * 1000.f)) + "ms",
                       juce::Rectangle<float>(lblX, (float)(by + BH) - 13.f, 30.f, 10.f),
                       juce::Justification::centredLeft);

            // Label de bande (en haut à gauche)
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(col.withAlpha(0.90f));
            g.drawText(lbls[i],
                       juce::Rectangle<float>(4.f, (float)by + 3.f, 20.f, 12.f),
                       juce::Justification::centredLeft);
        }

        // Axe temporel horizontal — graduations en ms
        g.setFont(juce::Font(juce::FontOptions(7.5f)));
        g.setColour(Theme::gridDark.withAlpha(0.35f));
        const float msMarks[] = { 0.f, 250.f, 500.f, 750.f, 1000.f };
        const char* msTxt[]   = { "0 ms", "250", "500", "750", "1000 ms" };
        for (int mi = 0; mi < 5; ++mi)
        {
            float tx2 = (msMarks[mi] / 1000.f) * (float)W;
            juce::Justification just = (mi == 0) ? juce::Justification::centredLeft
                                     : (mi == 4) ? juce::Justification::centredRight
                                     : juce::Justification::centred;
            g.drawText(msTxt[mi], juce::Rectangle<float>(tx2 - 18.f, (float)H - 11.f, 36.f, 10.f), just);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int W = getWidth(), H = getHeight();
        const int BH = H / 3;
        const float mx = (float)e.x, my = (float)e.y;

        dragStartX = mx;
        dragStartY = my;
        dragMode  = DragMode::None;
        dragBand  = -1;

        // Snapshot des valeurs actuelles
        for (int i = 0; i < 3; ++i)
        {
            dragStartThresh[i] = *apvts.getRawParameterValue(threshId(i));
            dragStartDepth[i]  = *apvts.getRawParameterValue(depthId(i));
        }

        // Détection : handle threshold ou zone rouge
        for (int i = 0; i < 3; ++i)
        {
            const float T  = dragStartThresh[i];
            const float tx = T * (float)W;
            const float cy = (float)(i * BH) + (float)BH * 0.5f;

            if (std::hypot(mx - tx, my - cy) < 12.f)
            {
                dragMode = DragMode::Threshold;
                dragBand = i;
                break;
            }
            if (mx > tx + 12.f && my > (float)(i * BH) && my < (float)((i + 1) * BH))
            {
                dragMode = DragMode::Depth;
                dragBand = i;
                break;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragMode == DragMode::None || dragBand < 0) return;
        const float W = (float)getWidth();
        const float H = (float)getHeight();
        const float dx = ((float)e.x - dragStartX) / W;
        const float dy = (dragStartY - (float)e.y) / H;

        auto setParam = [&](const char* id, float raw, float lo, float hi) {
            float clamped = juce::jlimit(lo, hi, raw);
            if (auto* param = apvts.getParameter(id))
                param->setValueNotifyingHost(param->convertTo0to1(clamped));
        };

        if (dragMode == DragMode::Threshold)
        {
            for (int b = 0; b < 3; ++b)
            {
                if (!linked && b != dragBand) continue;
                setParam(threshId(b), dragStartThresh[b] + dx, 0.02f, 1.0f);
            }
        }
        else if (dragMode == DragMode::Depth)
        {
            for (int b = 0; b < 3; ++b)
            {
                if (!linked && b != dragBand) continue;
                setParam(depthId(b), dragStartDepth[b] + dy * 2.0f, 0.0f, 1.0f);
            }
        }
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { dragMode = DragMode::None; }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int W = getWidth(), H = getHeight();
        const int BH = H / 3;
        const float mx = (float)e.x, my = (float)e.y;

        static const char* bandName[3] = { "HF", "MF", "LF" };
        static const char* threshDesc[3] = {
            "DE-REV HF SEUIL — drag gauche/droite. Separe la queue de reverb (rouge) du signal conserve. Aigus >4 kHz",
            "DE-REV MF SEUIL — drag gauche/droite. Zone vocale principale 500-4000 Hz",
            "DE-REV LF SEUIL — drag gauche/droite. Basses frequences <500 Hz — eviter de trop reduire"
        };
        static const char* depthDesc[3] = {
            "DE-REV HF PROFONDEUR — drag haut/bas. Intensite de la reduction sur les aigus (>4 kHz)",
            "DE-REV MF PROFONDEUR — drag haut/bas. Intensite de la reduction sur les mediums (500-4000 Hz)",
            "DE-REV LF PROFONDEUR — drag haut/bas. Intensite de la reduction sur les basses (<500 Hz)"
        };

        for (int i = 0; i < 3; ++i)
        {
            float T  = *apvts.getRawParameterValue(threshId(i));
            float tx = T * (float)W;
            float cy = (float)(i * BH) + (float)BH * 0.5f;
            if (std::hypot(mx - tx, my - cy) < 12.f)
            {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                if (onTooltipChange) onTooltipChange(
                    juce::String("DE-REV ") + bandName[i] + " SEUIL", threshDesc[i]);
                return;
            }
            if (mx > tx + 12.f && my > (float)(i * BH) && my < (float)((i + 1) * BH))
            {
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
                if (onTooltipChange) onTooltipChange(
                    juce::String("DE-REV ") + bandName[i] + " PROFONDEUR", depthDesc[i]);
                return;
            }
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
        if (onTooltipChange) onTooltipChange({}, {});
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (onTooltipChange) onTooltipChange({}, {});
    }

    std::function<void(const juce::String&, const juce::String&)> onTooltipChange;

private:
    void timerCallback() override
    {
        histWriteIdx = (histWriteIdx + 1) % HIST_LEN;
        // Mapping canvas (i=0=HF, i=1=MF, i=2=LF) → proc band index (0=LF,1=MF,2=HF)
        const int procIdx[3] = { 2, 1, 0 };
        for (int i = 0; i < 3; ++i)
        {
            float energy = proc.revBandLiveEnergy[procIdx[i]].load();
            // Conversion log pour visualisation perceptuelle
            float dB   = 20.f * std::log10(energy + 1e-6f);
            float norm = juce::jlimit(0.f, 1.f, (dB + 60.f) / 60.f);
            bandHistory[i][histWriteIdx] = norm;

            float gr = proc.revBandLiveGR[procIdx[i]].load();
            bandGRSmooth[i] = bandGRSmooth[i] * 0.80f + gr * 0.20f;
        }
        repaint();
    }

    const char* threshId(int i) const
    {
        static const char* ids[] = { "deRevThreshHF", "deRevThreshMF", "deRevThreshLF" };
        return ids[i];
    }
    const char* depthId(int i) const
    {
        static const char* ids[] = { "deRevBandHF", "deRevBandMF", "deRevBandLF" };
        return ids[i];
    }

    InvisibleFixAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    static constexpr int HIST_LEN = 60;
    float bandHistory[3][HIST_LEN]{};
    int   histWriteIdx{ 0 };
    float bandGRSmooth[3]{ 1.f, 1.f, 1.f };

    enum class DragMode { None, Threshold, Depth };
    DragMode dragMode{ DragMode::None };
    int   dragBand{ -1 };
    float dragStartX{}, dragStartY{};
    float dragStartThresh[3]{};
    float dragStartDepth[3]{};
    bool  linked{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeRevCanvas)
};

// =============================================================================
//  InvisibleFixAudioProcessorEditor — CONSTRUCTEUR
// =============================================================================
InvisibleFixAudioProcessorEditor::InvisibleFixAudioProcessorEditor(InvisibleFixAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    setVisible(true);

    // --- 0. ALLOCATION DU VISUALISEUR ---
    roomVisualizer = std::make_unique<RoomVisualizer>(audioProcessor);
    addChildComponent(roomVisualizer.get());

    // --- 1. CONFIGURATION DES POTENTIOMÈTRES FAÇADE PRINCIPALE ---
    setupModernSlider(cleanSlider,  "CLEAN");
    setupModernSlider(roomSlider,   "ROOM");
    setupModernSlider(reverbSlider, "DE-REV");
    setupModernSlider(deEssSlider,  "DE-ESS");
    setupModernSlider(airSlider,    "AIR");

    // Master Gain : boite verticale draggable (sans mini-slider horizontal)
    masterGainSlider.setSliderStyle(juce::Slider::SliderStyle::LinearBarVertical);
    masterGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterGainSlider.setWantsKeyboardFocus(false);
    addAndMakeVisible(masterGainSlider);

    masterGainLabel.setText("MASTER GAIN", juce::dontSendNotification);
    masterGainLabel.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
    masterGainLabel.setJustificationType(juce::Justification::centredRight);
    masterGainLabel.setColour(juce::Label::textColourId, Theme::textDark);
    addAndMakeVisible(masterGainLabel);

    masterGainSlider.setColour(juce::Slider::trackColourId,      Theme::teal.withAlpha(0.40f));
    masterGainSlider.setColour(juce::Slider::backgroundColourId, Theme::textPrimary.withAlpha(0.45f));

    // Descriptions légères sous les knobs façade
    auto applyFacadeDesc = [](ModernSlider& ms, const juce::String& desc) {
        ms.label.setText(desc, juce::dontSendNotification);
        ms.label.setFont(juce::Font(juce::FontOptions(10.5f)));
        ms.label.setJustificationType(juce::Justification::centred);
        ms.label.setColour(juce::Label::textColourId, Theme::textMuted);
    };
    applyFacadeDesc(cleanSlider,  "noise reduction & gate");
    applyFacadeDesc(roomSlider,   "ROOM NOTCH");
    applyFacadeDesc(reverbSlider, "reverb reducer");
    applyFacadeDesc(deEssSlider,  "sibilance reducer");
    applyFacadeDesc(airSlider,    "shelf & exciter");

    // --- 2. CONFIGURATION DES BOUTONS INTERACTIFS DISCRETS ---
    // CORRECTION DELTA : Encodage UTF-8 universel sécurisé via code hexadécimal
    deltaBtn.button.setButtonText(juce::String::fromUTF8("\xCE\x94"));
    deltaBtn.button.setClickingTogglesState(true);
    deltaBtn.button.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    deltaBtn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    deltaBtn.button.setColour(juce::TextButton::textColourOffId, Theme::textMuted);
    deltaBtn.button.setColour(juce::TextButton::textColourOnId,  Theme::accent);
    deltaBtn.button.setLookAndFeel(&deltaButtonLAF);
    addAndMakeVisible(deltaBtn.button);

    auto setupDeltaBtn = [&](CustomButton& btn) {
        btn.button.setButtonText(juce::String::fromUTF8("\xCE\x94"));
        btn.button.setClickingTogglesState(true);
        btn.button.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::textColourOffId, Theme::textMuted);
        btn.button.setColour(juce::TextButton::textColourOnId,  Theme::accent);
        btn.button.setLookAndFeel(&deltaButtonLAF);
        addAndMakeVisible(btn.button);
    };
    setupDeltaBtn(roomDeltaBtn);
    setupDeltaBtn(reverbDeltaBtn);
    setupDeltaBtn(deEssDeltaBtn);
    setupDeltaBtn(airDeltaBtn);

    // Bouton De-Ess Mode
    deEssModeBtn.button.setClickingTogglesState(true);
    deEssModeBtn.button.setColour(juce::TextButton::buttonColourId, Theme::white);
    deEssModeBtn.button.setColour(juce::TextButton::buttonOnColourId, Theme::cyanBright.withAlpha(0.12f));
    deEssModeBtn.button.setColour(juce::TextButton::textColourOffId, Theme::slateDark.withAlpha(0.6f));
    deEssModeBtn.button.setColour(juce::TextButton::textColourOnId, Theme::cyanDeep);

    deEssModeBtn.button.onClick = [this]() {
        bool isBell = deEssModeBtn.button.getToggleState();
        deEssModeBtn.button.setButtonText(isBell ? "BELL" : "SHELF");
        };
    {
        bool initialBell = *audioProcessor.apvts.getRawParameterValue("deEssFilterMode") > 0.5f;
        deEssModeBtn.button.setButtonText(initialBell ? "BELL" : "SHELF");
    }
    addAndMakeVisible(deEssModeBtn.button);

    // Boutons Bypass (5 modules) — clignotent rouge quand actifs
    auto setupBypassBtn = [&](CustomButton& btn) {
        btn.button.setClickingTogglesState(true);
        btn.button.setButtonText("BYP");
        btn.button.setLookAndFeel(&bypassBtnLAF);
        addAndMakeVisible(btn.button);
    };
    setupBypassBtn(cleanBypassBtn);
    setupBypassBtn(roomBypassBtn);
    setupBypassBtn(reverbBypassBtn);
    setupBypassBtn(deEssBypassBtn);
    setupBypassBtn(airBypassBtn);

    // Callbacks bypass → griser le module immédiatement
    auto makeBypassCb = [this](CustomButton& btn, ModernSlider& ms) {
        btn.button.onStateChange = [this, &btn, &ms]() {
            float a = btn.button.getToggleState() ? 0.35f : 1.0f;
            ms.slider.setAlpha(a);
            ms.label.setAlpha(a);
            repaint();
        };
    };
    makeBypassCb(cleanBypassBtn,  cleanSlider);
    makeBypassCb(roomBypassBtn,   roomSlider);
    makeBypassCb(reverbBypassBtn, reverbSlider);
    makeBypassCb(deEssBypassBtn,  deEssSlider);
    makeBypassCb(airBypassBtn,    airSlider);

    // --- 3. CONFIGURATION DES TIROIRS AVANCÉS ---
    setupModernSlider(advRoomFreqSlider, "Notch Freq");
    setupModernSlider(advRoomQSlider, "Notch Q");
    setupModernSlider(advRoomDepthBoostSlider, "Depth Boost");
    setupModernSlider(advDeEssFreqSlider,    "De-Ess Freq");
    setupModernSlider(advDeEssThreshSlider,  "Thresh dB");
    setupModernSlider(advDeEssIntenSlider,   "Intensity");
    // Intensity → slider vertical pour De-Ess (moins encombrant qu'un knob)
    advDeEssIntenSlider.slider.setSliderStyle(juce::Slider::LinearVertical);
    advDeEssIntenSlider.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 14);
    advDeEssIntenSlider.slider.setColour(juce::Slider::trackColourId,       Theme::textPrimary);
    advDeEssIntenSlider.slider.setColour(juce::Slider::thumbColourId,       Theme::accent);
    advDeEssIntenSlider.slider.setColour(juce::Slider::backgroundColourId,  Theme::surfaceLight);
    advDeEssIntenSlider.slider.setColour(juce::Slider::textBoxTextColourId, Theme::textDark);
    advDeEssIntenSlider.slider.setLookAndFeel(nullptr); // retirer ArcFinLAF, utiliser V4 par défaut
    setupModernSlider(advDeEssAtkSlider,     "Attack");
    setupModernSlider(advDeEssRelSlider,     "Release");
    setupModernSlider(advDeEssDetectSlider,  "Q");
    advDeEssDetectSlider.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    advDeEssDetectSlider.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 14);
    advDeEssDetectSlider.slider.setLookAndFeel(&airFreqLAF);
    setupModernSlider(advGateFloorSlider,    "Gate Floor");
    setupModernSlider(advCleanRelSlider,     "Gate Release");

    // Gate advanced
    setupModernSlider(advGateThresholdSlider, "Gate Thresh");
    setupModernSlider(advGateAttackSlider,    "Gate Attack");
    setupModernSlider(advGateHoldSlider,      "Gate Hold");
    setupModernSlider(plosiveProtectSlider,   "PLOSIVE");
    plosiveProtectSlider.slider.setName("plosiveProtectSlider");

    // Air drive
    setupModernSlider(advAirDriveSlider, "Heat");

    // Air saturation mode — 4 radio buttons
    auto setupSatBtn = [&](juce::TextButton& btn, const juce::String& label, int mode) {
        btn.setButtonText(label);
        btn.setClickingTogglesState(false);
        btn.setWantsKeyboardFocus(false);
        btn.setColour(juce::TextButton::buttonColourId,     juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::buttonOnColourId,   Theme::accentDark.withAlpha(0.25f));
        btn.setColour(juce::TextButton::textColourOffId,    Theme::textPrimary);
        btn.setColour(juce::TextButton::textColourOnId,     juce::Colours::white);
        btn.onClick = [this, mode]() {
            audioProcessor.undoManager.beginNewTransaction();
            if (auto* p = dynamic_cast<juce::AudioParameterInt*>(
                    audioProcessor.apvts.getParameter("airSatMode")))
                *p = mode;
        };
        addChildComponent(btn);
    };
    setupSatBtn(airSatTriodeBtn, "TRIODE",   0);
    setupSatBtn(airSatClassABtn, "CLASS-A",  1);
    setupSatBtn(airSatTapeBtn,   "TAPE",     2);
    setupSatBtn(airSatIronBtn,   "IRON",     3);
    airSatTriodeBtn.setName("airSatTriodeBtn");
    airSatClassABtn.setName("airSatClassABtn");
    airSatTapeBtn  .setName("airSatTapeBtn");
    airSatIronBtn  .setName("airSatIronBtn");

    // Air Shape — slider horizontal bipolaire
    advAirShapeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    advAirShapeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    advAirShapeSlider.setRange(-1.0, 1.0, 0.01);
    advAirShapeSlider.setValue(0.0, juce::dontSendNotification);
    advAirShapeSlider.setDoubleClickReturnValue(true, 0.0);
    advAirShapeSlider.setWantsKeyboardFocus(false);
    advAirShapeSlider.setName("advAirShapeSlider");
    advAirShapeSlider.setColour(juce::Slider::trackColourId,      Theme::accent.withAlpha(0.55f));
    advAirShapeSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    addChildComponent(advAirShapeSlider);

    advAirShapeLabel.setText("TRANSIENT SHAPE", juce::dontSendNotification);
    advAirShapeLabel.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
    advAirShapeLabel.setJustificationType(juce::Justification::centred);
    advAirShapeLabel.setColour(juce::Label::textColourId, Theme::textMuted);
    addChildComponent(advAirShapeLabel);

    // Curseur AIR FREQ (juce::Slider direct) — style personnalisé AirFreqSliderLAF
    advAirFreqSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    advAirFreqSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    advAirFreqSlider.setLookAndFeel(&airFreqLAF);
    addChildComponent(advAirFreqSlider);

    airCharacterLabel.setText("CHARACTER: PRESENCE", juce::dontSendNotification);
    airCharacterLabel.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    airCharacterLabel.setJustificationType(juce::Justification::centredLeft);
    airCharacterLabel.setColour(juce::Label::textColourId, juce::Colour((juce::uint8)44, (juce::uint8)62, (juce::uint8)80).withAlpha(0.4f));
    addChildComponent(airCharacterLabel);

    advAirFreqSlider.onValueChange = [this]() { repaint(); };

    // --- 4. ATTACHEMENTS APVTS ---
    cleanAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "clean", cleanSlider.slider);
    deEssAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deess", deEssSlider.slider);
    roomAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "room", roomSlider.slider);
    reverbAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "reverb", reverbSlider.slider);
    airAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "air", airSlider.slider);

    masterGainAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "outputGain", masterGainSlider);

    // Bypass attachments
    cleanBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "cleanBypass", cleanBypassBtn.button);
    roomBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "roomBypass", roomBypassBtn.button);
    reverbBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "reverbBypass", reverbBypassBtn.button);
    deEssBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "deessBypass", deEssBypassBtn.button);
    airBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "airBypass", airBypassBtn.button);

    // Delta attachments
    deltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "cleanDelta", deltaBtn.button);
    roomDeltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "roomDelta", roomDeltaBtn.button);
    reverbDeltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "reverbDelta", reverbDeltaBtn.button);
    deEssDeltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "deessDelta", deEssDeltaBtn.button);
    airDeltaAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "airDelta", airDeltaBtn.button);

    deEssModeAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "deEssFilterMode", deEssModeBtn.button);

    advRoomFreqAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "roomNotchFreq", advRoomFreqSlider.slider);
    advRoomQAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "roomNotchQ",    advRoomQSlider.slider);
    advRoomDepthBoostAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "roomDepthBoost", advRoomDepthBoostSlider.slider);

    advDeEssFreqAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssFreq",       advDeEssFreqSlider.slider);
    advDeEssThreshAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssThreshold",  advDeEssThreshSlider.slider);
    advDeEssIntenAttach   = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssIntensity",  advDeEssIntenSlider.slider);
    advDeEssAtkAttach     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssAttack",     advDeEssAtkSlider.slider);
    advDeEssRelAttach     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssRelease",    advDeEssRelSlider.slider);
    advDeEssDetectAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deEssDetectRatio", advDeEssDetectSlider.slider);
    advGateFloorAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanGateFloor",  advGateFloorSlider.slider);
    advCleanRelAttach     = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanRelease",    advCleanRelSlider.slider);

    advAirFreqAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "airFreq", advAirFreqSlider);

    // Gate advanced attachments
    advGateThresholdAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanGateThreshold", advGateThresholdSlider.slider);
    advGateAttackAttach    = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanGateAttack",    advGateAttackSlider.slider);
    advGateHoldAttach      = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanGateHold",      advGateHoldSlider.slider);
    plosiveProtectAttach   = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cleanPlosiveProtect", plosiveProtectSlider.slider);

    // Air drive
    advAirDriveAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "airDrive", advAirDriveSlider.slider);
    advAirShapeAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "airShape", advAirShapeSlider);

    // NR — Noise Reduction (Cedar-aligned labels)
    setupModernSlider(nrThreshSlider, "Sensitivity");
    setupModernSlider(nrRatioSlider,  "Curve");
    setupModernSlider(nrFloorSlider,  "Reduction");
    setupModernSlider(nrSmoothSlider, "Speed");
    setupModernSlider(nrArtifactBlendSlider, "Blend");
    nrThreshAttach       = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrThreshold",    nrThreshSlider.slider);
    nrRatioAttach        = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrRatio",        nrRatioSlider.slider);
    nrFloorAttach        = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrFloor",        nrFloorSlider.slider);
    nrSmoothAttach       = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrSmooth",       nrSmoothSlider.slider);
    nrArtifactBlendAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrArtifactBlend", nrArtifactBlendSlider.slider);

    // Curve and Speed: horizontal sliders (same style as Q slider in De-Ess)
    for (auto* ms : { &nrRatioSlider, &nrSmoothSlider })
    {
        ms->slider.setSliderStyle(juce::Slider::LinearHorizontal);
        ms->slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 14);
        ms->slider.setLookAndFeel(&airFreqLAF);
    }

    // Façade knobs: textbox 94px large, 20px tall (Arc Fin value display)
    for (auto* ms : { &cleanSlider, &roomSlider, &reverbSlider, &deEssSlider, &airSlider })
        ms->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 94, 20);

    // ── Nouveaux composants graphiques UX ────────────────────────────────────
    auto getParam = [&](const juce::String& id) -> juce::RangedAudioParameter* {
        return dynamic_cast<juce::RangedAudioParameter*>(audioProcessor.apvts.getParameter(id));
    };

    // NR Spectrum Display (FFT + draggable thresh/floor)
    {
        auto* nrSpec = new NRSpectrumDisplay();
        nrSpec->processor   = &audioProcessor;
        nrSpec->threshParam = dynamic_cast<juce::RangedAudioParameter*>(audioProcessor.apvts.getParameter("nrThreshold"));
        nrSpec->floorParam  = dynamic_cast<juce::RangedAudioParameter*>(audioProcessor.apvts.getParameter("nrFloor"));
        nrBandDisplay.reset(nrSpec);
    }
    addChildComponent(nrBandDisplay.get());

    // Bouton LEARN — snapshot plancher de bruit
    nrLearnBtn.setButtonText("LEARN");
    nrLearnBtn.setColour(juce::TextButton::buttonColourId,    Theme::panelDark);
    nrLearnBtn.setColour(juce::TextButton::buttonOnColourId,  Theme::teal);
    nrLearnBtn.setColour(juce::TextButton::textColourOffId,   Theme::textPrimary);
    nrLearnBtn.setColour(juce::TextButton::textColourOnId,    Theme::ink);
    nrLearnBtn.setToggleable(false);
    nrLearnBtn.onClick = [this]()
    {
        if (!nrLearnBtnActive)
        {
            audioProcessor.nrSnapshotRequest.store(true);
            nrLearnBtnActive = true;
        }
        else
        {
            audioProcessor.nrSnapshotRelease.store(true);
            nrLearnBtnActive = false;
        }
        nrLearnBtn.setColour(juce::TextButton::buttonColourId,
            nrLearnBtnActive ? Theme::teal : Theme::bgDeep);
        nrLearnBtn.setColour(juce::TextButton::textColourOffId,
            nrLearnBtnActive ? Theme::ink : Theme::textMuted);
        nrLearnBtn.repaint();
    };
    addChildComponent(nrLearnBtn);

    // Bouton AUTO gate — détecte la zone optimale et règle le threshold
    gateAutoBtn.setButtonText("AUTO");
    gateAutoBtn.setColour(juce::TextButton::buttonColourId,  Theme::panelDark);
    gateAutoBtn.setColour(juce::TextButton::buttonOnColourId,Theme::accent);
    gateAutoBtn.setColour(juce::TextButton::textColourOffId, Theme::textPrimary);
    gateAutoBtn.setColour(juce::TextButton::textColourOnId,  Theme::ink);
    gateAutoBtn.onClick = [this]()
    {
        if (!audioProcessor.gateAutoZoneValid) return;

        // Threshold = centre de la vallée (+2 dB de marge pour ne pas couper les débuts doux)
        float thresh = audioProcessor.gateAutoThreshDB + 2.0f;
        auto* paramT = audioProcessor.apvts.getParameter("cleanGateThreshold");
        if (paramT) {
            auto& r = paramT->getNormalisableRange();
            float norm = juce::jlimit(0.0f, 1.0f, (thresh - r.start) / (r.end - r.start));
            paramT->beginChangeGesture();
            paramT->setValueNotifyingHost(norm);
            paramT->endChangeGesture();
        }
        // Floor = centre vallée − 12 dB (atténuation confortable quand gate fermé)
        float floor = audioProcessor.gateAutoThreshDB - 12.0f;
        auto* paramF = audioProcessor.apvts.getParameter("cleanGateFloor");
        if (paramF) {
            auto& r = paramF->getNormalisableRange();
            float norm = juce::jlimit(0.0f, 1.0f, (floor - r.start) / (r.end - r.start));
            paramF->beginChangeGesture();
            paramF->setValueNotifyingHost(norm);
            paramF->endChangeGesture();
        }
    };
    addChildComponent(gateAutoBtn);

    // ── Bypass NR + Gate indépendants ────────────────────────────────────────
    auto setupBypassSmall = [&](juce::TextButton& btn, const juce::String& label) {
        btn.setButtonText(label);
        btn.setClickingTogglesState(true);
        btn.setWantsKeyboardFocus(false);
        btn.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOffId,  Theme::textMuted);
        btn.setColour(juce::TextButton::textColourOnId,   Theme::dangerSoft);
        addChildComponent(btn);
    };
    setupBypassSmall(nrBypassBtn,   "NR BYP");
    setupBypassSmall(gateBypassBtn, "GATE BYP");
    nrBypassAttach   = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "nrBypass",   nrBypassBtn);
    gateBypassAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "gateBypass", gateBypassBtn);

    // ── Bypass global A/B ─────────────────────────────────────────────────────
    // Le titre "INVISIBLE●FIX" agit comme bouton bypass-all (transparent, sur le wordmark)
    // globalBypassBtn : zone transparente sur le wordmark — aucun rendu JUCE
    globalBypassBtn.setButtonText("");
    globalBypassBtn.setLookAndFeel(&bypassZoneLAF);
    globalBypassBtn.setToggleable(false);
    globalBypassBtn.setWantsKeyboardFocus(false);
    globalBypassBtn.onClick = [this]()
    {
        bool newState = !audioProcessor.globalBypassActive.load();
        audioProcessor.globalBypassActive.store(newState);
        repaint();
    };
    addAndMakeVisible(globalBypassBtn);

    // LIVE/HQ bouton — haut droite (remplace le label FACTORY/USER)
    liveHQBtn.setButtonText("HQ");
    liveHQBtn.setWantsKeyboardFocus(false);
    liveHQBtn.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    liveHQBtn.setColour(juce::TextButton::textColourOffId,  Theme::teal);
    liveHQBtn.onClick = [this]()
    {
        audioProcessor.undoManager.beginNewTransaction();
        if (auto* p = audioProcessor.apvts.getParameter("liveMode"))
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
    };
    addAndMakeVisible(liveHQBtn);

    // ── Barre preset (style Arturia) ─────────────────────────────────────────
    auto stylePillBtn = [](juce::TextButton& b, const juce::String& text) {
        b.setButtonText(text);
        b.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        b.setColour(juce::TextButton::buttonOnColourId,Theme::teal.withAlpha(0.18f));
        b.setColour(juce::TextButton::textColourOffId, Theme::textSecondary);
        b.setColour(juce::TextButton::textColourOnId,  Theme::teal);
    };
    // ── Preset browser flottant ───────────────────────────────────────────────
    presetBrowser = std::make_unique<PresetBrowserPanel>(audioProcessor);
    presetBrowser->setVisible(false);
    addChildComponent(presetBrowser.get());

    // Bouton BROWSE (≡) dans la barre preset
    stylePillBtn(browsePresetBtn, juce::String::fromUTF8("\xe2\x89\xa1")); // ≡
    browsePresetBtn.onClick = [this]() { togglePresetBrowser(); };
    addAndMakeVisible(browsePresetBtn);

    prevPresetBtn.setButtonText(juce::String::fromUTF8("\xe2\x80\xb9")); // ‹
    nextPresetBtn.setButtonText(juce::String::fromUTF8("\xe2\x80\xba")); // ›
    for (auto* b : { &prevPresetBtn, &nextPresetBtn })
    {
        b->setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        b->setColour(juce::TextButton::buttonOnColourId,juce::Colours::transparentBlack);
        b->setColour(juce::TextButton::textColourOffId, Theme::textSecondary);
        b->setColour(juce::TextButton::textColourOnId,  Theme::teal);
    }
    stylePillBtn(savePresetBtn, "SAVE");
    stylePillBtn(initPresetBtn, "DEFAULT");

    prevPresetBtn.onClick = [this]()
    {
        int n = (int)audioProcessor.presetNames.size();
        if (n == 0) return;
        int idx = (audioProcessor.currentPresetIndex - 1 + n) % n;
        audioProcessor.loadPreset(idx);
        updatePresetBar();
    };
    nextPresetBtn.onClick = [this]()
    {
        int n = (int)audioProcessor.presetNames.size();
        if (n == 0) return;
        int idx = (audioProcessor.currentPresetIndex + 1) % n;
        audioProcessor.loadPreset(idx);
        updatePresetBar();
    };
    savePresetBtn.onClick = [this]()
    {
        // Boîte de dialogue nom de preset — style simple JUCE
        auto* dialog = new juce::AlertWindow("Sauvegarder le preset",
                                              "Nom du preset :", juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("name", audioProcessor.currentPresetName, "");
        dialog->addButton("Sauvegarder", 1);
        dialog->addButton("Annuler", 0);
        dialog->enterModalState(true,
            juce::ModalCallbackFunction::create([this, dialog](int result) {
                if (result == 1)
                {
                    juce::String name = dialog->getTextEditorContents("name").trim();
                    if (name.isNotEmpty())
                    {
                        audioProcessor.savePreset(name);
                        updatePresetBar();
                    }
                }
                delete dialog;
            }), false);
    };
    initPresetBtn.onClick = [this]()
    {
        audioProcessor.loadPreset(0); // Init est toujours l'index 0
        updatePresetBar();
    };

    presetNameLabel.setText(audioProcessor.currentPresetName, juce::dontSendNotification);
    presetNameLabel.setFont(juce::Font(juce::FontOptions(11.5f)));
    presetNameLabel.setJustificationType(juce::Justification::centred);
    presetNameLabel.setColour(juce::Label::textColourId, Theme::panelDark2);
    presetNameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    // presetTypeLabel dessiné dans paint(), composant caché
    presetTypeLabel.setVisible(false);

    addAndMakeVisible(prevPresetBtn);
    addAndMakeVisible(nextPresetBtn);
    addAndMakeVisible(savePresetBtn);
    addAndMakeVisible(initPresetBtn);
    addAndMakeVisible(presetNameLabel);
    presetNameLabel.addMouseListener(this, false); // clic → togglePresetBrowser
    addAndMakeVisible(presetTypeLabel);

    // NR Dual Rail (kept for reference, hidden in Clean tab — replaced by knobs + band display)
    nrDualRail = std::make_unique<DualRailComponent>();
    nrDualRail->upperParam = getParam("nrThreshold");
    nrDualRail->lowerParam = getParam("nrFloor");
    nrDualRail->upperLabel = "THRESHOLD";
    nrDualRail->lowerLabel = "FLOOR";
    nrDualRail->dbMin = -80.0f;
    nrDualRail->dbMax =   0.0f;
    addChildComponent(nrDualRail.get());

    // Gate Dual Rail (Threshold -60..0 + Floor -45..0)
    gateDualRail = std::make_unique<DualRailComponent>();
    gateDualRail->upperParam = getParam("cleanGateThreshold");
    gateDualRail->lowerParam = getParam("cleanGateFloor");
    gateDualRail->upperLabel = "THRESHOLD";
    gateDualRail->lowerLabel = "FLOOR";
    gateDualRail->dbMin = -60.0f;
    gateDualRail->dbMax =   0.0f;
    addChildComponent(gateDualRail.get());

    // Gate Waveform Display (remplace DualRail visuellement)
    {
        auto* wf      = new GateWaveformDisplay();
        wf->threshParam = getParam("cleanGateThreshold");
        wf->floorParam  = getParam("cleanGateFloor");
        gateWaveform.reset(wf);
    }
    addChildComponent(gateWaveform.get());

    // Gate AHR — trizone A|H|R
    {
        auto* ahr      = new AHRTrizone();
        ahr->atkParam  = getParam("cleanGateAttack");
        ahr->holdParam = getParam("cleanGateHold");
        ahr->relParam  = getParam("cleanRelease");
        gateAHR.reset(ahr);
    }
    addChildComponent(gateAHR.get());

    // De-Ess AR — trizone A|R (2 zones, holdParam = nullptr)
    {
        auto* ar      = new AHRTrizone();
        ar->atkParam  = getParam("deEssAttack");
        ar->relParam  = getParam("deEssRelease");
        deEssAR.reset(ar);
    }
    addChildComponent(deEssAR.get());

    // De-Ess Detect Display (Threshold + Detect zone)
    deEssDetect = std::make_unique<DeEssDetectDisplay>();
    deEssDetect->threshParam = getParam("deEssThreshold");
    deEssDetect->detectParam = getParam("deEssDetectRatio");
    deEssDetect->freqParam   = getParam("deEssFreq");
    deEssDetect->modeParam   = getParam("deEssFilterMode");
    deEssDetect->processor   = &audioProcessor;
    addChildComponent(deEssDetect.get());

    // ── Metering I/O — gain sliders invisibles sur les barres ─────────────────
    auto setupGainSlider = [&](juce::Slider& sl, const juce::String& paramId) {
        sl.setSliderStyle(juce::Slider::LinearVertical);
        sl.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        sl.setWantsKeyboardFocus(false);
        sl.setColour(juce::Slider::thumbColourId, juce::Colours::transparentBlack);
        sl.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
        sl.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
        addChildComponent(sl);
    };
    setupGainSlider(inputGainSlider, "inputGain");
    setupGainSlider(outputGainSlider, "outputGain");
    inputGainAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "inputGain",  inputGainSlider);
    outputGainAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "outputGain", outputGainSlider);
    inputGainSlider.onValueChange  = [this]{ repaint(); };
    outputGainSlider.onValueChange = [this]{ repaint(); };

    // ── A/B Snapshot — session only ───────────────────────────────────────────
    auto setupABBtn = [&](juce::TextButton& btn, const juce::String& txt) {
        btn.setButtonText(txt);
        btn.setWantsKeyboardFocus(false);
        btn.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::TextButton::textColourOffId,  Theme::tealDim);
        btn.setColour(juce::TextButton::textColourOnId,   Theme::teal);
        addAndMakeVisible(btn);
    };
    setupABBtn(abBtnA,    "A");
    setupABBtn(abArrowBtn, juce::CharPointer_UTF8("\xe2\x86\x92 B"));
    setupABBtn(abBtnB,    "B");

    abBtnA.onClick = [this]()
    {
        if (!hasSnapshotA) { abSaveSlot(0); }
        else               { abRecallSlot(0); }
        abActiveSlot = 0;
        abUpdateButtons();
    };
    abBtnB.onClick = [this]()
    {
        if (!hasSnapshotB) { abSaveSlot(1); }
        else               { abRecallSlot(1); }
        abActiveSlot = 1;
        abUpdateButtons();
    };
    abArrowBtn.onClick = [this]()
    {
        // Copie l'état courant dans l'AUTRE slot
        int target = (abActiveSlot == 0) ? 1 : 0;
        abSaveSlot(target);
        abUpdateButtons();
    };
    abUpdateButtons();

    // De-Rev Canvas — visualisation dual-axis interactive
    deRevCanvas = std::make_unique<DeRevCanvas>(audioProcessor, audioProcessor.apvts);
    addChildComponent(deRevCanvas.get());
    deRevCanvas->onTooltipChange = [this](const juce::String& param, const juce::String& desc) {
        currentTooltipParam = param;
        currentTooltipText  = desc;
        repaint();
    };

    // Bouton LINK pour le canvas De-Rev
    deRevLinkBtn.setButtonText(juce::CharPointer_UTF8("\xe2\x9b\x93 LINK"));
    deRevLinkBtn.setClickingTogglesState(true);
    deRevLinkBtn.setWantsKeyboardFocus(false);
    deRevLinkBtn.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    deRevLinkBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    deRevLinkBtn.setColour(juce::TextButton::textColourOffId,  Theme::tealDim);
    deRevLinkBtn.setColour(juce::TextButton::textColourOnId,   Theme::teal);
    deRevLinkBtn.onClick = [this]()
    {
        if (deRevCanvas) deRevCanvas->setLinked(deRevLinkBtn.getToggleState());
    };
    addChildComponent(deRevLinkBtn);

    // Initialiser l'alpha des modules bypass selon l'état courant
    updateBypassAlphas();

    // Timer 30Hz : VU fluide + blink BYP via compteur de frames
    startTimerHz(30);

    // Resize uniquement via le bouton de scale (pas de drag coin)
    setResizable(false, nullptr);

    // Indicateur scale cliquable (popup menu des tailles)
    scaleBtn.setButtonText("100 %");
    scaleBtn.setColour(juce::TextButton::buttonColourId,  Theme::scrim);
    scaleBtn.setColour(juce::TextButton::textColourOffId, Theme::textMuted);
    scaleBtn.onClick = [this]()
    {
        juce::PopupMenu m;
        const float sizes[]   = { 0.75f, 1.0f, 1.25f, 1.50f };
        const char*  labels[] = { "75 %", "100 %", "125 %", "150 %" };
        for (int i = 0; i < 4; ++i)
            m.addItem(i + 1, labels[i], true, std::abs(sizes[i] - currentScale) < 0.01f);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&scaleBtn),
            [this](int result)
            {
                if (result < 1 || result > 4) return;
                const float sizes[] = { 0.75f, 1.0f, 1.25f, 1.50f };
                currentScale = sizes[result - 1];
                audioProcessor.editorScale.store(currentScale);
                resizeConstrainer.currentSnap = currentScale;
                const int baseH = (currentTab == AdvancedTab::Clean) ? 504
                                : (currentTab == AdvancedTab::None)   ? 338 : 478;
                setSize(juce::roundToInt(760.0f * currentScale),
                        juce::roundToInt((float)baseH * currentScale));
            });
    };
    addAndMakeVisible(scaleBtn);

    // Undo / Redo (style Arturia — flèches dans la barre preset)
    undoBtn.setButtonText(juce::String::fromUTF8("\xe2\x86\x90")); // ←
    redoBtn.setButtonText(juce::String::fromUTF8("\xe2\x86\x92")); // →
    for (auto* b : { &undoBtn, &redoBtn }) {
        b->setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        b->setColour(juce::TextButton::textColourOffId, Theme::textSecondary);
    }
    undoBtn.onClick = [this]() { audioProcessor.undoManager.undo(); };
    redoBtn.onClick = [this]() { audioProcessor.undoManager.redo(); };
    addAndMakeVisible(undoBtn);
    addAndMakeVisible(redoBtn);

    // De-Rev Floor slider
    setupModernSlider(deRevFloorSlider, "FLOOR");
    deRevFloorAttach = std::make_unique<SliderAttachment>(
        audioProcessor.apvts, "deRevFloor", deRevFloorSlider.slider);

    setupModernSlider(deRevBandLFSlider, "LF");
    deRevBandLFSlider.slider.setName("deRevBandLFSlider");
    setupModernSlider(deRevBandMFSlider, "MF");
    deRevBandMFSlider.slider.setName("deRevBandMFSlider");
    setupModernSlider(deRevBandHFSlider, "HF");
    deRevBandHFSlider.slider.setName("deRevBandHFSlider");
    deRevBandLFAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deRevBandLF", deRevBandLFSlider.slider);
    deRevBandMFAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deRevBandMF", deRevBandMFSlider.slider);
    deRevBandHFAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "deRevBandHF", deRevBandHFSlider.slider);

    // Helper pour activer le double-clic reset avec la valeur par défaut APVTS
    auto enableDblClick = [&](juce::Slider& sl, const juce::String& paramId) {
        if (auto* p = audioProcessor.apvts.getParameter(paramId))
            sl.setDoubleClickReturnValue(true,
                p->convertFrom0to1(p->getDefaultValue()));
    };
    enableDblClick(cleanSlider.slider,    "clean");
    enableDblClick(deEssSlider.slider,    "deess");
    enableDblClick(roomSlider.slider,     "room");
    enableDblClick(reverbSlider.slider,   "reverb");
    enableDblClick(airSlider.slider,      "air");
    enableDblClick(masterGainSlider,      "outputGain");
    enableDblClick(advRoomFreqSlider.slider,       "roomNotchFreq");
    enableDblClick(advRoomQSlider.slider,          "roomNotchQ");
    enableDblClick(advRoomDepthBoostSlider.slider, "roomDepthBoost");
    enableDblClick(advDeEssFreqSlider.slider, "deEssFreq");
    enableDblClick(advDeEssThreshSlider.slider, "deEssThreshold");
    enableDblClick(advDeEssIntenSlider.slider,  "deEssIntensity");
    enableDblClick(advDeEssAtkSlider.slider,  "deEssAttack");
    enableDblClick(advDeEssRelSlider.slider,  "deEssRelease");
    enableDblClick(advDeEssDetectSlider.slider, "deEssDetectRatio");
    enableDblClick(advGateThresholdSlider.slider, "cleanGateThreshold");
    enableDblClick(advGateAttackSlider.slider,    "cleanGateAttack");
    enableDblClick(advGateHoldSlider.slider,      "cleanGateHold");
    enableDblClick(plosiveProtectSlider.slider,   "cleanPlosiveProtect");
    enableDblClick(advGateFloorSlider.slider,     "cleanGateFloor");
    enableDblClick(advCleanRelSlider.slider,      "cleanRelease");
    enableDblClick(advAirDriveSlider.slider,      "airDrive");
    enableDblClick(nrThreshSlider.slider, "nrThreshold");
    enableDblClick(nrRatioSlider.slider,  "nrRatio");
    enableDblClick(nrFloorSlider.slider,  "nrFloor");
    enableDblClick(nrSmoothSlider.slider, "nrSmooth");
    enableDblClick(deRevFloorSlider.slider,    "deRevFloor");
    enableDblClick(deRevBandLFSlider.slider,   "deRevBandLF");
    enableDblClick(deRevBandMFSlider.slider,   "deRevBandMF");
    enableDblClick(deRevBandHFSlider.slider,   "deRevBandHF");

    // Restaurer le scale persisté (sauvegardé dans getStateInformation)
    currentScale = juce::jlimit(0.75f, 1.50f, audioProcessor.editorScale.load());
    resizeConstrainer.currentSnap = currentScale;
    setSize(juce::roundToInt(760.0f * currentScale),
            juce::roundToInt(338.0f * currentScale));
}


InvisibleFixAudioProcessorEditor::~InvisibleFixAudioProcessorEditor() = default;

// =============================================================================
//  A/B Snapshot helpers
// =============================================================================
void InvisibleFixAudioProcessorEditor::abSaveSlot(int slot)
{
    juce::MemoryOutputStream mos;
    audioProcessor.apvts.copyState().writeToStream(mos);
    if (slot == 0) { snapshotA = mos.getMemoryBlock(); hasSnapshotA = true; }
    else           { snapshotB = mos.getMemoryBlock(); hasSnapshotB = true; }
}

void InvisibleFixAudioProcessorEditor::abRecallSlot(int slot)
{
    const juce::MemoryBlock& mb = (slot == 0) ? snapshotA : snapshotB;
    if (mb.getSize() == 0) return;
    juce::MemoryInputStream mis(mb, false);
    auto state = juce::ValueTree::readFromStream(mis);
    if (state.isValid()) audioProcessor.apvts.replaceState(state);
}

void InvisibleFixAudioProcessorEditor::abUpdateButtons()
{
    // Apparence A
    bool aActive = (abActiveSlot == 0);
    abBtnA.setColour(juce::TextButton::textColourOffId,
        aActive ? Theme::teal : (hasSnapshotA ? Theme::textMuted : Theme::tealDim));
    abBtnA.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);

    // Apparence B
    bool bActive = (abActiveSlot == 1);
    abBtnB.setColour(juce::TextButton::textColourOffId,
        bActive ? Theme::teal : (hasSnapshotB ? Theme::textMuted : Theme::tealDim));
    abBtnB.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);

    // Flèche — pointe vers l'autre slot
    if (abActiveSlot == 0)
        abArrowBtn.setButtonText(juce::CharPointer_UTF8("\xe2\x86\x92 B"));
    else if (abActiveSlot == 1)
        abArrowBtn.setButtonText(juce::CharPointer_UTF8("A \xe2\x86\x90"));
    else
        abArrowBtn.setButtonText(juce::CharPointer_UTF8("\xe2\x87\x84"));
}

// =============================================================================
//  APPLICATION DU STYLE AUX SLIDERS
// =============================================================================
void InvisibleFixAudioProcessorEditor::updatePresetBar()
{
    presetNameLabel.setText(audioProcessor.currentPresetName, juce::dontSendNotification);
    presetTypeLabel.setText(audioProcessor.currentPresetIsFactory ? "FACTORY" : "USER PRESET",
                            juce::dontSendNotification);
    presetTypeLabel.setColour(juce::Label::textColourId,
        audioProcessor.currentPresetIsFactory
            ? Theme::teal.withAlpha(0.65f)
            : Theme::accent.withAlpha(0.75f));
    if (presetBrowser != nullptr && presetBrowser->isVisible())
    {
        presetBrowser->rebuildFiltered();
        presetBrowser->repaint();
    }
    repaint();
}

void InvisibleFixAudioProcessorEditor::togglePresetBrowser()
{
    if (presetBrowser == nullptr) return;
    bool show = !presetBrowser->isVisible();
    presetBrowser->setVisible(show);
    if (show)
    {
        presetBrowser->rebuildFiltered();
        presetBrowser->toFront(false);
    }
}

void InvisibleFixAudioProcessorEditor::applyModernKnobStyle(juce::Slider& slider)
{
    slider.setLookAndFeel(&arcFinLAF);
    // Arc Fin : 270° de 225° (bas-gauche) à 135° (bas-droit), sens horaire
    // Le conic CSS démarre à 135°+90° = 225° (zone morte = 90° après "from 135deg")
    slider.setRotaryParameters(1.25f * juce::MathConstants<float>::pi,
                               2.75f * juce::MathConstants<float>::pi, true);
}

void InvisibleFixAudioProcessorEditor::setupModernSlider(ModernSlider& modernSlider, const juce::String& name)
{
    // Taille textbox par défaut pour les knobs avancés (50px)
    modernSlider.slider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    modernSlider.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    // double-clic reset géré par les custom mouseDoubleClick dans les composants
    modernSlider.slider.setColour(juce::Slider::textBoxTextColourId, Theme::textDark);
    modernSlider.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    modernSlider.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    modernSlider.slider.setColour(juce::Slider::textBoxHighlightColourId, Theme::accent.withAlpha(0.25f));

    applyModernKnobStyle(modernSlider.slider);
    addAndMakeVisible(modernSlider.slider);

    // Libellé paramètre (sous la valeur) — Arc Fin: 8px, couleur muted
    modernSlider.label.setText(name, juce::dontSendNotification);
    modernSlider.label.setFont(juce::Font(juce::FontOptions(8.0f)));
    modernSlider.label.setJustificationType(juce::Justification::centred);
    modernSlider.label.setColour(juce::Label::textColourId, Theme::textMuted);
    addAndMakeVisible(modernSlider.label);
}

// =============================================================================
//  SCALE CONSTRAINER — snap aux paliers avant tout resize
// =============================================================================
void InvisibleFixAudioProcessorEditor::ScaleConstrainer::checkBounds(
    juce::Rectangle<int>& bounds,
    const juce::Rectangle<int>& /*previousBounds*/,
    const juce::Rectangle<int>& /*limits*/,
    bool, bool, bool, bool)
{
    const float snaps[] = { 0.75f, 1.0f, 1.25f, 1.50f };
    float rawS = (float)bounds.getWidth() / 760.0f;

    // Hystérésis : on ne change de palier que si on dépasse le MILIEU vers le voisin.
    // Sans ça, on oscillerait à chaque pixel sur la frontière.
    float nearest = currentSnap;
    float nearestDist = std::abs(currentSnap - rawS);
    for (float s : snaps)
    {
        float d = std::abs(s - rawS);
        if (d < nearestDist) { nearestDist = d; nearest = s; }
    }
    if (nearest != currentSnap)
    {
        // On bascule seulement si on a passé le point médian
        float mid = (currentSnap + nearest) * 0.5f;
        bool pastMid = (nearest > currentSnap) ? (rawS > mid) : (rawS < mid);
        if (pastMid) currentSnap = nearest;
    }

    int baseH = 338;
    if (tabPtr != nullptr)
    {
        if      (*tabPtr == AdvancedTab::Clean) baseH = 504;
        else if (*tabPtr != AdvancedTab::None)  baseH = 478;
    }
    bounds.setWidth (juce::roundToInt(760.0f * currentSnap));
    bounds.setHeight(juce::roundToInt((float)baseH * currentSnap));
}

// =============================================================================
//  DESSIN GRAPHIQUE — ARC FIN
// =============================================================================
void InvisibleFixAudioProcessorEditor::paint(juce::Graphics& g)
{
    // ── Scale snap : tout le dessin est en coordonnées de base (760×baseH) ───
    // La transform scale(S) fait le reste automatiquement — zéro changement ailleurs.
    const float S   = getWidth() / 760.0f;
    const float BW  = 760.0f;
    const float BH  = getHeight() / S;   // hauteur en coordonnées virtuelles

    g.addTransform(juce::AffineTransform::scale(S));

    // ── Fond dégradé 145° ebf4f7 → e4f0ee → ddeaf0 ──────────────────────────
    {
        juce::ColourGradient bg(Theme::surfaceSky, 0.0f, 0.0f,
                                Theme::surfaceBlue, BW, BH, false);
        bg.addColour(0.55, Theme::surfaceLight);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(0.0f, 0.0f, BW, BH, 9.0f);
    }

    // ── Bordure extérieure glass ─────────────────────────────────────────────
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawRoundedRectangle(0.5f, 0.5f, BW - 1.0f, BH - 1.0f, 9.0f, 1.0f);

    // ── Barre Preset style Arturia — 28px, épurée, nom centré ────────────────
    {
        const float PW = BW;
        const float PH = 28.0f;

        // Fond légèrement distinct du reste du plugin
        g.setColour(Theme::surfaceBar);
        g.fillRect(0.0f, 0.0f, PW, PH);

        // Séparateur bas (trait fin)
        g.setColour(Theme::textMuted.withAlpha(0.28f));
        g.fillRect(0.0f, PH - 1.0f, PW, 1.0f);

        // Zone centrale : deux petits chevrons et le nom
        // Prev arrow zone : 24px à x=202
        // Name zone       : 302px x=228..530
        // Next arrow zone : 24px à x=532

        // Fond dégradé subtil de la zone nom (léger creux Arturia)
        juce::ColourGradient nameGrad(
            Theme::surfaceEdge.withAlpha(0.55f), 228.0f, 0.0f,
            Theme::surfaceBar.withAlpha(0.0f),  228.0f, PH, false);
        g.setGradientFill(nameGrad);
        g.fillRect(202.0f, 0.0f, 356.0f, PH - 1.0f);
        g.setColour(juce::Colours::transparentBlack);

        // Traits verticaux délimitant la zone nom (style Arturia)
        g.setColour(Theme::textMuted.withAlpha(0.20f));
        g.fillRect(202.0f, 5.0f, 1.0f, PH - 10.0f);
        g.fillRect(557.0f, 5.0f, 1.0f, PH - 10.0f);

        // Séparateur avant les boutons d'action
        g.fillRect(570.0f, 5.0f, 1.0f, PH - 10.0f);

    }

    // ── Tout le contenu ci-dessous est décalé de 28px vers le bas ────────────
    g.saveState();
    g.addTransform(juce::AffineTransform::translation(0.0f, 28.0f));

    // ── Header ───────────────────────────────────────────────────────────────
    // Wordmark gauche : INVISIBLE ● FIX — cliquable comme bypass-all
    bool bypassActive = audioProcessor.globalBypassActive.load();
    bool blinkVis     = bypassActive && bypassBlinkOn;  // vrai si le blink est visible

    int hY = 12, hH = 28;
    auto titleFont = juce::Font(juce::FontOptions(17.0f));
    g.setFont(titleFont);
    g.setColour(Theme::textDark);
    g.drawText("INVISIBLE", 22, hY, 120, hH, juce::Justification::centredLeft);

    // Calculer la largeur réelle du texte pour coller le point
    float invisW = juce::GlyphArrangement::getStringWidth(titleFont, "INVISIBLE");
    float dotX = 22.0f + invisW + 5.0f;
    float dotY = (float)hY + (float)hH * 0.5f - 3.0f;

    // Pastille : verte cyan normal, rouge clignotant si bypass actif
    juce::Colour dotColour = bypassActive
        ? (blinkVis ? Theme::danger : Theme::danger.withAlpha(0.20f))
        : Theme::accent;
    g.setColour(dotColour);
    g.fillEllipse(dotX, dotY, 6.0f, 6.0f);

    // FIX : teal normal, rouge clignotant si bypass actif
    juce::Colour fixColour = bypassActive
        ? (blinkVis ? Theme::danger : Theme::danger.withAlpha(0.20f))
        : Theme::teal;
    g.setFont(titleFont);
    g.setColour(fixColour);
    g.drawText("FIX", (int)(dotX + 9.0f), hY, 40, hH, juce::Justification::centredLeft);

    // Version — droite du header
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.setColour(Theme::textMuted);
    g.drawText("V0.8  LAB STUDIO", (int)BW - 345, hY, 110, hH, juce::Justification::centredRight);

    // ── Cartes modules ───────────────────────────────────────────────────────
    // x positions : [24,168,312,456,600], w=136, y=58, h=200
    const int modX[5]   = { 24, 168, 312, 456, 600 };
    const int modW = 136, modY = 58, modH = 200;
    const char* modNames[5] = { "CLEAN", "ROOM", "DE-REV", "DE-ESS", "AIR ENHANCER" };

    AdvancedTab tabForMod[5] = {
        AdvancedTab::Clean, AdvancedTab::Room,
        AdvancedTab::DeRev, AdvancedTab::DeEss, AdvancedTab::Air
    };
    // bypass states
    bool bypassed[5] = {
        cleanBypassBtn.button.getToggleState(),
        roomBypassBtn.button.getToggleState(),
        reverbBypassBtn.button.getToggleState(),
        deEssBypassBtn.button.getToggleState(),
        airBypassBtn.button.getToggleState()
    };

    for (int i = 0; i < 5; ++i)
    {
        float fx = (float)modX[i], fy = (float)modY;
        float fw = (float)modW,    fh = (float)modH;
        float r  = 9.0f;
        bool  sel = (currentTab != AdvancedTab::None && currentTab == tabForMod[i]);
        float cardAlpha = bypassed[i] ? 0.30f : 0.55f;

        // Ombre douce
        g.setColour(Theme::navy.withAlpha(0.07f));
        g.fillRoundedRectangle(fx + 1, fy + 2, fw, fh, r);

        // Fond carte glass
        g.setColour(juce::Colours::white.withAlpha(cardAlpha));
        g.fillRoundedRectangle(fx, fy, fw, fh, r);

        // Bordure blanche glass
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        g.drawRoundedRectangle(fx, fy, fw, fh, r, 1.0f);

        // Overlay cyan si sélectionné
        if (sel)
        {
            g.setColour(Theme::accent);
            g.drawRoundedRectangle(fx - 1, fy - 1, fw + 2, fh + 2, r, 1.5f);
        }

        // Nom module (centré, y=modY+22)
        g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        g.setColour(Theme::textDark.withAlpha(bypassed[i] ? 0.40f : 1.0f));
        g.drawText(modNames[i], modX[i], modY + 22, modW, 15, juce::Justification::centred);

        // LED — cyan uniquement pour le module dont l'onglet avancé est ouvert
        float ledX = fx + fw * 0.5f - 3.5f;
        float ledY = fy + fh - 18.0f;
        bool ledActive = (currentTab == tabForMod[i])
                      && (tabForMod[i] != AdvancedTab::None)
                      && !bypassed[i];
        g.setColour(ledActive ? Theme::accent : Theme::textPrimary);
        g.fillEllipse(ledX, ledY, 7.0f, 7.0f);
    }

    // ── Section Advanced ─────────────────────────────────────────────────────
    if (currentTab != AdvancedTab::None)
    {
        // Label "ADVANCED CONFIGURATION"
        g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
        g.setColour(Theme::textMuted);
        g.drawText("ADVANCED CONFIGURATION", 24, 265, 300, 13, juce::Justification::centredLeft);

        auto drawAdvPanel = [&](float px, float py, float pw, float ph, const juce::String& title)
        {
            g.setColour(juce::Colours::white.withAlpha(0.40f));
            g.fillRoundedRectangle(px, py, pw, ph, 9.0f);
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.drawRoundedRectangle(px, py, pw, ph, 9.0f, 1.0f);
            g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
            g.setColour(Theme::textMuted);
            g.drawText(title, px + 14, py + 12, pw - 28, 11, juce::Justification::centredLeft);
        };
        // Panneaux peints pilotés par les données (Source/Generated/UILayout.h ← tools/layout.json)
        auto drawPanel = [&](const UI::Panel& p){ drawAdvPanel(p.x, p.y, p.w, p.h, p.title); };

        if (currentTab == AdvancedTab::Clean)
        {
            drawPanel(UI::panel::nr);
            drawPanel(UI::panel::gate);

            // Separateur vertical NR : band display / knobs
            g.setColour(Theme::textMuted.withAlpha(0.10f));
            g.fillRect(157.0f, 294.0f, 1.0f, 132.0f);

            // Separateur vertical Gate : rail / enveloppe
            g.setColour(Theme::textMuted.withAlpha(0.12f));
            g.fillRect(462.0f, 294.0f, 1.0f, 132.0f);

            // Petit label "NR ACTIVITY" au-dessus du display bandes
            g.setFont(juce::Font(juce::FontOptions(7.0f).withStyle("Bold")));
            g.setColour(Theme::textMuted.withAlpha(0.6f));
            g.drawText("NR ACTIVITY", 34, 294, 118, 9, juce::Justification::centred);
        }
        else if (currentTab == AdvancedTab::DeEss)
        {
            drawPanel(UI::panel::deEss);
        }
        else if (currentTab == AdvancedTab::Room)
        {
            // ── Panneau gauche : NOTCH ANALYSIS (x=24, w=330, h=152) ─────
            const UI::Panel& la = UI::panel::roomAnalysis;
            float lpX = la.x, lpY = la.y, lpW = la.w, lpH = la.h;
            drawAdvPanel(lpX, lpY, lpW, lpH, la.title);

            // Y-axis labels (dB) alignés à gauche du canvas
            float yLabX = lpX + 8.0f;
            float canvX = lpX + 34.0f;  // canvas commence après zone Y-axis
            float canvY = lpY + 28.0f;
            float canvW = lpW - 44.0f;  // laisse 10px à droite
            float canvH = 112.0f;

            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(Theme::textMuted);
            const char* dbLabels[] = { "0", "-12", "-24", "-36" };
            for (int di = 0; di < 4; ++di)
            {
                float yPos = canvY + (float)di * (canvH / 3.0f);
                g.drawText(dbLabels[di], (int)yLabX, (int)(yPos - 5), 22, 10,
                           juce::Justification::centredRight);
            }

            // X-axis labels (fréquences)
            float xAxisY = canvY + canvH + 3.0f;
            auto xAtFreq = [&](float f) -> float {
                return canvX + canvW * std::log(f / 50.0f) / std::log(100.0f);
            };
            struct { float hz; const char* lbl; bool highlight; } xLabels[] = {
                {50,   "50 Hz", false}, {200, "200",  false},
                {500,  "500",   false}, {1000,"1k",   true},
                {2000, "2k",    false}, {5000,"5k Hz",false}
            };
            // Récupérer fc pour savoir si "1k" est proche
            double fcRoom = juce::jlimit(100.0, 2000.0,
                (double)*audioProcessor.apvts.getRawParameterValue("roomNotchFreq"));
            for (auto& xl : xLabels)
            {
                float lx = xAtFreq(xl.hz);
                bool near = (std::fabs(xl.hz - (float)fcRoom) < (float)fcRoom * 0.5f);
                g.setColour(near ? Theme::accent : Theme::textMuted);
                g.setFont(juce::Font(juce::FontOptions(near ? 7.5f : 7.0f)
                          .withStyle(near ? "Bold" : "Regular")));
                g.drawText(xl.lbl, (int)(lx - 14), (int)xAxisY, 30, 10,
                           juce::Justification::centred);
            }

            // ── Panneau droit : NOTCH PARAMETERS (x=364, w=372, h=152) ──
            const UI::Panel& rp = UI::panel::roomParams;
            float rpX = rp.x, rpY = rp.y, rpW = rp.w, rpH = rp.h;
            drawAdvPanel(rpX, rpY, rpW, rpH, rp.title);

            // Labels au-dessus des knobs
            g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
            g.setColour(Theme::textMuted);
            g.drawText("NOTCH FREQ", (int)(rpX + 16), (int)(rpY + 26), 80, 10,
                       juce::Justification::centredLeft);
            g.drawText("NOTCH Q",    (int)(rpX + 112), (int)(rpY + 26), 60, 10,
                       juce::Justification::centredLeft);

            // ── Chip NOTCH TRACKER — fréquence réelle du filtre + profondeur ────
            {
                float trackedFreq = audioProcessor.getRoomTrackedFreq();
                float dynDepth    = audioProcessor.getRoomDynDepth();
                float manualFreq  = *audioProcessor.apvts.getRawParameterValue("roomNotchFreq");
                float effectiveDB = -24.0f * juce::jmax(0.20f, dynDepth);
                bool  isTracking  = std::abs(trackedFreq - manualFreq) > 8.0f;

                float chipX = rpX + 10.0f, chipY = rpY + 88.0f;
                float chipW = rpW - 20.0f, chipH = 56.0f;

                // Fond — plus coloré si le tracking est actif
                juce::Colour chipAccent = isTracking ? Theme::teal
                                                     : Theme::accent;
                g.setColour(chipAccent.withAlpha(0.08f));
                g.fillRoundedRectangle(chipX, chipY, chipW, chipH, 6.0f);
                g.setColour(chipAccent.withAlpha(0.22f));
                g.drawRoundedRectangle(chipX, chipY, chipW, chipH, 6.0f, 1.0f);

                // Titre
                g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
                g.setColour(Theme::textMuted);
                g.drawText("NOTCH TRACKER", (int)chipX, (int)(chipY + 6), (int)chipW, 10,
                           juce::Justification::centred);

                // Fréquence trackée (filtre réel) vs manuelle (slider)
                juce::String freqStr = juce::String((int)trackedFreq) + " Hz";
                if (isTracking)
                    freqStr += "  (slider: " + juce::String((int)manualFreq) + " Hz)";
                g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
                g.setColour(chipAccent);
                g.drawText(freqStr, (int)chipX, (int)(chipY + 20), (int)chipW, 12,
                           juce::Justification::centred);

                // Profondeur dynamique + barre de niveau
                juce::String depthStr = "Reduction: " + juce::String(effectiveDB, 1) + " dB";
                g.setFont(juce::Font(juce::FontOptions(7.5f)));
                g.setColour(Theme::textMuted);
                g.drawText(depthStr, (int)chipX, (int)(chipY + 34), (int)chipW, 10,
                           juce::Justification::centred);

                // Barre de profondeur dynamique (dynDepth visualisé)
                float barX = chipX + 16.0f, barY = chipY + 46.0f;
                float barW = chipW - 32.0f, barH = 5.0f;
                g.setColour(Theme::textPrimary.withAlpha(0.4f));
                g.fillRoundedRectangle(barX, barY, barW, barH, 2.5f);
                g.setColour(chipAccent.withAlpha(0.75f));
                g.fillRoundedRectangle(barX, barY, barW * juce::jmax(0.20f, dynDepth), barH, 2.5f);
            }
        }
        else if (currentTab == AdvancedTab::DeRev)
        {
            drawPanel(UI::panel::deRev);
        }
        else if (currentTab == AdvancedTab::Air)
        {
            drawPanel(UI::panel::air);

            // Fréquence de coupure (lue depuis l'APVTS = même valeur que le slider)
            float fc = juce::jlimit(7000.0f, 16000.0f,
                        (float)*audioProcessor.apvts.getRawParameterValue("airFreq"));

            // Conversion linéaire → pixel (7kHz–16kHz, plage réelle du traitement)
            auto freqToX = [&](float f, float x0, float w2) -> float {
                float t = (f - 7000.0f) / 9000.0f;
                return x0 + t * w2;
            };

            // ── Canvas spectre ────────────────────────────────────────────
            float sX = 38.0f, sY = 302.0f, sW = 414.0f, sH = 72.0f;
            // Position du thumb alignée sur AirFreqSliderLAF (r=8px, plage APVTS)
            float thumbR = 8.0f;
            float fcNorm = (fc - 7000.0f) / 9000.0f;
            float fcXpx = sX + thumbR + fcNorm * (sW - 2.0f * thumbR);

            g.setColour(Theme::surfaceLighter.withAlpha(0.70f));
            g.fillRoundedRectangle(sX, sY, sW, sH, 5.0f);
            g.setColour(Theme::accent.withAlpha(0.12f));
            g.drawRoundedRectangle(sX, sY, sW, sH, 5.0f, 1.0f);

            float zeroY   = sY + sH - 10.0f;
            float maxGainY = sY + 8.0f;

            // 0 dB pointillé
            g.setColour(Theme::textMuted.withAlpha(0.32f));
            for (float d = sX + 4.0f; d < sX + sW; d += 9.0f)
                g.fillRect(d, zeroY, 4.0f, 1.0f);

            // Label "+ AIR BOOST" et "0 dB" sur le canvas
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(Theme::accent.withAlpha(0.55f));
            g.drawText("+ AIR BOOST", (int)(fcXpx + 8), (int)(maxGainY + 6), 60, 10,
                       juce::Justification::centredLeft);
            g.setColour(Theme::textMuted.withAlpha(0.60f));
            g.drawText("0 dB", (int)(sX + sW - 28), (int)(zeroY - 10), 28, 10,
                       juce::Justification::centredRight);

            // Courbe high-shelf (7kHz–16kHz linéaire)
            juce::Path shelf;
            for (int px = 0; px < (int)sW; ++px)
            {
                float freq = 7000.0f + (float)px / sW * 9000.0f;
                float sig  = 1.0f / (1.0f + std::exp(-5.5f * std::log10(freq / fc)));
                float py   = zeroY - sig * (zeroY - maxGainY);
                if (px == 0) shelf.startNewSubPath(sX, py);
                else         shelf.lineTo(sX + (float)px, py);
            }

            // Remplissage gauche (neutre) + droite (cyan)
            {
                juce::Path fillL = shelf;
                fillL.lineTo(fcXpx, zeroY); fillL.lineTo(sX, zeroY); fillL.closeSubPath();
                g.setColour(Theme::textMuted.withAlpha(0.08f));
                g.fillPath(fillL);

                juce::Path fillR;
                bool started = false;
                for (int px = 0; px < (int)sW; ++px)
                {
                    float cx2 = sX + (float)px;
                    if (cx2 < fcXpx) continue;
                    float freq = 7000.0f + (float)px / sW * 9000.0f;
                    float sig  = 1.0f / (1.0f + std::exp(-5.5f * std::log10(freq / fc)));
                    float py   = zeroY - sig * (zeroY - maxGainY);
                    if (!started) { fillR.startNewSubPath(cx2, zeroY); fillR.lineTo(cx2, py); started = true; }
                    else          fillR.lineTo(cx2, py);
                }
                if (started) { fillR.lineTo(sX + sW, zeroY); fillR.closeSubPath(); }

                // Remplissage droit : cyan→teal de base, réchauffe vers orange selon Drive
                float airDriveVal = juce::jlimit(0.0f, 1.0f,
                    (float)*audioProcessor.apvts.getRawParameterValue("airDrive"));

                juce::Colour cyanNear = Theme::accent;
                juce::Colour tealFar  = Theme::teal;
                juce::Colour warmNear = Theme::airWarm;
                juce::Colour warmFar = Theme::airWarmFar;

                juce::Colour gradNear = cyanNear.interpolatedWith(warmNear, airDriveVal * 0.85f);
                juce::Colour gradFar  = tealFar .interpolatedWith(warmFar,  airDriveVal * 0.85f);

                float alphaNear = 0.22f + airDriveVal * 0.18f;
                float alphaFar  = 0.38f + airDriveVal * 0.22f;

                juce::ColourGradient cg(gradNear.withAlpha(alphaNear), fcXpx, sY,
                                        gradFar .withAlpha(alphaFar),  sX + sW, sY, false);
                g.setGradientFill(cg);
                g.fillPath(fillR);
            }

            // Lire Drive pour la suite (déjà lu dans le bloc ci-dessus mais hors scope ici)
            float airDriveVal = juce::jlimit(0.0f, 1.0f,
                (float)*audioProcessor.apvts.getRawParameterValue("airDrive"));

            // Stroke courbe : toujours cyan d'origine
            g.setColour(Theme::accent.withAlpha(0.92f));
            g.strokePath(shelf, juce::PathStrokeType(1.5f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Ligne verticale fc : cyan fixe, fine (pas d'animation)
            g.setColour(Theme::accent.withAlpha(0.55f));
            g.fillRect(fcXpx - 1.0f, sY, 1.5f, sH);

            // Label fréquence au-dessus du canvas (suit la ligne)
            juce::String fcLabel = (fc >= 1000.0f)
                ? (juce::String(fc / 1000.0f, 1) + " kHz")
                : (juce::String((int)fc) + " Hz");
            g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
            g.setColour(Theme::accent);
            g.drawText(fcLabel, (int)(fcXpx - 20), (int)(sY - 14), 44, 12,
                       juce::Justification::centred);

            // ── Slider track (dessiné dans paint pour les ticks & labels) ─
            float tkY = sY + sH + 14.0f;  // y centre de la track (slider JUCE y=tkY-10 h=20)

            // Ticks au-dessus du slider (7k–16k)
            const float tickFreqs[] = { 7000, 8000, 10000, 12000, 14000, 16000 };
            const int   nTicks      = 6;
            for (int ti = 0; ti < nTicks; ++ti)
            {
                float tf = tickFreqs[ti];
                float tx = freqToX(tf, sX, sW);
                bool isFc = (std::fabs(tf - fc) < 800.0f);
                g.setColour(isFc ? Theme::accent : Theme::textPrimary);
                g.fillRect(tx, tkY - 5.0f, isFc ? 2.0f : 1.0f, 5.0f);
            }

            // Labels fréquences sous le slider
            const char* lblStr[] = { "7k","8k","10k","12k","14k","16k" };
            g.setFont(juce::Font(juce::FontOptions(7.5f)));
            for (int li = 0; li < nTicks; ++li)
            {
                float tx = freqToX(tickFreqs[li], sX, sW);
                bool isFc = (std::fabs(tickFreqs[li] - fc) < 800.0f);
                g.setColour(isFc ? Theme::accent : Theme::textMuted);
                g.drawText(lblStr[li], (int)(tx - 14), (int)(tkY + 6), 30, 10,
                           juce::Justification::centred);
            }
            // Label actif sous le thumb (centré)
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(Theme::accent);
            g.drawText(fcLabel, (int)(fcXpx - 16), (int)(tkY + 6), 34, 10,
                       juce::Justification::centred);

            // ── Labels zone droite ────────────────────────────────────────
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(Theme::textMuted.withAlpha(0.80f));
            g.drawText("HEAT",     464, 283, 58,  10, juce::Justification::centred);
            g.drawText("SAT TYPE", 520, 283, 200, 10, juce::Justification::centred);

            // ── Valeur % Drive sous le knob HEAT ──────────────────────────
            {
                float drv = juce::jlimit(0.0f, 1.0f,
                    (float)*audioProcessor.apvts.getRawParameterValue("airDrive"));
                g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
                g.setColour(Theme::accent);
                g.drawText(juce::String(juce::roundToInt(drv * 100)) + " %", 460, 364, 58, 10,
                           juce::Justification::centred);
            }

            // ── TRANSIENT SHAPE — valeur ───────────────────
            {
                float shapeVal = *audioProcessor.apvts.getRawParameterValue("airShape");
                // Valeur
                juce::String shapeTxt;
                juce::Colour shapeCol;
                if      (shapeVal >  0.05f) { shapeTxt = "CRISP +" + juce::String(juce::roundToInt(shapeVal * 100)) + "%"; shapeCol = Theme::accent; }
                else if (shapeVal < -0.05f) { shapeTxt = "SMOOTH " + juce::String(juce::roundToInt(shapeVal * 100)) + "%"; shapeCol = Theme::teal; }
                else                        { shapeTxt = "NEUTRAL"; shapeCol = Theme::textMuted; }
                g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
                g.setColour(shapeCol);
                g.drawText(shapeTxt, 464, 408, 264, 10, juce::Justification::centred);
            }

            // ── CHARACTER — sous le slider de fréquence ───────────────────
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(Theme::textMuted.withAlpha(0.60f));
            g.drawText("CHARACTER", 38, 428, 200, 10, juce::Justification::centredLeft);
            g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
            g.setColour(Theme::accent);
            g.drawText(fc > 12000.0f ? "PURE AIR" : "PRESENCE", 38, 438, 200, 12,
                       juce::Justification::centredLeft);

        }
    }

    // ── Tooltip bar (20px, bas du plugin) — memes couleurs que barre preset ───
    {
        float tbY = BH - 48.0f; // bas de l'espace contenu (hors preset bar 28px)
        // Fond identique a la barre preset
        g.setColour(Theme::surfaceBar);
        g.fillRect(0.0f, tbY, BW, 20.0f);
        // Separateur haut
        g.setColour(Theme::textMuted.withAlpha(0.28f));
        g.fillRect(0.0f, tbY, BW, 1.0f);

        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        if (currentTooltipText.isNotEmpty()) {
            // Nom du parametre en cyan
            g.setColour(Theme::accentDark);
            g.drawText(currentTooltipParam + "  ", 10, (int)tbY, 140, 20, juce::Justification::centredLeft);
            // Description en gris
            g.setColour(Theme::textSecondary);
            g.drawText(currentTooltipText, 148, (int)tbY, (int)BW - 200, 20, juce::Justification::centredLeft);
        } else {
            g.setColour(Theme::textMuted.withAlpha(0.50f));
            g.drawText("Survoler un parametre pour voir sa description", 10, (int)tbY, (int)BW - 20, 20, juce::Justification::centredLeft);
        }
    }

    // ── Valeur Master Gain dessinee dans le header ───────────────────────────
    {
        float gainDB = (float)masterGainSlider.getValue();
        juce::String gainStr = (gainDB >= 0.0f ? "+" : "") + juce::String(gainDB, 1) + " dB";
        g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
        g.setColour(Theme::textDark.withAlpha(0.75f));
        g.drawText(gainStr, 697, 14, 56, 16, juce::Justification::centredLeft);
    }

    // Fin du bloc translaté de 22px (preset bar en haut)
    g.restoreState();
}

// =============================================================================
//  PLACEMENT — ARC FIN (760px, modules 136px aux x=[24,168,312,456,600])
// =============================================================================
void InvisibleFixAudioProcessorEditor::resized()
{
    // Derive le scale depuis la vraie largeur du composant (source de verite)
    currentScale = (float)getWidth() / 760.0f;
    resizeConstrainer.currentSnap = currentScale;
    const float S = currentScale;

    // Helpers : coordonnées base × S
    auto sc = [S](int v) { return (int)(v * S); };

    // ── Façade : knob 94px + textbox 20px = 114px, centré dans 136px → x+21 ──
    const int modX[5] = { 24, 168, 312, 456, 600 };
    int sKnobY  = sc(123);
    int sLabelY = sc(242);

    ModernSlider* facadeSliders[5] = {
        &cleanSlider, &roomSlider, &reverbSlider, &deEssSlider, &airSlider
    };
    for (int i = 0; i < 5; ++i)
    {
        facadeSliders[i]->slider.setBounds(sc(modX[i] + 21), sKnobY, sc(94), sc(114));
        facadeSliders[i]->label.setBounds(sc(modX[i]), sLabelY, sc(136), sc(14));
    }

    // ── BYP et △ dans chaque module ──────────────────────────────────────────
    int bY = sc(97), bW = sc(26), bH = sc(14);
    CustomButton* bypBtns[5] = {
        &cleanBypassBtn, &roomBypassBtn, &reverbBypassBtn, &deEssBypassBtn, &airBypassBtn
    };
    CustomButton* deltaBtns[5] = {
        &deltaBtn, &roomDeltaBtn, &reverbDeltaBtn, &deEssDeltaBtn, &airDeltaBtn
    };
    for (int i = 0; i < 5; ++i)
    {
        bypBtns[i]->button.setBounds(sc(modX[i] + 8),            bY, bW, bH);
        deltaBtns[i]->button.setBounds(sc(modX[i] + 136 - 24), bY, sc(16), bH);
    }

    // ── Master gain ───────────────────────────────────────────────────────────
    // Master Gain : label + boite draggable verticale compacte
    masterGainLabel .setBounds(sc(580), sc(42), sc(80),  sc(16));
    masterGainSlider.setBounds(sc(666), sc(36), sc(28),  sc(28));
    // Bypass global : couvre le wordmark INVISIBLE●FIX (x=22, y=40, 160×28 en virtuel)
    globalBypassBtn .setBounds(sc(22), sc(40), sc(160), sc(28));

    // ── Sliders gain entrée/sortie invisibles (sur les barres meter) ─────────
    inputGainSlider .setBounds(-1, -1, 1, 1);
    outputGainSlider.setBounds(-1, -1, 1, 1);

    // ── Barre preset Arturia — 28px, layout centré ────────────────────────────
    // Indicateur scale — coin bas-droit, toujours visible (position dynamique)
    scaleBtn.setButtonText(juce::String(juce::roundToInt(currentScale * 100)) + " %");
    scaleBtn.setBounds(getWidth() - sc(62), getHeight() - sc(18), sc(58), sc(14));

    // A/B Snapshot — à gauche du undo/redo
    abBtnA   .setBounds(getWidth() - sc(212), getHeight() - sc(18), sc(24), sc(14));
    abArrowBtn.setBounds(getWidth() - sc(186), getHeight() - sc(18), sc(36), sc(14));
    abBtnB   .setBounds(getWidth() - sc(148), getHeight() - sc(18), sc(24), sc(14));

    // Undo / Redo — coin bas-droit, à gauche du bouton scale
    undoBtn.setBounds(getWidth() - sc(122), getHeight() - sc(18), sc(28), sc(14));
    redoBtn.setBounds(getWidth() - sc(92),  getHeight() - sc(18), sc(28), sc(14));

    presetTypeLabel .setVisible(false);
    liveHQBtn.setBounds(sc(644), sc(4), sc(68), sc(20));
    prevPresetBtn   .setBounds(sc(202), sc(2),  sc(24), sc(24));
    presetNameLabel .setBounds(sc(228), sc(0),  sc(302),sc(28));
    nextPresetBtn   .setBounds(sc(532), sc(2),  sc(24), sc(24));
    savePresetBtn   .setBounds(sc(558), sc(5),  sc(42), sc(18));
    initPresetBtn   .setBounds(sc(602), sc(5),  sc(40), sc(18));
    // Bouton Browse (≡) à GAUCHE de la barre preset
    browsePresetBtn .setBounds(sc(4),   sc(4),  sc(24), sc(20));

    // Panel browser : prend toute la hauteur sous la barre preset
    if (presetBrowser != nullptr)
        presetBrowser->setBounds(sc(160), sc(28), sc(440), sc(280));

    // ── Masquer tous les contrôles advanced ──────────────────────────────────
    if (roomVisualizer != nullptr) roomVisualizer->setVisible(false);

    auto hideSlider = [](ModernSlider& ms) {
        ms.slider.setVisible(false); ms.label.setVisible(false);
    };
    hideSlider(advRoomFreqSlider);  hideSlider(advRoomQSlider);  hideSlider(advRoomDepthBoostSlider);
    hideSlider(advDeEssFreqSlider); hideSlider(advDeEssThreshSlider);
    hideSlider(advDeEssIntenSlider); hideSlider(advDeEssAtkSlider);
    hideSlider(advDeEssRelSlider);  hideSlider(advDeEssDetectSlider);
    hideSlider(advGateFloorSlider); hideSlider(advCleanRelSlider);
    hideSlider(advGateThresholdSlider); hideSlider(advGateAttackSlider);
    hideSlider(advGateHoldSlider);
    hideSlider(plosiveProtectSlider);
    hideSlider(advAirDriveSlider);
    hideSlider(nrThreshSlider); hideSlider(nrRatioSlider);
    hideSlider(nrFloorSlider);  hideSlider(nrSmoothSlider);
    hideSlider(nrArtifactBlendSlider);
    hideSlider(deRevFloorSlider);
    hideSlider(deRevBandLFSlider);
    hideSlider(deRevBandMFSlider);
    hideSlider(deRevBandHFSlider);
    if (deRevCanvas) deRevCanvas->setVisible(false);
    deRevLinkBtn.setVisible(false);
    deEssModeBtn.button.setVisible(false);
    advAirFreqSlider.setVisible(false);
    airCharacterLabel.setVisible(false);
    airSatTriodeBtn.setVisible(false);
    airSatClassABtn.setVisible(false);
    airSatTapeBtn  .setVisible(false);
    airSatIronBtn  .setVisible(false);
    advAirShapeSlider.setVisible(false);
    advAirShapeLabel .setVisible(false);

    // Cacher les nouveaux composants graphiques par défaut
    if (nrDualRail   != nullptr) nrDualRail->setVisible(false);
    if (nrBandDisplay != nullptr) nrBandDisplay->setVisible(false);
    nrLearnBtn.setVisible(false);
    nrBypassBtn.setVisible(false);
    gateBypassBtn.setVisible(false);
    gateAutoBtn.setVisible(false);
    if (gateDualRail  != nullptr) gateDualRail->setVisible(false);
    if (gateWaveform  != nullptr) gateWaveform->setVisible(false);
    if (gateAHR       != nullptr) gateAHR->setVisible(false);
    if (deEssAR     != nullptr) deEssAR->setVisible(false);
    if (deEssDetect != nullptr) deEssDetect->setVisible(false);

    // ── Advanced tabs ─────────────────────────────────────────────────────────
    // Knobs advanced: 50px rotary + 16px textbox = 66px total
    // Étiquette paramètre: sous le knob+textbox, h=10
    // advY = y départ des knobs dans le panneau
    // NR panel: x=24 pad=14 → 1er knob à x=38
    //   4 knobs, écart = (296-28-200)/3 = 22.67 → ~23px → pitch=73px
    //   positions x: 38, 111, 184, 257
    // Gate panel: x=334 pad=14 → 1er knob à x=348
    //   5 knobs, pitch=(402-28-250)/4=31px → pitch=81px
    //   positions x: 348, 429, 510, 591, 672

    if (currentTab == AdvancedTab::Clean)
    {
        // ── NR panel: x=24, y=281, w=296, h=155 ─────────────────────────────
        // Left:  6-band activity display    x=34,  y=293, w=118, h=116
        // Right: Sensitivity + Reduction knobs (rotary), then Curve + Speed (horiz)
        //   Knobs row: x=162, y=293, w=44, h=55 each (pitch=58)
        //   Labels:    x=160/218, y=350, w=50, h=10
        //   Horiz sliders: y=367 and y=386, x=162, w=104 + textbox 36px
        if (nrBandDisplay != nullptr) {
            nrBandDisplay->setVisible(true);
            nrBandDisplay->setBounds(sc(34), sc(321), sc(266), sc(116));
        }
        // nrThreshSlider and nrFloorSlider are now controlled by NRSpectrumDisplay drag
        // keep them hidden — nrRatioSlider and nrSmoothSlider remain
        nrRatioSlider.slider.setVisible(true);  nrRatioSlider.label.setVisible(true);
        nrRatioSlider.slider.setBounds(sc(34), sc(445), sc(260), sc(14));
        nrRatioSlider.label.setBounds(sc(296), sc(445), sc(38),  sc(14));
        nrSmoothSlider.slider.setVisible(true); nrSmoothSlider.label.setVisible(true);
        nrSmoothSlider.slider.setBounds(sc(34), sc(463), sc(260), sc(14));
        nrSmoothSlider.label.setBounds(sc(296), sc(463), sc(38),  sc(14));
        nrLearnBtn.setVisible(false);
        nrLearnBtn.setBounds(sc(248), sc(314), sc(54), sc(13));
        nrBypassBtn.setVisible(true);
        nrBypassBtn.setBounds(sc(34), sc(314), sc(60), sc(13));
        nrArtifactBlendSlider.slider.setVisible(true);
        nrArtifactBlendSlider.label .setVisible(true);
        nrArtifactBlendSlider.slider.setBounds(sc(304), sc(346), sc(34), sc(50));
        nrArtifactBlendSlider.label .setBounds(sc(300), sc(398), sc(42), sc(11));

        gateAutoBtn.setVisible(false);
        gateBypassBtn.setVisible(true);
        gateBypassBtn.setBounds(sc(342), sc(314), sc(66), sc(13));
        if (gateWaveform != nullptr) {
            gateWaveform->setVisible(true);
            gateWaveform->setBounds(sc(342), sc(326), sc(186), sc(130));
        }
        if (gateDualRail != nullptr) gateDualRail->setVisible(false);
        if (gateAHR != nullptr) {
            gateAHR->setVisible(true);
            gateAHR->setBounds(sc(534), sc(326), sc(192), sc(60));
        }

        // Plosive protection knob (sous gateAHR)
        plosiveProtectSlider.slider.setVisible(true);
        plosiveProtectSlider.label .setVisible(true);
        plosiveProtectSlider.slider.setBounds(sc(668), sc(462), sc(44), sc(36));
        plosiveProtectSlider.label .setBounds(sc(656), sc(500), sc(68), sc(11));
    }
    else if (currentTab == AdvancedTab::Room)
    {
        // Canvas heatmap : canvX=58, canvY=309, canvW=296, canvH=112
        // (lpX=24 + 34px Y-axis zone = 58; lpY=281 + 28pad = 309)
        if (roomVisualizer != nullptr) {
            addAndMakeVisible(roomVisualizer.get());
            roomVisualizer->setVisible(true);
            roomVisualizer->setBounds(sc(58), sc(337), sc(296), sc(112));
        }
        advRoomFreqSlider.slider.setVisible(true); advRoomFreqSlider.label.setVisible(true);
        advRoomQSlider.slider.setVisible(true);    advRoomQSlider.label.setVisible(true);
        advRoomFreqSlider.slider.setBounds(sc(380), sc(347), sc(50), sc(66));
        advRoomFreqSlider.label.setBounds(sc(378), sc(415), sc(54), sc(10));
        advRoomQSlider.slider.setBounds(sc(476), sc(347), sc(50), sc(66));
        advRoomQSlider.label.setBounds(sc(474), sc(415), sc(54), sc(10));
        advRoomDepthBoostSlider.slider.setVisible(true); advRoomDepthBoostSlider.label.setVisible(true);
        advRoomDepthBoostSlider.slider.setBounds(sc(572), sc(347), sc(50), sc(66));
        advRoomDepthBoostSlider.label.setBounds(sc(570), sc(415), sc(54), sc(10));
    }
    else if (currentTab == AdvancedTab::DeEss)
    {
        // Panel: x=24, y=281, w=712, h=130
        // → DeEssDetect display: x=34, y=289, w=360, h=108  (plus haut, moins large)
        // → AR envelope: x=402, y=289, w=220, h=108          (compact)
        // → Intensity slider vertical: x=628, y=289, w=96, h=108
        if (deEssDetect != nullptr) {
            deEssDetect->setVisible(true);
            deEssDetect->setBounds(sc(34), sc(317), sc(360), sc(84));
        }
        {
            advDeEssDetectSlider.slider.setBounds(sc(34), sc(404), sc(318), sc(16));
            advDeEssDetectSlider.label.setBounds(sc(354), sc(404), sc(18), sc(16));
            advDeEssDetectSlider.slider.setVisible(true);
            advDeEssDetectSlider.label.setVisible(true);
        }
        if (deEssAR != nullptr) {
            deEssAR->setVisible(true);
            deEssAR->setBounds(sc(402), sc(317), sc(218), sc(60));
        }
        {
            auto show = [](ModernSlider& ms) { ms.slider.setVisible(true); ms.label.setVisible(true); };
            advDeEssIntenSlider.slider.setBounds(sc(630), sc(321), sc(30), sc(88));
            advDeEssIntenSlider.label.setBounds(sc(618), sc(411), sc(54), sc(10));
            show(advDeEssIntenSlider);
        }
    }
    else if (currentTab == AdvancedTab::DeRev)
    {
        // Canvas dual-axis : remplace les 4 knobs
        if (deRevCanvas != nullptr)
        {
            deRevCanvas->setVisible(true);
            deRevCanvas->setBounds(sc(36), sc(305), sc(606), sc(120));
        }
        // Knob Floor à droite du canvas
        deRevFloorSlider.slider.setVisible(true);
        deRevFloorSlider.label .setVisible(true);
        deRevFloorSlider.slider.setBounds(sc(656), sc(299), sc(50), sc(66));
        deRevFloorSlider.label .setBounds(sc(654), sc(367), sc(54), sc(10));
        // Bouton LINK sous le knob Floor
        deRevLinkBtn.setVisible(true);
        deRevLinkBtn.setBounds(sc(656), sc(387), sc(52), sc(18));
    }
    else if (currentTab == AdvancedTab::Air)
    {
        advAirFreqSlider.setVisible(true);
        advAirDriveSlider.slider.setVisible(true);
        advAirDriveSlider.label.setVisible(true);
        airCharacterLabel.setVisible(false);
        advAirFreqSlider.setBounds(sc(38), sc(406), sc(414), sc(20));
        // HEAT knob — à gauche de la zone droite
        advAirDriveSlider.slider.setBounds(sc(464), sc(294), sc(50), sc(66));
        advAirDriveSlider.label.setBounds(sc(460), sc(362), sc(58), sc(10));

        // SAT TYPE buttons — 2x2 à droite du knob Heat
        airSatTriodeBtn.setVisible(true);
        airSatClassABtn.setVisible(true);
        airSatTapeBtn  .setVisible(true);
        airSatIronBtn  .setVisible(true);
        airSatTriodeBtn.setBounds(sc(520), sc(292), sc(98), sc(19));
        airSatClassABtn.setBounds(sc(622), sc(292), sc(98), sc(19));
        airSatTapeBtn  .setBounds(sc(520), sc(315), sc(98), sc(19));
        airSatIronBtn  .setBounds(sc(622), sc(315), sc(98), sc(19));

        // TRANSIENT SHAPE — slider bipolaire dans un cadre bleuté sous l'ensemble
        advAirShapeLabel .setVisible(true);
        advAirShapeSlider.setVisible(true);
        advAirShapeLabel .setBounds(sc(464), sc(372), sc(264), sc(11));
        advAirShapeSlider.setBounds(sc(468), sc(385), sc(256), sc(20));
    }
}

void InvisibleFixAudioProcessorEditor::timerCallback()
{
    // BYP blink toutes les 15 frames à 30 Hz = toggle à 1 Hz
    blinkCounter = (blinkCounter + 1) % 15;
    if (blinkCounter == 0)
    {
        bypassBlinkOn = !bypassBlinkOn;
        bypassBtnLAF.blinkOn   = bypassBlinkOn;
        deltaButtonLAF.blinkOn = bypassBlinkOn;
    }

    // Repaint des boutons BYP actifs (clignotement rouge)
    CustomButton* byps[5] = { &cleanBypassBtn, &roomBypassBtn, &reverbBypassBtn,
                               &deEssBypassBtn, &airBypassBtn };
    for (auto* b : byps)
        if (b->button.getToggleState())
            b->button.repaint();

    // Repaint des boutons DELTA actifs (clignotement bleu)
    CustomButton* deltas[5] = { &deltaBtn, &roomDeltaBtn, &reverbDeltaBtn,
                                 &deEssDeltaBtn, &airDeltaBtn };
    for (auto* d : deltas)
        if (d->button.getToggleState())
            d->button.repaint();

    // Blink global bypass (dot + FIX rouge)
    if (audioProcessor.globalBypassActive.load())
        repaint();

    // Mise à jour bouton LIVE/HQ
    {
        bool live = *audioProcessor.apvts.getRawParameterValue("liveMode") > 0.5f;
        liveHQBtn.setButtonText(live ? "LIVE" : "HQ");
        liveHQBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        liveHQBtn.setColour(juce::TextButton::textColourOffId,
            live ? Theme::airWarm : Theme::teal);
        liveHQBtn.repaint();
    }

    // Mise à jour visuelle des boutons SAT MODE (radio)
    if (currentTab == AdvancedTab::Air)
    {
        int satMode = (int)*audioProcessor.apvts.getRawParameterValue("airSatMode");
        juce::TextButton* satBtns[4] = { &airSatTriodeBtn, &airSatClassABtn, &airSatTapeBtn, &airSatIronBtn };
        for (int i = 0; i < 4; ++i)
        {
            bool active = (i == satMode);
            satBtns[i]->setColour(juce::TextButton::buttonColourId,
                active ? Theme::accentDark : Theme::panelDark3);
            satBtns[i]->setColour(juce::TextButton::textColourOffId,
                active ? juce::Colours::white : Theme::textMuted);
            satBtns[i]->repaint();
        }
    }

    // Synchronisation état bouton LEARN avec le flag DSP
    {
        bool snapActive = audioProcessor.nrSnapshotActive;
        if (snapActive != nrLearnBtnActive)
        {
            nrLearnBtnActive = snapActive;
            nrLearnBtn.setColour(juce::TextButton::buttonColourId,
                snapActive ? Theme::teal : Theme::bgDeep);
            nrLearnBtn.setColour(juce::TextButton::textColourOffId,
                snapActive ? Theme::ink : Theme::textMuted);
            nrLearnBtn.repaint();
        }
    }

    // Mise à jour VU level pour les dual rails (NR + Gate) — depuis FFT d'entrée brute
    if (currentTab == AdvancedTab::Clean)
    {
        float sr = (float)audioProcessor.getSampleRate();
        if (sr < 1.0f) sr = 44100.0f;
        float vuMags[2048]{};
        audioProcessor.copyInputFftMagnitudes(vuMags, 2048);
        float sumSq = 0.0f;
        int nBins = juce::jlimit(1, 2047, (int)(4000.0f * 2048.0f / sr));
        for (int b = 1; b <= nBins; ++b) sumSq += vuMags[b] * vuMags[b];
        float rms = std::sqrt(sumSq / (float)nBins) / 2048.0f;
        float levelDB = 20.0f * std::log10(rms + 1e-7f);
        nrVuLevel = juce::jlimit(0.0f, 1.0f, (levelDB + 60.0f) / 60.0f);

        if (nrDualRail != nullptr && nrDualRail->isVisible()) {
            nrDualRail->vuLevel = nrVuLevel;
            nrDualRail->repaint();
        }
        // ── Mise à jour GateWaveformDisplay ──────────────────────────────────
        if (gateWaveform != nullptr && gateWaveform->isVisible())
        {
            float signalDB = audioProcessor.gateCurrentLevelDB;
            // État gate vrai depuis le processeur (pas recalculé)
            bool  gateOpen = audioProcessor.gateIsOpenForDisplay.load();

            // GR normalisé : uniquement quand gate fermé, basé sur le niveau RMS
            float grNorm = 0.0f;
            if (!gateOpen)
            {
                float thDB = -26.0f;
                if (auto* tp = dynamic_cast<juce::RangedAudioParameter*>(
                                   audioProcessor.apvts.getParameter("cleanGateThreshold")))
                    thDB = tp->convertFrom0to1(tp->getValue());
                float flDB = -40.0f;
                if (auto* fp = dynamic_cast<juce::RangedAudioParameter*>(
                                   audioProcessor.apvts.getParameter("cleanGateFloor")))
                    flDB = fp->convertFrom0to1(fp->getValue());
                float range = thDB - flDB;
                if (range > 0.5f)
                    grNorm = juce::jlimit(0.0f, 1.0f, (thDB - signalDB) / range);
            }

            auto* gwDisplay = static_cast<GateWaveformDisplay*>(gateWaveform.get());
            gwDisplay->pushSample(signalDB, gateOpen, grNorm);
            gwDisplay->showAutoLine = gateAutoBtn.getToggleState();

            // ── Détection de vallée AUTO — avec lissage pour stabilité ───────
            // Utilise des membres de classe (pas de static partagé entre instances)

            float hist[80], smooth[80]{};
            std::memcpy(hist, audioProcessor.gateHistogram, sizeof(hist));
            for (int i = 0; i < 80; ++i) {
                smooth[i]  = hist[i];
                if (i > 0)  smooth[i] += hist[i - 1] * 0.5f;
                if (i < 79) smooth[i] += hist[i + 1] * 0.5f;
                smooth[i] /= 2.0f;
            }
            int peak1 = 0;
            for (int i = 1; i < 80; ++i)
                if (smooth[i] > smooth[peak1]) peak1 = i;

            bool zoneValid = false;
            if (smooth[peak1] > 0.5f && peak1 > 12)
            {
                int peak2 = -1;
                for (int i = 0; i < peak1 - 8; ++i)
                    if (peak2 < 0 || smooth[i] > smooth[peak2]) peak2 = i;

                if (peak2 >= 0 && smooth[peak2] > smooth[peak1] * 0.04f)
                {
                    int valley = peak2;
                    for (int i = peak2; i <= peak1; ++i)
                        if (smooth[i] < smooth[valley]) valley = i;

                    float rawValley = (float)valley - 80.0f;
                    if (!autoHasValley) { autoSmoothValley = rawValley; autoHasValley = true; }
                    autoSmoothValley = autoSmoothValley * 0.993f + rawValley * 0.007f;

                    audioProcessor.gateAutoThreshDB  = autoSmoothValley;
                    audioProcessor.gateAutoZoneLow   = autoSmoothValley - 3.0f;
                    audioProcessor.gateAutoZoneHigh  = autoSmoothValley + 3.0f;
                    audioProcessor.gateAutoZoneValid  = true;
                    zoneValid = true;
                }
            }
            if (!zoneValid) audioProcessor.gateAutoZoneValid = false;
            gateAutoBtn.setColour(juce::TextButton::textColourOffId,
                zoneValid ? Theme::accent : Theme::textMuted.withAlpha(0.4f));
        }
        // Repaint des enveloppes AHR pour refléter les changements de paramètres
        if (gateAHR != nullptr && gateAHR->isVisible())
            gateAHR->repaint();
    }

    // Repaint du DeEss Detect si ouvert (barres FFT live)
    if (currentTab == AdvancedTab::DeEss)
    {
        if (deEssDetect != nullptr && deEssDetect->isVisible())
            deEssDetect->repaint();
        if (deEssAR != nullptr && deEssAR->isVisible())
            deEssAR->repaint();
    }

    // Repaint du tiroir Air si ouvert et Air Drive actif (animation halo saturation)
    if (currentTab == AdvancedTab::Air)
    {
        float drv = *audioProcessor.apvts.getRawParameterValue("airDrive");
        if (drv > 0.08f)
            repaint();
    }

    // Repaint NR Spectrum (spectre FFT continu — necessite refresh periodique)
    if (currentTab == AdvancedTab::Clean && nrBandDisplay != nullptr && nrBandDisplay->isVisible())
        nrBandDisplay->repaint();

    // Notch tracker Room : repaint continu pour mise à jour smooth temps réel
    if (currentTab == AdvancedTab::Room)
        repaint();

}

void InvisibleFixAudioProcessorEditor::mouseMove(const juce::MouseEvent& e)
{
    const float S = juce::jmax(0.01f, currentScale);
    // Les coordonnees sont dans l'espace du plugin (les modules sont decales de 28px preset bar)
    const float vx = (float)e.x / S;
    const float vy = (float)e.y / S - 28.0f; // -28 pour ignorer la barre preset

    struct TooltipEntry { float x, y, w, h; const char* param; const char* desc; };
    // Zones principales (modules facade)
    static const TooltipEntry zones[] = {
        // Modules
        {  24,  58, 136, 200, "CLEAN",              "Reducteur de bruit adaptatif + gate intelligent 3 bandes" },
        { 168,  58, 136, 200, "ROOM",               "Notch dynamique : reduit les resonances de piece en temps reel" },
        { 312,  58, 136, 200, "DE-REV",             "Suppresseur de reverb 3 bandes, style Cedar DNS" },
        { 456,  58, 136, 200, "DE-ESS",             "Suppresseur de sibilances adaptatif avec look-ahead 2ms" },
        { 600,  58, 136, 200, "AIR ENHANCER",       "Shelf haute frequence + excitateur harmonique (Drive)" },
        // Header
        {  22,  40, 160,  28, "BYPASS GLOBAL",      "Active/desactive tous les modules simultanement (A/B)" },
        { 580,  40, 120,  28, "MASTER GAIN",        "Gain de sortie global en dB (drag vertical)" },
        // Barre preset (vy < -18 = zone preset bar)
        { 202, -26, 356,  26, "PRESET",             "Preset actif. Clic double pour renommer. Fleches pour naviguer." },
    };

    juce::String newParam, newDesc;
    for (auto& t : zones) {
        if (vx >= t.x && vx < t.x + t.w && vy >= t.y && vy < t.y + t.h) {
            newParam = t.param; newDesc = t.desc; break;
        }
    }

    // Detection sur les composants JUCE via getName()
    if (newParam.isEmpty()) {
        struct CompTip { const char* name; const char* param; const char* desc; };
        static const CompTip compTips[] = {
            { "advRoomFreqSlider",    "ROOM FREQ",       "Frequence cible du notch dynamique (Hz)" },
            { "advRoomQSlider",       "ROOM Q",          "Selectivite du notch (Q eleve = bande etroite)" },
            { "advDeEssFreqSlider",   "DE-ESS FREQ",     "Centre de la zone de detection sibilante (Hz)" },
            { "advDeEssThreshSlider", "DE-ESS THRESH",   "Seuil de declenchement en dB (niveau minimum)" },
            { "advDeEssIntenSlider",  "DE-ESS INTENSITY","Intensite de la reduction (ratio de compression)" },
            { "advDeEssAtkSlider",    "DE-ESS ATTACK",   "Temps d'attaque du detecteur (ms)" },
            { "advDeEssRelSlider",    "DE-ESS RELEASE",  "Temps de relachement du detecteur (ms)" },
            { "advDeEssDetectSlider", "DE-ESS Q",        "Selectivite de la detection : bas=large/tolerant, haut=etroit/chirurgical (ratio spectral Weiss)" },
            { "advGateThresholdSlider","GATE THRESHOLD", "Seuil de declenchement du gate (dBFS)" },
            { "advGateAttackSlider",  "GATE ATTACK",     "Temps d'ouverture du gate (ms)" },
            { "advGateHoldSlider",    "GATE HOLD",       "Temps de maintien apres fermeture (ms)" },
            { "advGateFloorSlider",   "GATE FLOOR",      "Attenuation minimale quand gate ferme (dB)" },
            { "advCleanRelSlider",    "GATE RELEASE",    "Temps de fermeture du gate (ms)" },
            { "plosiveProtectSlider", "PLOSIVE PROTECT", "Protege les plosives (P, B, T) contre la fermeture abusive du gate. 0=desactive, 1=tres sensible." },
            { "nrThreshSlider",       "NR THRESHOLD",    "Seuil de reduction du bruit (dB) - drag sur spectre aussi" },
            { "nrFloorSlider",        "NR FLOOR",        "Niveau plancher de reduction (dB) - drag sur spectre aussi" },
            { "nrRatioSlider",        "NR CURVE",        "Courbe de reduction (ratio compression)" },
            { "nrSmoothSlider",       "NR SMOOTH",       "Lissage temporel de la reduction (evite les artefacts)" },
            { "deRevFloorSlider",   "DE-REV FLOOR", "Limite la reduction maximale du DE-REV par bande" },
            { "deRevBandLFSlider", "DE-REV LF",    "Intensite de reduction sur les basses (<500 Hz) — bas = preserv basses, haut = retire rumble de salle" },
            { "deRevBandMFSlider", "DE-REV MF",    "Intensite de reduction sur les mediums (500-4000 Hz) — zone vocale principale" },
            { "deRevBandHFSlider", "DE-REV HF",    "Intensite de reduction sur les aigus (>4000 Hz) — shimmer et brillance de salle" },
            { "advAirDriveSlider",    "AIR DRIVE",       "Intensite de l'excitateur harmonique haute frequence" },
            { "advAirShapeSlider",    "TRANSIENT SHAPE",  "+ boost les attaques (intelligibilite, presence) / - adoucit les transitoires (chaleur, naturel)" },
            { "nrArtifactBlendSlider","NR ARTIFACT BLEND","Melange vers une reduction plus douce pour limiter les artefacts de pompage" },
            { "airSatTriodeBtn",      "TRIODE",           "Tube triode (ref. Neve 1073, Manley) - harmoniques paires (2e), chaleur asymetrique" },
            { "airSatClassABtn",      "CLASS-A",          "Ampli classe A transistor (ref. SSL 4000, API) - harmoniques impaires (3e), mordant" },
            { "airSatTapeBtn",        "TAPE",             "Bande magnetique (ref. Ampex ATR, Studer A800) - saturation tres douce, densite" },
            { "airSatIronBtn",        "IRON",             "Transformateur de sortie (ref. Neve/Carnhill, API 2520) - genou net, caractere" },
        };

        auto* comp = getComponentAt(e.x, e.y);
        if (comp && comp != this) {
            auto n = comp->getName();
            for (auto& ct : compTips) {
                if (n == ct.name) { newParam = ct.param; newDesc = ct.desc; break; }
            }
            // Fallback : boutons bypass et delta
            if (newParam.isEmpty()) {
                const char* byNames[5] = { "BYP CLEAN","BYP ROOM","BYP DE-REV","BYP DE-ESS","BYP AIR" };
                const char* byDescs[5] = {
                    "Bypass module CLEAN (le bruit passe sans traitement)",
                    "Bypass module ROOM (le notch est desactive)",
                    "Bypass module DE-REV (la reverb n'est pas traitee)",
                    "Bypass module DE-ESS (les sibilances passent sans traitement)",
                    "Bypass module AIR (le shelf et le drive sont desactives)"
                };
                CustomButton* byps[5] = { &cleanBypassBtn, &roomBypassBtn, &reverbBypassBtn, &deEssBypassBtn, &airBypassBtn };
                for (int i = 0; i < 5; ++i)
                    if (comp == &byps[i]->button) { newParam = byNames[i]; newDesc = byDescs[i]; break; }
            }
        }
    }

    if (newParam != currentTooltipParam || newDesc != currentTooltipText) {
        currentTooltipParam = newParam;
        currentTooltipText  = newDesc;
        repaint();
    }
}

void InvisibleFixAudioProcessorEditor::updateBypassAlphas()
{
    ModernSlider* sliders[5] = { &cleanSlider, &roomSlider, &reverbSlider,
                                  &deEssSlider, &airSlider };
    CustomButton* byps[5] = { &cleanBypassBtn, &roomBypassBtn, &reverbBypassBtn,
                               &deEssBypassBtn, &airBypassBtn };
    for (int i = 0; i < 5; ++i)
    {
        float a = byps[i]->button.getToggleState() ? 0.35f : 1.0f;
        sliders[i]->slider.setAlpha(a);
        sliders[i]->label.setAlpha(a);
    }
}

void InvisibleFixAudioProcessorEditor::mouseUp(const juce::MouseEvent& event)
{
    // Clic sur le nom du preset → ouvre/ferme le browser
    if (event.eventComponent == &presetNameLabel)
    {
        togglePresetBrowser();
        return;
    }

    // Convertir le clic (px écran) → espace virtuel (base 760px, hors preset bar 28px)
    // screen = (virtual + 28) * S   →   virtual = screen/S - 28
    const float S = juce::jmax(0.01f, currentScale);
    const juce::Point<float> vClick(
        event.getPosition().x / S,
        event.getPosition().y / S - 28.0f
    );

    AdvancedTab clickedTab = AdvancedTab::None;

    // Zones virtuelles (coordonnées de base 760px) : w=136, y=58 h=200
    if      (juce::Rectangle<float>( 24, 58, 136, 200).contains(vClick)) clickedTab = AdvancedTab::Clean;
    else if (juce::Rectangle<float>(168, 58, 136, 200).contains(vClick)) clickedTab = AdvancedTab::Room;
    else if (juce::Rectangle<float>(312, 58, 136, 200).contains(vClick)) clickedTab = AdvancedTab::DeRev;
    else if (juce::Rectangle<float>(456, 58, 136, 200).contains(vClick)) clickedTab = AdvancedTab::DeEss;
    else if (juce::Rectangle<float>(600, 58, 136, 200).contains(vClick)) clickedTab = AdvancedTab::Air;

    if (clickedTab != AdvancedTab::None)
    {
        if (currentTab == clickedTab) currentTab = AdvancedTab::None;
        else                          currentTab = clickedTab;

        const float S = currentScale;
        if (currentTab == AdvancedTab::None)       setSize((int)(760*S), (int)(338*S));
        else if (currentTab == AdvancedTab::Clean) setSize((int)(760*S), (int)(504*S));
        else                                       setSize((int)(760*S), (int)(478*S));

        resized();
        repaint();
    }
}