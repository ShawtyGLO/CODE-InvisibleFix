// =============================================================================
//  UILayout.h — GÉNÉRÉ AUTOMATIQUEMENT depuis tools/layout.json
//  NE PAS ÉDITER À LA MAIN : toute modification sera écrasée à la régénération.
//  Pour déplacer/renommer un panneau peint : éditer tools/layout.json
//  (ou Shawty's Plugin Factory), puis régénérer ce fichier.
//
//  Portée actuelle (lot "panneaux + titres") : les cadres peints par
//  drawAdvPanel() dans PluginEditor.cpp::paint(). Le reste du layout
//  (setBounds dans resized()) sera migré ici par la suite.
// =============================================================================
#pragma once

namespace UI
{
    struct Panel { float x, y, w, h; const char* title; };

    namespace panel
    {
        // Onglet CLEAN
        constexpr Panel nr    {  24.0f, 281.0f, 296.0f, 155.0f, "NOISE REDUCTION" };
        constexpr Panel gate  { 334.0f, 281.0f, 402.0f, 155.0f, "GATE" };
        // Onglet DE-ESS
        constexpr Panel deEss {  24.0f, 281.0f, 712.0f, 130.0f, "DE-ESSER ADVANCED" };
        // Onglet ROOM (deux panneaux)
        constexpr Panel roomAnalysis {  24.0f, 281.0f, 330.0f, 152.0f, "NOTCH ANALYSIS" };
        constexpr Panel roomParams   { 364.0f, 281.0f, 372.0f, 152.0f, "NOTCH PARAMETERS" };
        // Onglet DE-REV
        constexpr Panel deRev {  24.0f, 281.0f, 712.0f, 152.0f, "DE-REV ADVANCED" };
        // Onglet AIR
        constexpr Panel air   {  24.0f, 281.0f, 712.0f, 152.0f, "AIR CHARACTER" };
    }
}
