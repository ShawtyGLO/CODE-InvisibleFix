// =============================================================================
//  Theme.h — point d'entrée du thème (couleurs + polices) d'InvisibleFix
//  Écrit à la main (stable). Les valeurs de couleurs vivent dans le header
//  généré Generated/ThemeColors.g.h (source : tools/theme.json).
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include "Generated/ThemeColors.g.h"

namespace Theme
{
    // Helper de police — centralise la construction des Font.
    // À terme : rôles nommés (label, value, tiny…) et famille unique ici.
    inline juce::Font font(float size, const juce::String& style = {})
    {
        return juce::Font(juce::FontOptions(size).withStyle(style));
    }
}
