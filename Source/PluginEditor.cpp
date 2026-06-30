#include "PluginProcessor.h"
#include "PluginEditor.h"

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
        g.setColour(juce::Colour(0xFFcdddd9));
        g.strokePath(track, juce::PathStrokeType(arcThick,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ── 2. Arc rempli (gradient cyan→teal jusqu'à la valeur) ────────────
        if (sliderPos > 0.005f)
        {
            juce::Path filled;
            filled.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, valAngle, true);
            juce::ColourGradient grad(
                juce::Colour(0xFF13add6), cx - arcR, cy,
                juce::Colour(0xFF1bb89e), cx + arcR, cy + arcR,
                false);
            g.setGradientFill(grad);
            g.strokePath(filled, juce::PathStrokeType(arcThick,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // ── 3. Cercle central (fond verre + ring) ───────────────────────────
        float inR = outerR - inset;
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.fillEllipse(cx - inR, cy - inR, inR * 2.0f, inR * 2.0f);
        g.setColour(juce::Colour(0xFF16b3bf).withAlpha(0.20f));
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
            juce::Colour(0xFF13add6), cx, cy - inR,
            juce::Colour(0xFF1bb89e), cx, cy - inR + needleH,
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
            auto col = blinkOn ? juce::Colour(0xFFFF3333)
                               : juce::Colour(0xFFFF3333).withAlpha(0.20f);
            g.setColour(col);
        }
        else
        {
            g.setColour(juce::Colour(0xFF94a5a2));
        }
        g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
        g.drawText(btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred);
    }
};
static BypassButtonLAF bypassBtnLAF;

// =============================================================================
//  DELTA BUTTON — triangle plus grand, transparent, cyan quand actif
// =============================================================================
class DeltaButtonLAF : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool, bool) override {}

    void drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool) override
    {
        g.setColour(btn.getToggleState() ? juce::Colour(0xFF13add6)
                                         : juce::Colour(0xFF94a5a2));
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
        g.setColour(juce::Colour(0xFFcdddd9));
        g.fillRoundedRectangle((float)x, cy - trackH * 0.5f,
                                (float)width, trackH, 2.0f);

        // Remplissage gauche cyan
        float fillW = sliderPos - (float)x;
        if (fillW > 0.0f) {
            juce::ColourGradient grad(juce::Colour(0xFF13add6).withAlpha(0.50f), (float)x, cy,
                                      juce::Colour(0xFF13add6), sliderPos, cy, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle((float)x, cy - trackH * 0.5f, fillW, trackH, 2.0f);
        }

        // Thumb cercle blanc avec ring teal
        float r = 8.0f;
        g.setColour(juce::Colours::white);
        g.fillEllipse(sliderPos - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colour(0xFF16b3bf));
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
        g.setColour(juce::Colour(0xFF94a5a2));
        for (int di = 0; di < 5; ++di) {
            float dbVal = dbMax - (float)di * (dbMax - dbMin) / 4.0f;
            float yp = railTop + (float)di * railH / 4.0f;
            g.drawText(juce::String((int)dbVal), 0, (int)(yp - 5), 26, 10,
                       juce::Justification::centredRight);
        }

        // Rail background
        g.setColour(juce::Colour(0xFFe4f0ee));
        g.fillRoundedRectangle(railX, railTop, railW, railH, 3.5f);

        // Bande auto-zone (à gauche du rail, bande teal lisible)
        if (autoZoneValid)
        {
            float zoneTopY = dbToY(juce::jlimit(dbMin, dbMax, autoZoneHighDB));
            float zoneBotY = dbToY(juce::jlimit(dbMin, dbMax, autoZoneLowDB));
            float bx = railX - 12.0f;
            float bw = 8.0f;
            // Fond
            g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.25f));
            g.fillRoundedRectangle(bx, zoneTopY, bw, zoneBotY - zoneTopY, 2.5f);
            // Bords lumineux haut et bas
            g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.85f));
            g.fillRoundedRectangle(bx, zoneTopY, bw, 2.0f, 1.0f);
            g.fillRoundedRectangle(bx, zoneBotY - 2.0f, bw, 2.0f, 1.0f);
            // Étiquette dB de la zone optimale (centre de la vallée)
            float centerY = (zoneTopY + zoneBotY) * 0.5f;
            float centerDB = (autoZoneHighDB + autoZoneLowDB) * 0.5f;
            g.setFont(juce::Font(juce::FontOptions(6.5f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF1bb89e));
            g.drawText(juce::String((int)centerDB) + "dB",
                       (int)(bx - 18), (int)(centerY - 5), 18, 10,
                       juce::Justification::centredRight);
        }

        // Zone active (cyan entre les deux thumbs, quel que soit leur ordre)
        {
            float zoneTop = juce::jmin(upperY, lowerY);
            float zoneBot = juce::jmax(upperY, lowerY);
            if (zoneBot > zoneTop + 0.5f) {
                g.setColour(juce::Colour(0xFF13add6).withAlpha(0.18f));
                g.fillRoundedRectangle(railX, zoneTop, railW, zoneBot - zoneTop, 2.0f);
            }
        }

        // VU bar background
        g.setColour(juce::Colour(0xFFe4f0ee));
        g.fillRoundedRectangle(vuX, railTop, vuW, railH, 3.5f);

        // VU bar fill — vuLevel est 0-1 pour -60 dB à 0 dB ; on recalibre sur [dbMin, dbMax]
        {
            float vuDB   = -60.0f + vuLevel * 60.0f;
            float vuFrac = juce::jlimit(0.0f, 1.0f, (vuDB - dbMin) / (dbMax - dbMin));
            float vuBarH = vuFrac * railH;
            if (vuBarH > 0.5f) {
                juce::ColourGradient vuGrad(
                    juce::Colour(0xFF1bb89e), vuX, railBot,
                    juce::Colour(0xFF13add6), vuX, railBot - vuBarH, false);
                g.setGradientFill(vuGrad);
                g.fillRoundedRectangle(vuX, railBot - vuBarH, vuW, vuBarH, 3.5f);
            }
        }

        // Lignes de référence sur VU
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.35f));
        g.fillRect(vuX, upperY, vuW, 1.5f);
        g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.25f));
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
        g.setColour(juce::Colour(0xFF13add6));
        g.drawRoundedRectangle(tX, upperY - tH * 0.5f, tW, tH, 2.5f, 1.5f);

        // Thumb lower (floor)
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(tX, lowerY - tH * 0.5f, tW, tH, 2.5f);
        g.setColour(juce::Colour(0xFF1bb89e));
        g.drawRoundedRectangle(tX, lowerY - tH * 0.5f, tW, tH, 2.5f, 1.5f);

        // Valeurs numériques dB à droite des thumbs
        float labelX = vuX + vuW + 6.0f;
        float labelW = W - labelX;
        g.setFont(juce::Font(juce::FontOptions(8.0f)));

        if (upperParam) {
            g.setColour(juce::Colour(0xFF13add6));
            g.drawText(juce::String(upperDB, 1) + " dB",
                       (int)labelX, (int)(upperY - 5), (int)labelW, 10,
                       juce::Justification::centredLeft);
        }
        if (lowerParam) {
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText(juce::String(lowerDB, 1) + " dB",
                       (int)labelX, (int)(lowerY - 5), (int)labelW, 10,
                       juce::Justification::centredLeft);
        }

        // Labels sous le composant
        g.setFont(juce::Font(juce::FontOptions(7.0f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFF13add6));
        g.drawText(upperLabel, (int)railX - 6, (int)(H - 18), 60, 10,
                   juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xFF94a5a2));
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
        g.setColour(juce::Colour(0xFFe4f0ee).withAlpha(0.60f));
        g.fillRoundedRectangle(0, 0, W, H, 5.0f);
        g.setColour(juce::Colour(0xFFcdddd9).withAlpha(0.40f));
        g.drawRoundedRectangle(0.5f, 0.5f, W - 1, H - 1, 5.0f, 0.5f);

        // Baseline
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.20f));
        g.fillRect(x0, yBot, x3 - x0, 1.0f);

        // Forme enveloppe
        juce::Path env;
        env.startNewSubPath(x0, yBot);
        env.lineTo(x1, yTop);
        if (hasHold) env.lineTo(x2, yTop);
        env.lineTo(x3, yBot);

        juce::Path envFill = env;
        envFill.lineTo(x3, yBot); envFill.lineTo(x0, yBot); envFill.closeSubPath();
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.10f));
        g.fillPath(envFill);

        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.90f));
        g.strokePath(env, juce::PathStrokeType(1.8f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Points de contrôle
        auto drawCtrl = [&](float cx, float cy, juce::Colour col) {
            g.setColour(juce::Colours::white);
            g.fillEllipse(cx - 4.5f, cy - 4.5f, 9.0f, 9.0f);
            g.setColour(col);
            g.drawEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f, 1.5f);
        };
        drawCtrl(x1, yTop, juce::Colour(0xFF13add6));
        if (hasHold) drawCtrl(x2, yTop, juce::Colour(0xFF13add6));
        drawCtrl(x3, yBot, juce::Colour(0xFF1bb89e));

        // Flèches ↕ indiquant la direction de drag
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.55f));
        g.drawText(juce::String::fromUTF8("\xE2\x86\x95"), (int)(x1 - 6), (int)(yTop - 14), 12, 12, juce::Justification::centred);
        if (hasHold) g.drawText(juce::String::fromUTF8("\xE2\x86\x95"), (int)(x2 - 6), (int)(yTop - 14), 12, 12, juce::Justification::centred);
        g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.55f));
        g.drawText(juce::String::fromUTF8("\xE2\x86\x95"), (int)(x3 - 6), (int)(yBot - 12), 12, 12, juce::Justification::centred);

        // Labels segments + valeurs ms — positions fixes (toujours visibles même si segment=0)
        float labY = H + 2.0f;
        float valY = H + 13.0f;
        int nSegs = hasHold ? 3 : 2;
        float slotW = (W - 12.0f) / (float)nSegs;

        auto drawSegFixed = [&](const char* name, juce::RangedAudioParameter* p, int idx, juce::Colour col) {
            float midX = 6.0f + ((float)idx + 0.5f) * slotW;
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF94a5a2));
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
        drawSegFixed("ATK",  atkParam,  0, juce::Colour(0xFF13add6));
        if (hasHold) drawSegFixed("HOLD", holdParam, 1, juce::Colour(0xFF13add6));
        drawSegFixed("REL",  relParam,  hasHold ? 2 : 1, juce::Colour(0xFF1bb89e));
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
        g.setColour(juce::Colour(0xFFe8f2f0).withAlpha(0.55f));
        g.fillRoundedRectangle(0, 0, W, H, 4.0f);

        // ── Barre GR (haut, 7px) ──────────────────────────────────────────
        g.setColour(juce::Colour(0xFFd0e0dc).withAlpha(0.55f));
        g.fillRoundedRectangle(0, 0, W, GR_H, 2.0f);
        if (grSmooth > 0.02f)
        {
            g.setColour(juce::Colour(0xFFb87edb).withAlpha(0.60f));
            g.fillRoundedRectangle(0, 0, grSmooth * W, GR_H, 2.0f);
            g.setFont(juce::Font(juce::FontOptions(6.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF7030a0));
            g.drawText("GR \xe2\x88\x92" + juce::String((int)(grSmooth * 40)) + " dB",
                       3, 0, (int)(grSmooth * W + 2), (int)GR_H, juce::Justification::centredLeft);
        }
        // séparateur
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.18f));
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
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.22f));
        for (float px = 2.0f; px < W; px += 7.0f)
            g.fillRect(px, flY, 4.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.45f));
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
            g.setColour(op ? juce::Colour(0xFF1bb89e).withAlpha(0.50f)
                           : juce::Colour(0xFF94a5a2).withAlpha(0.18f));
            g.fillRect((float)px, y, 1.0f, barH);
            // Bord lumineux au sommet
            g.setColour(op ? juce::Colour(0xFF1bb89e).withAlpha(0.90f)
                           : juce::Colour(0xFF94a5a2).withAlpha(0.40f));
            g.fillRect((float)px, y, 1.0f, 1.5f);
        }

        // ── Ligne threshold (draggable, pointillés cyan) ──────────────────
        // Zone drag visible
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.07f));
        g.fillRect(0.0f, thY - 5.0f, W, 10.0f);
        // Pointillés
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.90f));
        for (float px = 0.0f; px < W; px += 8.0f)
            g.fillRect(px, thY, 5.0f, 1.5f);
        // Label valeur
        g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFF13add6));
        g.drawText(juce::String((int)thDB) + " dB", 3, (int)(thY - 10), 36, 9,
                   juce::Justification::centredLeft);

        // ── Indicateur OPEN / CLOSED ──────────────────────────────────────
        bool latestOpen = isOpen[writePos];
        g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
        g.setColour(latestOpen ? juce::Colour(0xFF1bb89e)
                               : juce::Colour(0xFF94a5a2).withAlpha(0.50f));
        g.drawText(latestOpen ? "\xe2\x97\x8f OPEN" : "\xe2\x97\x8b CLOSED",
                   (int)(W - 50), (int)(WAVE_TOP + 2), 48, 10,
                   juce::Justification::centredRight);

        // ── Échelle de temps ──────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.35f));
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
//  COMPOSANT : DeEssARSliders — Attack / Release en sliders horizontaux
//              + aperçu enveloppe ghost (style cohérent GateAHRSliders)
// =============================================================================
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
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText(labels[i], 4, (int)(cy - 7), 16, 14, juce::Justification::centred);

            // Track de fond
            float ty = cy - 3.0f;
            g.setColour(juce::Colour(0xFFd0e0dc).withAlpha(0.55f));
            g.fillRoundedRectangle(trackX, ty, trackW, 6.0f, 3.0f);

            // Fill teal proportionnel
            float norm = params[i] ? params[i]->getValue() : 0.5f;
            g.setGradientFill(juce::ColourGradient(
                juce::Colour(0xFF0ea693), trackX, 0,
                juce::Colour(0xFF1bb89e), trackX + trackW, 0, false));
            g.fillRoundedRectangle(trackX, ty, trackW * norm, 6.0f, 3.0f);

            // Thumb blanc
            float tx = trackX + norm * trackW - 5.0f;
            g.setColour(juce::Colours::white);
            g.fillEllipse(tx, cy - 5.5f, 11.0f, 11.0f);
            g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.55f));
            g.drawEllipse(tx + 0.5f, cy - 5.0f, 10.0f, 10.0f, 1.0f);

            // Valeur numérique
            float ms = params[i]
                ? params[i]->convertFrom0to1(params[i]->getValue()) * 1000.0f
                : 0.0f;
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(juce::Colour(0xFF1bb89e));
            g.drawText(juce::String(ms, 1) + " ms",
                       (int)(trackX + trackW + 4), (int)(cy - 7), 38, 14,
                       juce::Justification::centredLeft);
        }

        // ── Séparateur ────────────────────────────────────────────────────
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.12f));
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

        g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.12f));
        juce::Path fill = ghost;
        fill.lineTo(ghostX, gBot); fill.closeSubPath();
        g.fillPath(fill);
        g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.60f));
        g.strokePath(ghost, juce::PathStrokeType(1.5f));

        // Labels A / R dans le ghost
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.45f));
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
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText(segs[i].lbl, 2, (int)(ty - 6), (int)(trackX - 4), 12,
                       juce::Justification::centredRight);

            // Piste inactive
            g.setColour(juce::Colour(0xFFcdddd9));
            g.fillRoundedRectangle(trackX, ty - 2.0f, trackW, 4.0f, 2.0f);

            // Remplissage gauche (gradient cyan)
            if (val > 0.01f)
            {
                juce::ColourGradient grad(
                    juce::Colour(0xFF13add6).withAlpha(0.45f), trackX, ty,
                    juce::Colour(0xFF13add6), tx, ty, false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(trackX, ty - 2.0f, tx - trackX, 4.0f, 2.0f);
            }

            // Thumb blanc + ring teal
            g.setColour(juce::Colours::white);
            g.fillEllipse(tx - 7.0f, ty - 7.0f, 14.0f, 14.0f);
            g.setColour(juce::Colour(0xFF16b3bf));
            g.drawEllipse(tx - 6.0f, ty - 6.0f, 12.0f, 12.0f, 1.5f);

            // Valeur en ms
            float ms = segs[i].p
                ? segs[i].p->convertFrom0to1(val) * 1000.0f
                : val * segs[i].maxMs;
            juce::String valStr = (ms < 10.0f)
                ? juce::String(ms, 1) + " ms"
                : juce::String((int)ms) + " ms";
            g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF13add6));
            g.drawText(valStr, (int)(trackX + trackW + 4.0f), (int)(ty - 6), 42, 12,
                       juce::Justification::centredLeft);
        }

        // Séparateur vertical
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.12f));
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
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.14f));
        g.fillRect(gx, bot, ghostW, 1.0f);

        // Fill sous la courbe
        juce::Path fill;
        fill.startNewSubPath(gx, bot);
        fill.lineTo(hx, top); fill.lineTo(hEnd, top); fill.lineTo(rx, bot);
        fill.lineTo(gx + ghostW, bot); fill.lineTo(gx, bot); fill.closeSubPath();
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.09f));
        g.fillPath(fill);

        // Courbe
        juce::Path curve;
        curve.startNewSubPath(gx, bot);
        curve.lineTo(hx, top); curve.lineTo(hEnd, top); curve.lineTo(rx, bot);
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.55f));
        g.strokePath(curve, juce::PathStrokeType(1.8f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Labels zones A H R sous la courbe
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.50f));
        if (hx   - gx   > 8.0f) g.drawText("A", (int)((gx + hx)     * 0.5f - 5), (int)(bot + 2), 10, 8, juce::Justification::centred);
        if (hEnd  - hx  > 6.0f) g.drawText("H", (int)((hx + hEnd)   * 0.5f - 5), (int)(bot + 2), 10, 8, juce::Justification::centred);
        if (rx   - hEnd > 8.0f) g.drawText("R", (int)((hEnd + rx)   * 0.5f - 5), (int)(bot + 2), 10, 8, juce::Justification::centred);

        // Petit label en haut de la zone ghost
        g.setFont(juce::Font(juce::FontOptions(6.5f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.35f));
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

        float newVal = juce::jlimit(0.0f, 1.0f, ((float)e.x - trackX) / trackW);
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

        // Fréquence de détection (2k–16k → log-scale)
        auto normAtFreq = [](float f) { return std::log(f / 2000.0f) / std::log(8.0f); };
        auto xAtFreq    = [&](float f) { return 6.0f + normAtFreq(juce::jlimit(2000.0f, 16000.0f, f)) * (W - 12.0f); };
        float fc = 2000.0f * std::pow(8.0f, freqN);

        // Zone detect : centrée sur fc, largeur proportionnelle à detectN
        float zoneOctaves = 0.5f + detectN * 2.5f;
        float fLow  = juce::jlimit(2000.0f, 16000.0f, fc * std::pow(2.0f, -zoneOctaves * 0.5f));
        float fHigh = juce::jlimit(2000.0f, 16000.0f, fc * std::pow(2.0f,  zoneOctaves * 0.5f));
        float xLow  = xAtFreq(fLow),  xHigh = xAtFreq(fHigh), fcX = xAtFreq(fc);

        // Threshold Y position
        float threshY = 6.0f + (1.0f - threshN) * (H - 16.0f);

        // Canvas background
        g.setColour(juce::Colour(0xFFf0fafc).withAlpha(0.80f));
        g.fillRoundedRectangle(0, 0, W, H, 5.0f);
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.10f));
        g.drawRoundedRectangle(0.5f, 0.5f, W - 1, H - 1, 5.0f, 0.5f);

        // === BARRES FFT LIVE (2k–16kHz) ===
        if (processor != nullptr)
        {
            float sr = (float)processor->getSampleRate();
            if (sr < 1.0f) sr = 44100.0f;
            float fftMags[2048]{};
            processor->copyInputFftMagnitudes(fftMags, 2048);

            int nBarW = (int)W - 12;
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
                g.setColour(aboveThresh ? juce::Colour(0xFF13add6).withAlpha(0.65f)
                                        : juce::Colour(0xFF94a5a2).withAlpha(0.30f));
                g.fillRect(6.0f + (float)px, barY, 1.0f, barH);
            }
        }

        // Zone de détection : Band = rectangle autour de fc, Shelf = dégradé vers la droite
        bool isBellMode = modeParam ? (modeParam->getValue() > 0.5f) : true;
        if (isBellMode) {
            // BAND : zone centrée sur fc avec bords pointillés
            g.setColour(juce::Colour(0xFF13add6).withAlpha(0.10f));
            g.fillRect(xLow, 0.0f, xHigh - xLow, H);
            g.setColour(juce::Colour(0xFF13add6).withAlpha(0.50f));
            for (float py = 4.0f; py < H - 2.0f; py += 7.0f) {
                g.fillRect(xLow,  py, 1.5f, 4.0f);
                g.fillRect(xHigh, py, 1.5f, 4.0f);
            }
        } else {
            // SHELF : dégradé cyan de fc vers le bord droit, symbolisant le shelf
            juce::ColourGradient shelfGrad(
                juce::Colour(0xFF1bb89e).withAlpha(0.22f), fcX, 0.0f,
                juce::Colour(0xFF13add6).withAlpha(0.08f), W - 6.0f, 0.0f,
                false);
            g.setGradientFill(shelfGrad);
            g.fillRect(fcX, 0.0f, W - 6.0f - fcX, H);
            // Bord gauche shelf (ligne solide depuis fc)
            g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.60f));
            g.fillRect(fcX, 0.0f, 1.5f, H);
        }

        // Ligne fc (centre de zone)
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.65f));
        g.fillRect(fcX - 0.75f, 0.0f, 1.5f, H);

        // Ligne threshold — pointillés teal + triangle indicateur
        g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.85f));
        for (float px = 6.0f; px < W - 6.0f; px += 8.0f)
            g.fillRect(px, threshY, 5.0f, 1.5f);

        // Poignée drag threshold (triangle à gauche)
        juce::Path tri;
        tri.startNewSubPath(3.0f, threshY - 5.0f);
        tri.lineTo(10.0f, threshY);
        tri.lineTo(3.0f, threshY + 5.0f);
        tri.closeSubPath();
        g.setColour(juce::Colour(0xFF1bb89e));
        g.fillPath(tri);

        // Valeur threshold
        if (threshParam) {
            float dB = threshParam->convertFrom0to1(threshN);
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF1bb89e));
            g.drawText(juce::String(dB, 1) + " dB",
                       4, (int)(threshY - 11), 42, 10,
                       juce::Justification::centredLeft);
        }

        // Label fc centré sur la ligne de fréquence
        juce::String fcLbl = (fc >= 1000.0f)
            ? juce::String(fc / 1000.0f, 1) + " kHz"
            : juce::String((int)fc) + " Hz";
        g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFF13add6));
        g.drawText(fcLbl, (int)(fcX - 20), 2, 42, 10, juce::Justification::centred);

        // Pill BAND / SHELF — overlay top-right du canvas
        {
            bool isBell = modeParam ? (modeParam->getValue() > 0.5f) : true;
            float pillW = 72.0f, pillH = 14.0f;
            float pillX = W - pillW - 6.0f, pillY = 4.0f;
            float halfW = pillW * 0.5f;

            // Fond pill
            g.setColour(juce::Colour(0xFFe4f0ee).withAlpha(0.85f));
            g.fillRoundedRectangle(pillX, pillY, pillW, pillH, 4.0f);
            g.setColour(juce::Colour(0xFF13add6).withAlpha(0.30f));
            g.drawRoundedRectangle(pillX + 0.5f, pillY + 0.5f, pillW - 1.0f, pillH - 1.0f, 4.0f, 0.8f);

            // Côté actif (highlight cyan)
            if (isBell) {
                g.setColour(juce::Colour(0xFF13add6).withAlpha(0.22f));
                g.fillRoundedRectangle(pillX, pillY, halfW, pillH, 4.0f);
            } else {
                g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.22f));
                g.fillRoundedRectangle(pillX + halfW, pillY, halfW, pillH, 4.0f);
            }

            // Labels
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(isBell ? juce::Colour(0xFF13add6) : juce::Colour(0xFF94a5a2));
            g.drawText("BAND", (int)(pillX), (int)pillY, (int)halfW, (int)pillH, juce::Justification::centred);
            g.setColour(!isBell ? juce::Colour(0xFF1bb89e) : juce::Colour(0xFF94a5a2));
            g.drawText("SHELF", (int)(pillX + halfW), (int)pillY, (int)halfW, (int)pillH, juce::Justification::centred);
        }

        // Axes fréquences sous le canvas
        const float labFreqs[] = { 2000, 4000, 8000, 16000 };
        const char* labStrs[]  = { "2k", "4k", "8k", "16k" };
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        for (int li = 0; li < 4; ++li) {
            float lx = xAtFreq(labFreqs[li]);
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText(labStrs[li], (int)(lx - 10), (int)(H + 4), 22, 10,
                       juce::Justification::centred);
        }

        // Légendes bas
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(juce::Colour(0xFF1bb89e));
        g.drawText("THRESH", 0, (int)(H + 10), 42, 10, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xFF13add6));
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

        g.fillAll(juce::Colour(0xFF060514));

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
            if      (e < 0.10f) { float t = e / 0.10f;             r=lerp8(t,6,36);   gg=lerp8(t,5,11);   b=lerp8(t,20,90);  }
            else if (e < 0.28f) { float t = (e - 0.10f) / 0.18f;   r=lerp8(t,36,136); gg=lerp8(t,11,25);  b=lerp8(t,90,52);  }
            else if (e < 0.52f) { float t = (e - 0.28f) / 0.24f;   r=lerp8(t,136,220);gg=lerp8(t,25,80);  b=lerp8(t,52,14);  }
            else if (e < 0.76f) { float t = (e - 0.52f) / 0.24f;   r=lerp8(t,220,240);gg=lerp8(t,80,160); b=14;              }
            else                { float t = (e - 0.76f) / 0.24f;   r=lerp8(t,240,255);gg=lerp8(t,160,220);b=lerp8(t,14,180); }

            g.setColour(juce::Colour(r, gg, b));
            g.fillRect(px, barY, 1, barH);
        }

        // Grille subtile
        g.setColour(juce::Colour(0xFFFFFFFF).withAlpha(0.05f));
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
        float knobRoom = *apvts.getRawParameterValue("room");
        float fcX   = xAtFreq((float)fc);

        float sigma = wf * (float)(std::log(1.0 + 1.0 / (2.0 * Q)) / std::log(100.0));
        sigma = juce::jmax(2.0f, sigma);

        float baseY   = hf * 0.15f;
        float maxDipY = hf * 0.72f;

        juce::Path curve;
        bool first = true;
        for (int px = 0; px < W; ++px)
        {
            float dx  = (float)px - fcX;
            float dip = std::exp(-0.5f * (dx / sigma) * (dx / sigma));
            float py  = baseY + dip * maxDipY * knobRoom;
            if (first) { curve.startNewSubPath((float)px, py); first = false; }
            else         curve.lineTo((float)px, py);
        }

        {
            juce::Path fill = curve;
            fill.lineTo(wf, baseY);
            fill.lineTo(0.0f, baseY);
            fill.closeSubPath();
            g.setColour(juce::Colour(0xFF13add6).withAlpha(0.08f));
            g.fillPath(fill);
        }

        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.95f));
        g.strokePath(curve, juce::PathStrokeType(2.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Marqueurs ±1σ (bande passante réelle du filtre)
        float xLeft  = fcX - sigma;
        float xRight = fcX + sigma;
        g.setColour(juce::Colour(0xFF13add6).withAlpha(0.45f));
        for (float marker : { xLeft, xRight })
        {
            if (marker < 0.0f || marker > wf) continue;
            for (float py = 0.0f; py < hf; py += 6.0f)
                g.fillRect(marker, py, 1.5f, 2.5f);
        }

        g.setColour(juce::Colour(0xFF1bb89e).withAlpha(0.55f));
        for (float py = 0.0f; py < hf; py += 7.0f)
            g.fillRect(fcX, py, 1.8f, 3.0f);

        g.setColour(juce::Colour(0xFF3A4D5C).withAlpha(0.40f));
        g.drawRect(getLocalBounds(), 1);
    }

private:
    void timerCallback() override { repaint(); }
    InvisibleFixAudioProcessor& processor;
    float displayMags[2048]{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoomVisualizer)
};

// =============================================================================
//  NRBandDisplay — 6-band Cedar noise reduction activity meter
// =============================================================================
class NRBandDisplay : public juce::Component, private juce::Timer
{
public:
    explicit NRBandDisplay(InvisibleFixAudioProcessor* p) : processor(p) { startTimerHz(30); }
    ~NRBandDisplay() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        int W = getWidth(), H = getHeight();
        g.fillAll(juce::Colour(0xFF060514));
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.12f));
        g.drawRect(0, 0, W, H, 1);

        if (!processor) return;
        float gains[6]{};
        processor->copyNRBandGains(gains, 6);

        static const char* labels[] = { "<150", "500", "1.5k", "4k", "8k", ">8k" };
        int labelH = 13;
        int barsH  = H - labelH - 2;
        float barW = (float)(W - 2) / 6.0f;

        for (int b = 0; b < 6; ++b)
        {
            float gain      = juce::jlimit(0.0f, 1.0f, gains[b]);
            float reduction = 1.0f - gain;
            float bx        = 1.0f + (float)b * barW;
            float slotH     = (float)barsH - 4.0f;

            // dark slot
            g.setColour(juce::Colour(0xFF0d1a1e));
            g.fillRect(bx + 1.0f, 3.0f, barW - 2.0f, slotH);

            // active bar (fill from bottom, height = reduction amount)
            if (reduction > 0.01f)
            {
                float fillH = reduction * slotH;
                float fillY = 3.0f + slotH - fillH;
                juce::Colour barCol = juce::Colour(0xFF1bb89e).withAlpha(0.35f + reduction * 0.65f);
                g.setColour(barCol);
                g.fillRect(bx + 1.0f, fillY, barW - 2.0f, fillH);

                // bright top edge
                g.setColour(juce::Colour(0xFF1bb89e));
                g.fillRect(bx + 1.0f, fillY, barW - 2.0f, 1.5f);
            }

            // frequency label
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText(labels[b], (int)bx, H - labelH, (int)barW, labelH,
                       juce::Justification::centred);
        }

        // faint horizontal guide at -12 dB (gain ~0.25 → reduction 0.75)
        float guideY = 3.0f + (float)barsH * (1.0f - 0.75f) - 4.0f;
        g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.08f));
        g.drawHorizontalLine((int)guideY, 1.0f, (float)(W - 1));
    }

private:
    void timerCallback() override { repaint(); }
    InvisibleFixAudioProcessor* processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NRBandDisplay)
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

    // Master Gain (juce::Slider direct)
    masterGainSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    masterGainSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(masterGainSlider);

    masterGainLabel.setText("MASTER GAIN", juce::dontSendNotification);
    masterGainLabel.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
    masterGainLabel.setJustificationType(juce::Justification::centredRight);
    masterGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF26332f));
    addAndMakeVisible(masterGainLabel);

    // Master gain slider : Arc Fin horizontal style
    masterGainSlider.setColour(juce::Slider::thumbColourId,       juce::Colour(0xFF16b3bf));
    masterGainSlider.setColour(juce::Slider::trackColourId,       juce::Colour(0xFFcdddd9));
    masterGainSlider.setColour(juce::Slider::backgroundColourId,  juce::Colour(0xFFcdddd9));
    masterGainSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFF94a5a2));
    masterGainSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xB3FFFFFF));
    masterGainSlider.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xB3C8DCDC));

    // Descriptions légères sous les knobs façade
    auto applyFacadeDesc = [](ModernSlider& ms, const juce::String& desc) {
        ms.label.setText(desc, juce::dontSendNotification);
        ms.label.setFont(juce::Font(juce::FontOptions(10.5f)));
        ms.label.setJustificationType(juce::Justification::centred);
        ms.label.setColour(juce::Label::textColourId, juce::Colour(0xFF94a5a2));
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
    deltaBtn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF94a5a2));
    deltaBtn.button.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xFF13add6));
    deltaBtn.button.setLookAndFeel(&deltaButtonLAF);
    addAndMakeVisible(deltaBtn.button);

    auto setupDeltaBtn = [&](CustomButton& btn) {
        btn.button.setButtonText(juce::String::fromUTF8("\xCE\x94"));
        btn.button.setClickingTogglesState(true);
        btn.button.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        btn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF94a5a2));
        btn.button.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xFF13add6));
        btn.button.setLookAndFeel(&deltaButtonLAF);
        addAndMakeVisible(btn.button);
    };
    setupDeltaBtn(roomDeltaBtn);
    setupDeltaBtn(reverbDeltaBtn);
    setupDeltaBtn(deEssDeltaBtn);
    setupDeltaBtn(airDeltaBtn);

    // Bouton De-Ess Mode
    deEssModeBtn.button.setClickingTogglesState(true);
    deEssModeBtn.button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFFFFFF));
    deEssModeBtn.button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF00E5FF).withAlpha(0.12f));
    deEssModeBtn.button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF2C3E50).withAlpha(0.6f));
    deEssModeBtn.button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF00B1CC));

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
    setupModernSlider(advDeEssFreqSlider,    "De-Ess Freq");
    setupModernSlider(advDeEssThreshSlider,  "Thresh dB");
    setupModernSlider(advDeEssIntenSlider,   "Intensity");
    // Intensity → slider vertical pour De-Ess (moins encombrant qu'un knob)
    advDeEssIntenSlider.slider.setSliderStyle(juce::Slider::LinearVertical);
    advDeEssIntenSlider.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 14);
    advDeEssIntenSlider.slider.setColour(juce::Slider::trackColourId,       juce::Colour(0xFFcdddd9));
    advDeEssIntenSlider.slider.setColour(juce::Slider::thumbColourId,       juce::Colour(0xFF13add6));
    advDeEssIntenSlider.slider.setColour(juce::Slider::backgroundColourId,  juce::Colour(0xFFe4f0ee));
    advDeEssIntenSlider.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFF26332f));
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

    // Air drive
    setupModernSlider(advAirDriveSlider, "Heat");

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
    advRoomQAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "roomNotchQ", advRoomQSlider.slider);

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

    // Air drive
    advAirDriveAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "airDrive", advAirDriveSlider.slider);

    // NR — Noise Reduction (Cedar-aligned labels)
    setupModernSlider(nrThreshSlider, "Sensitivity");
    setupModernSlider(nrRatioSlider,  "Curve");
    setupModernSlider(nrFloorSlider,  "Reduction");
    setupModernSlider(nrSmoothSlider, "Speed");
    nrThreshAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrThreshold", nrThreshSlider.slider);
    nrRatioAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrRatio",     nrRatioSlider.slider);
    nrFloorAttach  = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrFloor",     nrFloorSlider.slider);
    nrSmoothAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "nrSmooth",    nrSmoothSlider.slider);

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

    // NR Band Activity display (6-band Cedar meter)
    nrBandDisplay = std::make_unique<NRBandDisplay>(&audioProcessor);
    addChildComponent(nrBandDisplay.get());

    // Bouton LEARN — snapshot plancher de bruit
    nrLearnBtn.setButtonText("LEARN");
    nrLearnBtn.setColour(juce::TextButton::buttonColourId,    juce::Colour(0xFF0d1a1e));
    nrLearnBtn.setColour(juce::TextButton::buttonOnColourId,  juce::Colour(0xFF1bb89e));
    nrLearnBtn.setColour(juce::TextButton::textColourOffId,   juce::Colour(0xFF94a5a2));
    nrLearnBtn.setColour(juce::TextButton::textColourOnId,    juce::Colour(0xFF060514));
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
            nrLearnBtnActive ? juce::Colour(0xFF1bb89e) : juce::Colour(0xFF0d1a1e));
        nrLearnBtn.setColour(juce::TextButton::textColourOffId,
            nrLearnBtnActive ? juce::Colour(0xFF060514) : juce::Colour(0xFF94a5a2));
        nrLearnBtn.repaint();
    };
    addChildComponent(nrLearnBtn);

    // Bouton AUTO gate — détecte la zone optimale et règle le threshold
    gateAutoBtn.setButtonText("AUTO");
    gateAutoBtn.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF0d1a1e));
    gateAutoBtn.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xFF13add6));
    gateAutoBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF94a5a2));
    gateAutoBtn.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xFF060514));
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

    // Gate AHR — sliders horizontaux + ghost envelope (style B+)
    {
        auto* ahr      = new GateAHRSliders();
        ahr->atkParam  = getParam("cleanGateAttack");
        ahr->holdParam = getParam("cleanGateHold");
        ahr->relParam  = getParam("cleanRelease");
        gateAHR.reset(ahr);
    }
    addChildComponent(gateAHR.get());

    // De-Ess AR — sliders horizontaux Attack / Release (style DeEssARSliders)
    {
        auto* ar      = new DeEssARSliders();
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

    // Initialiser l'alpha des modules bypass selon l'état courant
    updateBypassAlphas();

    // Timer 30Hz : VU fluide + blink BYP via compteur de frames
    startTimerHz(30);

    setSize(760, 290);
}

InvisibleFixAudioProcessorEditor::~InvisibleFixAudioProcessorEditor() = default;

// =============================================================================
//  APPLICATION DU STYLE AUX SLIDERS
// =============================================================================
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
    modernSlider.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFF26332f));
    modernSlider.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    modernSlider.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    modernSlider.slider.setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xFF13add6).withAlpha(0.25f));

    applyModernKnobStyle(modernSlider.slider);
    addAndMakeVisible(modernSlider.slider);

    // Libellé paramètre (sous la valeur) — Arc Fin: 8px, couleur muted
    modernSlider.label.setText(name, juce::dontSendNotification);
    modernSlider.label.setFont(juce::Font(juce::FontOptions(8.0f)));
    modernSlider.label.setJustificationType(juce::Justification::centred);
    modernSlider.label.setColour(juce::Label::textColourId, juce::Colour(0xFF94a5a2));
    addAndMakeVisible(modernSlider.label);
}

// =============================================================================
//  DESSIN GRAPHIQUE — ARC FIN
// =============================================================================
void InvisibleFixAudioProcessorEditor::paint(juce::Graphics& g)
{
    int W = getWidth(), H = getHeight();

    // ── Fond dégradé 145° ebf4f7 → e4f0ee → ddeaf0 ──────────────────────────
    {
        juce::ColourGradient bg(juce::Colour(0xFFebf4f7), 0.0f, 0.0f,
                                juce::Colour(0xFFddeaf0), (float)W, (float)H, false);
        bg.addColour(0.55, juce::Colour(0xFFe4f0ee));
        g.setGradientFill(bg);
        g.fillRoundedRectangle(0, 0, W, H, 9.0f);
    }

    // ── Bordure extérieure glass ─────────────────────────────────────────────
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawRoundedRectangle(0.5f, 0.5f, (float)W-1, (float)H-1, 9.0f, 1.0f);

    // ── Header ───────────────────────────────────────────────────────────────
    // Wordmark gauche : INVISIBLE ● FIX (positions calculées sur largeur réelle)
    int hY = 12, hH = 28;
    auto titleFont = juce::Font(juce::FontOptions(17.0f));
    g.setFont(titleFont);
    g.setColour(juce::Colour(0xFF26332f));
    g.drawText("INVISIBLE", 22, hY, 120, hH, juce::Justification::centredLeft);

    // Calculer la largeur réelle du texte pour coller le point
    float invisW = juce::GlyphArrangement::getStringWidth(titleFont, "INVISIBLE");
    float dotX = 22.0f + invisW + 5.0f;
    float dotY = (float)hY + (float)hH * 0.5f - 3.0f;
    g.setColour(juce::Colour(0xFF13add6));
    g.fillEllipse(dotX, dotY, 6.0f, 6.0f);

    g.setFont(titleFont);
    g.setColour(juce::Colour(0xFF1bb89e));
    g.drawText("FIX", (int)(dotX + 9.0f), hY, 40, hH, juce::Justification::centredLeft);

    // Version — droite du header
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.setColour(juce::Colour(0xFF94a5a2));
    g.drawText("V0.8  LAB STUDIO", W - 345, hY, 110, hH, juce::Justification::centredRight);

    // ── Cartes modules ───────────────────────────────────────────────────────
    // x positions : [24,168,312,456,600], w=136, y=58, h=200
    const int modX[5]   = { 24, 168, 312, 456, 600 };
    const int modW = 136, modY = 58, modH = 200;
    const char* modNames[5] = { "CLEAN", "ROOM", "DE-REV", "DE-ESS", "AIR ENHANCER" };

    AdvancedTab tabForMod[5] = {
        AdvancedTab::Clean, AdvancedTab::Room,
        AdvancedTab::None,  AdvancedTab::DeEss, AdvancedTab::Air
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
        g.setColour(juce::Colour(0xFF283050).withAlpha(0.07f));
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
            g.setColour(juce::Colour(0xFF13add6));
            g.drawRoundedRectangle(fx - 1, fy - 1, fw + 2, fh + 2, r, 1.5f);
        }

        // Nom module (centré, y=modY+22)
        g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFF26332f).withAlpha(bypassed[i] ? 0.40f : 1.0f));
        g.drawText(modNames[i], modX[i], modY + 22, modW, 15, juce::Justification::centred);

        // LED — cyan uniquement pour le module dont l'onglet avancé est ouvert
        float ledX = fx + fw * 0.5f - 3.5f;
        float ledY = fy + fh - 18.0f;
        bool ledActive = (currentTab == tabForMod[i])
                      && (tabForMod[i] != AdvancedTab::None)
                      && !bypassed[i];
        g.setColour(ledActive ? juce::Colour(0xFF13add6) : juce::Colour(0xFFcdddd9));
        g.fillEllipse(ledX, ledY, 7.0f, 7.0f);
    }

    // ── Section Advanced ─────────────────────────────────────────────────────
    if (currentTab != AdvancedTab::None)
    {
        // Label "ADVANCED CONFIGURATION"
        g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
        g.setColour(juce::Colour(0xFF94a5a2));
        g.drawText("ADVANCED CONFIGURATION", 24, 265, 300, 13, juce::Justification::centredLeft);

        auto drawAdvPanel = [&](float px, float py, float pw, float ph, const juce::String& title)
        {
            g.setColour(juce::Colours::white.withAlpha(0.40f));
            g.fillRoundedRectangle(px, py, pw, ph, 9.0f);
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.drawRoundedRectangle(px, py, pw, ph, 9.0f, 1.0f);
            g.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText(title, px + 14, py + 12, pw - 28, 11, juce::Justification::centredLeft);
        };

        if (currentTab == AdvancedTab::Clean)
        {
            drawAdvPanel(24, 281, 296, 155, "NOISE REDUCTION");
            drawAdvPanel(334, 281, 402, 155, "GATE");

            // Separateur vertical NR : band display / knobs
            g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.10f));
            g.fillRect(157.0f, 294.0f, 1.0f, 132.0f);

            // Separateur vertical Gate : rail / enveloppe
            g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.12f));
            g.fillRect(462.0f, 294.0f, 1.0f, 132.0f);

            // Petit label "NR ACTIVITY" au-dessus du display bandes
            g.setFont(juce::Font(juce::FontOptions(7.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.6f));
            g.drawText("NR ACTIVITY", 34, 294, 118, 9, juce::Justification::centred);
        }
        else if (currentTab == AdvancedTab::DeEss)
        {
            drawAdvPanel(24, 281, (float)(W - 48), 130, "DE-ESSER ADVANCED");
        }
        else if (currentTab == AdvancedTab::Room)
        {
            // ── Panneau gauche : NOTCH ANALYSIS (x=24, w=330, h=152) ─────
            float lpX = 24.0f, lpY = 281.0f, lpW = 330.0f, lpH = 152.0f;
            drawAdvPanel(lpX, lpY, lpW, lpH, "NOTCH ANALYSIS");

            // Y-axis labels (dB) alignés à gauche du canvas
            float yLabX = lpX + 8.0f;
            float canvX = lpX + 34.0f;  // canvas commence après zone Y-axis
            float canvY = lpY + 28.0f;
            float canvW = lpW - 44.0f;  // laisse 10px à droite
            float canvH = 112.0f;

            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(juce::Colour(0xFF94a5a2));
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
                g.setColour(near ? juce::Colour(0xFF13add6) : juce::Colour(0xFF94a5a2));
                g.setFont(juce::Font(juce::FontOptions(near ? 7.5f : 7.0f)
                          .withStyle(near ? "Bold" : "Regular")));
                g.drawText(xl.lbl, (int)(lx - 14), (int)xAxisY, 30, 10,
                           juce::Justification::centred);
            }

            // ── Panneau droit : NOTCH PARAMETERS (x=364, w=372, h=152) ──
            float rpX = 364.0f, rpY = 281.0f, rpW = 372.0f, rpH = 152.0f;
            drawAdvPanel(rpX, rpY, rpW, rpH, "NOTCH PARAMETERS");

            // Labels au-dessus des knobs
            g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText("NOTCH FREQ", (int)(rpX + 16), (int)(rpY + 26), 80, 10,
                       juce::Justification::centredLeft);
            g.drawText("NOTCH Q",    (int)(rpX + 112), (int)(rpY + 26), 60, 10,
                       juce::Justification::centredLeft);

            // ── Chip DOMINANT RESONANCE — détection FFT réelle ────────────────────
            {
                float sr2 = (float)audioProcessor.getSampleRate();
                if (sr2 < 1.0f) sr2 = 44100.0f;
                float displayMagsChip[2048]{};
                audioProcessor.copyFftMagnitudes(displayMagsChip, 2048);

                int binLow  = juce::jlimit(1, 2047, (int)(100.0f  * 2048.0f / sr2));
                int binHigh = juce::jlimit(1, 2047, (int)(2000.0f * 2048.0f / sr2));
                int   peakBin = binLow;
                float peakMag = 0.0f;
                for (int b = binLow; b <= binHigh; ++b)
                {
                    if (displayMagsChip[b] > peakMag)
                    { peakMag = displayMagsChip[b]; peakBin = b; }
                }
                float peakFreqFFT = (float)peakBin * sr2 / 2048.0f;
                float peakDB      = 20.0f * std::log10(peakMag / 2048.0f + 1e-7f);
                bool  hasSignal   = (peakDB > -55.0f);

                double QD2   = juce::jlimit(0.5, 15.0,
                    (double)*audioProcessor.apvts.getRawParameterValue("roomNotchQ"));
                double bwFFT = (double)peakFreqFFT / QD2;
                int fLowFFT  = juce::jmax(1, (int)(peakFreqFFT - bwFFT * 0.5));
                int fHighFFT = (int)(peakFreqFFT + bwFFT * 0.5);

                float chipX = rpX + 10.0f, chipY = rpY + 96.0f;
                float chipW = rpW - 20.0f, chipH = 48.0f;
                g.setColour(juce::Colour(0xFF13add6).withAlpha(0.06f));
                g.fillRoundedRectangle(chipX, chipY, chipW, chipH, 6.0f);
                g.setColour(juce::Colour(0xFF13add6).withAlpha(0.14f));
                g.drawRoundedRectangle(chipX, chipY, chipW, chipH, 6.0f, 1.0f);

                g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
                g.setColour(juce::Colour(0xFF94a5a2));
                g.drawText("DOMINANT RESONANCE", (int)chipX, (int)(chipY + 7), (int)chipW, 10,
                           juce::Justification::centred);

                if (hasSignal)
                {
                    juce::String zoneTxt = "Zone estim" + juce::String::fromUTF8("\xC3\xA9") + "e   "
                        + juce::String(fLowFFT) + " \xe2\x80\x93 " + juce::String(fHighFFT) + " Hz";
                    g.setFont(juce::Font(juce::FontOptions(8.0f)));
                    g.setColour(juce::Colour(0xFF7ab8c0));
                    g.drawText(zoneTxt, (int)chipX, (int)(chipY + 20), (int)chipW, 10,
                               juce::Justification::centred);

                    juce::String centTxt = "Centro" + juce::String::fromUTF8("\xC3\xAF") + "de \xC2\xB7 "
                        + juce::String((int)peakFreqFFT) + " Hz \xC2\xB7 Q "
                        + juce::String(QD2, 2);
                    g.setFont(juce::Font(juce::FontOptions(7.5f)));
                    g.setColour(juce::Colour(0xFF94a5a2));
                    g.drawText(centTxt, (int)chipX, (int)(chipY + 32), (int)chipW, 10,
                               juce::Justification::centred);
                }
                else
                {
                    g.setFont(juce::Font(juce::FontOptions(8.0f)));
                    g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.5f));
                    g.drawText("en attente de signal audio...",
                               (int)chipX, (int)(chipY + 22), (int)chipW, 10,
                               juce::Justification::centred);
                }
            }
        }
        else if (currentTab == AdvancedTab::Air)
        {
            drawAdvPanel(24, 281, (float)(W - 48), 152, "AIR CHARACTER");

            // Fréquence de coupure (lue depuis l'APVTS = même valeur que le slider)
            float fc = juce::jlimit(500.0f, 20000.0f,
                        (float)*audioProcessor.apvts.getRawParameterValue("airFreq"));

            // Conversion log → pixel (500Hz–20kHz)
            auto freqToX = [&](float f, float x0, float w2) -> float {
                float t = (std::log10(f) - std::log10(500.0f))
                        / (std::log10(20000.0f) - std::log10(500.0f));
                return x0 + t * w2;
            };

            // ── Canvas spectre ────────────────────────────────────────────
            float sX = 38.0f, sY = 302.0f, sW = 414.0f, sH = 72.0f;
            // Position du thumb alignée sur AirFreqSliderLAF (r=8px, plage APVTS)
            float thumbR = 8.0f;
            float sliderMin = (float)advAirFreqSlider.getMinimum();
            float sliderMax = (float)advAirFreqSlider.getMaximum();
            float fcNorm = (fc - sliderMin) / (sliderMax - sliderMin);
            float fcXpx = sX + thumbR + fcNorm * (sW - 2.0f * thumbR);

            g.setColour(juce::Colour(0xFFf0fafc).withAlpha(0.70f));
            g.fillRoundedRectangle(sX, sY, sW, sH, 5.0f);
            g.setColour(juce::Colour(0xFF13add6).withAlpha(0.12f));
            g.drawRoundedRectangle(sX, sY, sW, sH, 5.0f, 1.0f);

            float zeroY   = sY + sH - 10.0f;
            float maxGainY = sY + 8.0f;

            // 0 dB pointillé
            g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.32f));
            for (float d = sX + 4.0f; d < sX + sW; d += 9.0f)
                g.fillRect(d, zeroY, 4.0f, 1.0f);

            // Label "+ AIR BOOST" et "0 dB" sur le canvas
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(juce::Colour(0xFF13add6).withAlpha(0.55f));
            g.drawText("+ AIR BOOST", (int)(fcXpx + 8), (int)(maxGainY + 6), 60, 10,
                       juce::Justification::centredLeft);
            g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.60f));
            g.drawText("0 dB", (int)(sX + sW - 28), (int)(zeroY - 10), 28, 10,
                       juce::Justification::centredRight);

            // Courbe high-shelf
            juce::Path shelf;
            for (int px = 0; px < (int)sW; ++px)
            {
                float freq = 500.0f * std::pow(40.0f, (float)px / sW);
                float sig  = 1.0f / (1.0f + std::exp(-5.5f * std::log10(freq / fc)));
                float py   = zeroY - sig * (zeroY - maxGainY);
                if (px == 0) shelf.startNewSubPath(sX, py);
                else         shelf.lineTo(sX + (float)px, py);
            }

            // Remplissage gauche (neutre) + droite (cyan)
            {
                juce::Path fillL = shelf;
                fillL.lineTo(fcXpx, zeroY); fillL.lineTo(sX, zeroY); fillL.closeSubPath();
                g.setColour(juce::Colour(0xFF94a5a2).withAlpha(0.08f));
                g.fillPath(fillL);

                juce::Path fillR;
                bool started = false;
                for (int px = 0; px < (int)sW; ++px)
                {
                    float cx2 = sX + (float)px;
                    if (cx2 < fcXpx) continue;
                    float freq = 500.0f * std::pow(40.0f, (float)px / sW);
                    float sig  = 1.0f / (1.0f + std::exp(-5.5f * std::log10(freq / fc)));
                    float py   = zeroY - sig * (zeroY - maxGainY);
                    if (!started) { fillR.startNewSubPath(cx2, zeroY); fillR.lineTo(cx2, py); started = true; }
                    else          fillR.lineTo(cx2, py);
                }
                if (started) { fillR.lineTo(sX + sW, zeroY); fillR.closeSubPath(); }
                juce::ColourGradient cg(juce::Colour(0xFF13add6).withAlpha(0.22f), fcXpx, sY,
                                        juce::Colour(0xFF1bb89e).withAlpha(0.38f), sX+sW, sY, false);
                g.setGradientFill(cg);
                g.fillPath(fillR);
            }

            // Stroke courbe
            g.setColour(juce::Colour(0xFF13add6).withAlpha(0.92f));
            g.strokePath(shelf, juce::PathStrokeType(1.5f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Ligne verticale fc — position exactement sur le thumb du slider
            float airDriveVal = juce::jlimit(0.0f, 1.0f,
                (float)*audioProcessor.apvts.getRawParameterValue("airDrive"));
            bool satActive = (airDriveVal > 0.08f);

            float lineAlpha = satActive ? (bypassBlinkOn ? 1.0f : 0.60f) : 1.0f;
            float lineWidth = satActive ? (bypassBlinkOn ? 3.0f : 1.5f)  : 1.5f;

            g.setColour(juce::Colour(0xFF13add6).withAlpha(lineAlpha));
            g.fillRect(fcXpx - lineWidth * 0.5f, sY, lineWidth, sH);

            if (satActive)
            {
                float haloW = 4.0f + airDriveVal * 14.0f;
                juce::ColourGradient haloR(
                    juce::Colour(0xFF13add6).withAlpha(airDriveVal * 0.30f), fcXpx, sY,
                    juce::Colours::transparentBlack, fcXpx + haloW, sY, false);
                g.setGradientFill(haloR);
                g.fillRect(fcXpx, sY, haloW, sH);

                juce::ColourGradient haloL(
                    juce::Colour(0xFF13add6).withAlpha(airDriveVal * 0.30f), fcXpx, sY,
                    juce::Colours::transparentBlack, fcXpx - haloW, sY, false);
                g.setGradientFill(haloL);
                g.fillRect(fcXpx - haloW, sY, haloW, sH);
            }

            // Label fréquence au-dessus du canvas (suit la ligne)
            juce::String fcLabel = (fc >= 1000.0f)
                ? (juce::String(fc / 1000.0f, 1) + " kHz")
                : (juce::String((int)fc) + " Hz");
            g.setFont(juce::Font(juce::FontOptions(8.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF13add6));
            g.drawText(fcLabel, (int)(fcXpx - 20), (int)(sY - 14), 44, 12,
                       juce::Justification::centred);

            // ── Slider track (dessiné dans paint pour les ticks & labels) ─
            float tkY = sY + sH + 14.0f;  // y centre de la track (slider JUCE y=tkY-10 h=20)

            // Ticks au-dessus du slider
            const float tickFreqs[] = { 500, 1000, 2000, 5000, 8500, 10000, 20000 };
            for (float tf : tickFreqs)
            {
                float tx = freqToX(tf, sX, sW);
                bool isFc = (std::fabs(tf - fc) < fc * 0.15f);
                g.setColour(isFc ? juce::Colour(0xFF13add6) : juce::Colour(0xFFcdddd9));
                g.fillRect(tx, tkY - 5.0f, isFc ? 2.0f : 1.0f, 5.0f);
            }

            // Labels fréquences sous le slider
            const char* lblStr[] = { "500 Hz","1k","2k","5k","","10k","20k" };
            g.setFont(juce::Font(juce::FontOptions(7.5f)));
            for (int li = 0; li < 7; ++li)
            {
                if (juce::String(lblStr[li]).isEmpty()) continue;
                float tx = freqToX(tickFreqs[li], sX, sW);
                bool isFc = (std::fabs(tickFreqs[li] - fc) < tickFreqs[li] * 0.15f);
                g.setColour(isFc ? juce::Colour(0xFF13add6) : juce::Colour(0xFF94a5a2));
                g.drawText(lblStr[li], (int)(tx - 14), (int)(tkY + 6), 30, 10,
                           juce::Justification::centred);
            }
            // Label actif sous le thumb (centré)
            g.setFont(juce::Font(juce::FontOptions(7.5f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF13add6));
            g.drawText(fcLabel, (int)(fcXpx - 16), (int)(tkY + 6), 34, 10,
                       juce::Justification::centred);

            // ── Air Drive knob zone (droite) ──────────────────────────────
            // Knob dessiné par le composant child (setBounds dans resized)
            // Juste le label valeur et sub-label

            // ── Chip CHARACTER ────────────────────────────────────────────
            float chipX = 464.0f, chipY2 = 380.0f, chipW = 264.0f, chipH = 48.0f;
            g.setColour(juce::Colour(0xFF13add6).withAlpha(0.07f));
            g.fillRoundedRectangle(chipX, chipY2, chipW, chipH, 6.0f);
            g.setColour(juce::Colour(0xFF13add6).withAlpha(0.16f));
            g.drawRoundedRectangle(chipX, chipY2, chipW, chipH, 6.0f, 1.0f);
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(juce::Colour(0xFF94a5a2));
            g.drawText("CHARACTER", (int)chipX, (int)(chipY2 + 10), (int)chipW, 10,
                       juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
            g.setColour(juce::Colour(0xFF13add6));
            g.drawText(fc > 10000.0f ? "PURE AIR" : "PRESENCE",
                       (int)chipX, (int)(chipY2 + 24), (int)chipW, 14,
                       juce::Justification::centred);
        }
    }
}

// =============================================================================
//  PLACEMENT — ARC FIN (760px, modules 136px aux x=[24,168,312,456,600])
// =============================================================================
void InvisibleFixAudioProcessorEditor::resized()
{
    // ── Façade : knob 94px + textbox 20px = 114px, centré dans 136px → x+21 ──
    const int modX[5] = { 24, 168, 312, 456, 600 };
    int sKnobY = 95;
    int sLabelY = 95 + 114 + 5; // sous la textbox : 214

    ModernSlider* facadeSliders[5] = {
        &cleanSlider, &roomSlider, &reverbSlider, &deEssSlider, &airSlider
    };
    for (int i = 0; i < 5; ++i)
    {
        facadeSliders[i]->slider.setBounds(modX[i] + 21, sKnobY, 94, 114);
        facadeSliders[i]->label.setBounds(modX[i], sLabelY, 136, 14);
    }

    // ── BYP et △ dans chaque module ──────────────────────────────────────────
    // BYP: coin sup-gauche, △: coin sup-droit, tous deux à y=69 (dans la carte)
    int bY = 69, bW = 26, bH = 14;
    CustomButton* bypBtns[5] = {
        &cleanBypassBtn, &roomBypassBtn, &reverbBypassBtn, &deEssBypassBtn, &airBypassBtn
    };
    CustomButton* deltaBtns[5] = {
        &deltaBtn, &roomDeltaBtn, &reverbDeltaBtn, &deEssDeltaBtn, &airDeltaBtn
    };
    for (int i = 0; i < 5; ++i)
    {
        bypBtns[i]->button.setBounds(modX[i] + 8,            bY, bW, bH);
        deltaBtns[i]->button.setBounds(modX[i] + 136 - 8 - 16, bY, 16, bH);
    }

    // ── Master gain ───────────────────────────────────────────────────────────
    masterGainLabel.setBounds(getWidth() - 200, 12, 80, 28);
    masterGainSlider.setBounds(getWidth() - 116, 18, 92, 20);

    // ── Masquer tous les contrôles advanced ──────────────────────────────────
    if (roomVisualizer != nullptr) roomVisualizer->setVisible(false);

    auto hideSlider = [](ModernSlider& ms) {
        ms.slider.setVisible(false); ms.label.setVisible(false);
    };
    hideSlider(advRoomFreqSlider);  hideSlider(advRoomQSlider);
    hideSlider(advDeEssFreqSlider); hideSlider(advDeEssThreshSlider);
    hideSlider(advDeEssIntenSlider); hideSlider(advDeEssAtkSlider);
    hideSlider(advDeEssRelSlider);  hideSlider(advDeEssDetectSlider);
    hideSlider(advGateFloorSlider); hideSlider(advCleanRelSlider);
    hideSlider(advGateThresholdSlider); hideSlider(advGateAttackSlider);
    hideSlider(advGateHoldSlider);
    hideSlider(advAirDriveSlider);
    hideSlider(nrThreshSlider); hideSlider(nrRatioSlider);
    hideSlider(nrFloorSlider);  hideSlider(nrSmoothSlider);
    deEssModeBtn.button.setVisible(false);
    advAirFreqSlider.setVisible(false);
    airCharacterLabel.setVisible(false);

    // Cacher les nouveaux composants graphiques par défaut
    if (nrDualRail   != nullptr) nrDualRail->setVisible(false);
    if (nrBandDisplay != nullptr) nrBandDisplay->setVisible(false);
    nrLearnBtn.setVisible(false);
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
            nrBandDisplay->setBounds(34, 293, 118, 116);
        }
        // Sensitivity knob (nrThreshSlider)
        nrThreshSlider.slider.setVisible(true);
        nrThreshSlider.label.setVisible(true);
        nrThreshSlider.slider.setBounds(162, 293, 44, 55);
        nrThreshSlider.label.setBounds(158, 350, 52, 10);
        // Reduction knob (nrFloorSlider)
        nrFloorSlider.slider.setVisible(true);
        nrFloorSlider.label.setVisible(true);
        nrFloorSlider.slider.setBounds(222, 293, 44, 55);
        nrFloorSlider.label.setBounds(218, 350, 52, 10);
        // Curve slider (nrRatioSlider) — horizontal
        nrRatioSlider.slider.setVisible(true);
        nrRatioSlider.label.setVisible(true);
        nrRatioSlider.slider.setBounds(162, 367, 104, 14);
        nrRatioSlider.label.setBounds(268, 367, 38, 14);
        // Speed slider (nrSmoothSlider) — horizontal
        nrSmoothSlider.slider.setVisible(true);
        nrSmoothSlider.label.setVisible(true);
        nrSmoothSlider.slider.setBounds(162, 386, 104, 14);
        nrSmoothSlider.label.setBounds(268, 386, 38, 14);
        // Bouton LEARN — pill dans le header du panel NR (top-right)
        nrLearnBtn.setVisible(true);
        nrLearnBtn.setBounds(248, 286, 54, 13);

        // ── Gate panel: x=334, y=281, w=402, h=155 ───────────────────────────
        // Waveform RMS (gauche)  : x=342, w=186, h=122
        // AHR sliders+ghost (droite): x=534, w=192, h=122
        // AUTO button (bas, pleine largeur): x=342, w=384, h=16
        // AUTO — pill dans le header du panel Gate (top-right)
        gateAutoBtn.setVisible(true);
        gateAutoBtn.setBounds(662, 286, 54, 13);
        // Waveform + AHR s'étendent maintenant sur toute la hauteur utile
        if (gateWaveform != nullptr) {
            gateWaveform->setVisible(true);
            gateWaveform->setBounds(342, 298, 186, 130);
        }
        if (gateDualRail != nullptr)
            gateDualRail->setVisible(false);
        if (gateAHR != nullptr) {
            gateAHR->setVisible(true);
            gateAHR->setBounds(534, 298, 192, 130);
        }
    }
    else if (currentTab == AdvancedTab::Room)
    {
        // Canvas heatmap : canvX=58, canvY=309, canvW=296, canvH=112
        // (lpX=24 + 34px Y-axis zone = 58; lpY=281 + 28pad = 309)
        if (roomVisualizer != nullptr) {
            addAndMakeVisible(roomVisualizer.get());
            roomVisualizer->setVisible(true);
            roomVisualizer->setBounds(58, 309, 296, 112);
        }
        // Knobs dans le panneau droit (rpX=364, labels à rpY+26=307)
        // NOTCH FREQ : x=rpX+16=380, knob y=rpY+38=319, 50x66
        // NOTCH Q   : x=rpX+112=476, knob y=319, 50x66
        advRoomFreqSlider.slider.setVisible(true); advRoomFreqSlider.label.setVisible(true);
        advRoomQSlider.slider.setVisible(true);    advRoomQSlider.label.setVisible(true);
        advRoomFreqSlider.slider.setBounds(380, 319, 50, 66);
        advRoomFreqSlider.label.setBounds(378, 387, 54, 10);
        advRoomQSlider.slider.setBounds(476, 319, 50, 66);
        advRoomQSlider.label.setBounds(474, 387, 54, 10);
    }
    else if (currentTab == AdvancedTab::DeEss)
    {
        // Panel: x=24, y=281, w=712, h=130
        // → DeEssDetect display: x=34, y=289, w=360, h=108  (plus haut, moins large)
        // → AR envelope: x=402, y=289, w=220, h=108          (compact)
        // → Intensity slider vertical: x=628, y=289, w=96, h=108
        if (deEssDetect != nullptr) {
            deEssDetect->setVisible(true);
            deEssDetect->setBounds(34, 289, 360, 84);
        }
        // Slider Q (deEssDetectRatio) horizontal sous le display de détection
        {
            advDeEssDetectSlider.slider.setBounds(34, 376, 318, 16);
            advDeEssDetectSlider.label.setBounds(354, 376, 18, 16);
            advDeEssDetectSlider.slider.setVisible(true);
            advDeEssDetectSlider.label.setVisible(true);
        }
        if (deEssAR != nullptr) {
            deEssAR->setVisible(true);
            deEssAR->setBounds(402, 289, 218, 108);
        }
        {
            auto show = [](ModernSlider& ms) { ms.slider.setVisible(true); ms.label.setVisible(true); };
            // Slider vertical Intensity : pleine hauteur à droite du panel
            advDeEssIntenSlider.slider.setBounds(630, 293, 30, 88);
            advDeEssIntenSlider.label.setBounds(618, 383, 54, 10);
            show(advDeEssIntenSlider);
        }
    }
    else if (currentTab == AdvancedTab::Air)
    {
        // Canvas spectre : sY=302, sH=72 → bottom=374; slider: y=374+14-10=378, h=20
        advAirFreqSlider.setVisible(true);
        advAirDriveSlider.slider.setVisible(true);
        advAirDriveSlider.label.setVisible(true);
        airCharacterLabel.setVisible(false);
        // slider centré sur la même plage que le canvas (sX=38, sW=414)
        advAirFreqSlider.setBounds(38, 378, 414, 20);
        // Air Drive : colonne droite (x=464+107=571 centré dans 264px → x=571)
        advAirDriveSlider.slider.setBounds(556, 302, 50, 66);
        advAirDriveSlider.label.setBounds(556, 370, 60, 10);
    }
}

void InvisibleFixAudioProcessorEditor::timerCallback()
{
    // BYP blink toutes les 15 frames à 30 Hz = toggle à 1 Hz
    blinkCounter = (blinkCounter + 1) % 15;
    if (blinkCounter == 0)
    {
        bypassBlinkOn = !bypassBlinkOn;
        bypassBtnLAF.blinkOn = bypassBlinkOn;
    }

    // Repaint des boutons BYP actifs (clignotement rouge)
    CustomButton* byps[5] = { &cleanBypassBtn, &roomBypassBtn, &reverbBypassBtn,
                               &deEssBypassBtn, &airBypassBtn };
    for (auto* b : byps)
        if (b->button.getToggleState())
            b->button.repaint();

    // Synchronisation état bouton LEARN avec le flag DSP
    {
        bool snapActive = audioProcessor.nrSnapshotActive;
        if (snapActive != nrLearnBtnActive)
        {
            nrLearnBtnActive = snapActive;
            nrLearnBtn.setColour(juce::TextButton::buttonColourId,
                snapActive ? juce::Colour(0xFF1bb89e) : juce::Colour(0xFF0d1a1e));
            nrLearnBtn.setColour(juce::TextButton::textColourOffId,
                snapActive ? juce::Colour(0xFF060514) : juce::Colour(0xFF94a5a2));
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
            bool  gateOpen = audioProcessor.gateIsOpenForDisplay;

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

            static_cast<GateWaveformDisplay*>(gateWaveform.get())
                ->pushSample(signalDB, gateOpen, grNorm);

            // ── Détection de vallée AUTO — avec lissage pour stabilité ───────
            // Lissage exponentiel : ~5s de mémoire à 30 Hz (coeff = exp(-1/(5*30)))
            static float smoothedValley = -40.0f;
            static bool  hasValley      = false;

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
                    // Premier appel : initialiser sans historique
                    if (!hasValley) { smoothedValley = rawValley; hasValley = true; }
                    // Lissage lourd : ~5s à 30 Hz
                    smoothedValley = smoothedValley * 0.993f + rawValley * 0.007f;

                    audioProcessor.gateAutoThreshDB  = smoothedValley;
                    audioProcessor.gateAutoZoneLow   = smoothedValley - 3.0f;
                    audioProcessor.gateAutoZoneHigh  = smoothedValley + 3.0f;
                    audioProcessor.gateAutoZoneValid  = true;
                    zoneValid = true;
                }
            }
            if (!zoneValid) audioProcessor.gateAutoZoneValid = false;
            gateAutoBtn.setColour(juce::TextButton::textColourOffId,
                zoneValid ? juce::Colour(0xFF13add6) : juce::Colour(0xFF94a5a2).withAlpha(0.4f));
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
    juce::Point<int> clickPos = event.getPosition();
    AdvancedTab clickedTab = AdvancedTab::None;

    // Zones de clic : 5 modules x=[24,168,312,456,600] w=136, y=58 h=200
    if      (juce::Rectangle<int>( 24, 58, 136, 200).contains(clickPos)) clickedTab = AdvancedTab::Clean;
    else if (juce::Rectangle<int>(168, 58, 136, 200).contains(clickPos)) clickedTab = AdvancedTab::Room;
    else if (juce::Rectangle<int>(456, 58, 136, 200).contains(clickPos)) clickedTab = AdvancedTab::DeEss;
    else if (juce::Rectangle<int>(600, 58, 136, 200).contains(clickPos)) clickedTab = AdvancedTab::Air;

    if (clickedTab != AdvancedTab::None)
    {
        if (currentTab == clickedTab) currentTab = AdvancedTab::None;
        else                          currentTab = clickedTab;

        if (currentTab == AdvancedTab::None)  setSize(760, 290);
        else if (currentTab == AdvancedTab::Clean) setSize(760, 456);
        else                                  setSize(760, 430);

        resized();
        repaint();
    }
}