# =============================================================================
#  generate.ps1 - Regenere les headers C++ depuis les sources de verite JSON.
#    tools/theme.json  -> Source/Generated/ThemeColors.g.h   (namespace Theme::)
#    tools/layout.json -> Source/Generated/UILayout.h        (namespace UI::panel::)
#
#  Usage :  powershell -ExecutionPolicy Bypass -File tools\generate.ps1
#  Apres generation : rebuild dans VS2022 pour appliquer au plugin.
# =============================================================================
$ErrorActionPreference = 'Stop'
$tools = $PSScriptRoot
$root  = Split-Path $tools -Parent
$gen   = Join-Path $root 'Source\Generated'
if (-not (Test-Path $gen)) { New-Item -ItemType Directory -Force $gen | Out-Null }
$nl = "`r`n"

function Read-JsonUtf8([string]$path) {
    $txt = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    return ($txt | ConvertFrom-Json)
}
function Write-NoBom([string]$path, [string]$content) {
    $enc = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($path, $content, $enc)
}
# Formatage de float en litteral C++ : TOUJOURS via InvariantCulture, sinon la locale FR
# produit une virgule ("7,5f") et le C++ ne compile plus.
function Fmt([double]$v) {
    return $v.ToString('0.0###', [System.Globalization.CultureInfo]::InvariantCulture)
}
# Justification -> entier stable, mappe cote C++ (0 = gauche, 1 = centre, 2 = droite)
function AlignCode([string]$a) {
    switch ($a) { 'left' { return 0 } 'right' { return 2 } default { return 1 } }
}

# ---------------------------------------------------------------- Theme colors
$theme = Read-JsonUtf8 (Join-Path $tools 'theme.json')
$t = New-Object System.Text.StringBuilder
[void]$t.Append("// ============================================================================$nl")
[void]$t.Append("//  ThemeColors.g.h - GENERE depuis tools/theme.json par tools/generate.ps1$nl")
[void]$t.Append("//  NE PAS EDITER A LA MAIN.$nl")
[void]$t.Append("// ============================================================================$nl")
[void]$t.Append("#pragma once$nl$nl#include <JuceHeader.h>$nl$nl")
[void]$t.Append("namespace Theme$nl{$nl")
foreach ($p in $theme.colors.PSObject.Properties) {
    $name = $p.Name; $hex = $p.Value.hex; $note = $p.Value.note
    $line = "    const juce::Colour " + $name.PadRight(14) + " = juce::Colour(0x" + $hex + "); // " + $note
    [void]$t.Append($line + $nl)
}
[void]$t.Append("}$nl")
Write-NoBom (Join-Path $gen 'ThemeColors.g.h') $t.ToString()
$nbColors = @($theme.colors.PSObject.Properties).Count

# ---------------------------------------------------------------- UI panels
$layout = Read-JsonUtf8 (Join-Path $tools 'layout.json')
$u = New-Object System.Text.StringBuilder
[void]$u.Append("// ============================================================================$nl")
[void]$u.Append("//  UILayout.h - GENERE depuis tools/layout.json par tools/generate.ps1$nl")
[void]$u.Append("//  NE PAS EDITER A LA MAIN. Portee : panneaux peints (drawAdvPanel).$nl")
[void]$u.Append("// ============================================================================$nl")
[void]$u.Append("#pragma once$nl$nl")
[void]$u.Append("namespace UI$nl{$nl")
[void]$u.Append("    struct Panel      { float x, y, w, h; const char* title; };$nl")
[void]$u.Append("    struct Rect       { float x, y, w, h; };$nl")
[void]$u.Append("    struct AnchorBR   { float right, bottom, w, h; }; // ancre bas-droite : x=getWidth()-right, y=getHeight()-bottom$nl")
[void]$u.Append("    struct AnchorBottomBar { float bottom, h, w; };   // barre ancree en bas a gauche : y=getHeight()-bottom, largeur w (coords de base)$nl")
[void]$u.Append("    // Texte peint par paint() : libelle statique pilote par les donnees.$nl")
[void]$u.Append("    // align : 0 = gauche, 1 = centre, 2 = droite. La couleur reste choisie dans le C++ (Theme::),$nl")
[void]$u.Append("    // seul son alpha vient des donnees.$nl")
[void]$u.Append("    struct PText      { float x, y, w, h; const char* text; float size; bool bold; int align; float alpha; };$nl$nl")
[void]$u.Append("    namespace panel$nl    {$nl")
$nbPanels = 0
foreach ($sk in $layout.states.PSObject.Properties) {
    $panels = @($sk.Value.zones | Where-Object { $_.kind -eq 'panel' })
    if ($panels.Count -eq 0) { continue }
    [void]$u.Append("        // Onglet " + $sk.Name + $nl)
    foreach ($z in $panels) {
        $cname = ($z.id -replace 'Panel$', '')
        $title = ($z.label -replace '"', '\"')
        $nx = ([int]$z.x).ToString().PadLeft(4)
        $ny = ([int]$z.y).ToString().PadLeft(4)
        $nw = ([int]$z.w).ToString().PadLeft(4)
        $nh = ([int]$z.h).ToString().PadLeft(4)
        $line = "        constexpr Panel " + $cname.PadRight(13) + " { " + $nx + ".0f, " + $ny + ".0f, " + $nw + ".0f, " + $nh + '.0f, "' + $title + '" };'
        [void]$u.Append($line + $nl)
        $nbPanels++
    }
}
[void]$u.Append("    }$nl$nl")

# --- Rectangles des controles (bounds de resized()), un namespace par fenetre ---
[void]$u.Append("    namespace ctrl$nl    {$nl")
$nbCtrl = 0
foreach ($sk in $layout.states.PSObject.Properties) {
    # Les textes peints ne sont pas des bounds de composants : ils ont leur propre namespace plus bas.
    $ctrls = @($sk.Value.controls | Where-Object { $_.group -ne 'ptext' })
    if ($ctrls.Count -eq 0) { continue }
    [void]$u.Append("        namespace " + $sk.Name + $nl + "        {$nl")
    foreach ($c in $ctrls) {
        $nx = ([int]$c.x).ToString().PadLeft(4)
        $ny = ([int]$c.y).ToString().PadLeft(4)
        $nw = ([int]$c.w).ToString().PadLeft(4)
        $nh = ([int]$c.h).ToString().PadLeft(4)
        [void]$u.Append("            constexpr Rect " + $c.id.PadRight(22) + " { " + $nx + ".0f, " + $ny + ".0f, " + $nw + ".0f, " + $nh + ".0f };" + $nl)
        $nbCtrl++
    }
    [void]$u.Append("        }$nl")
}
[void]$u.Append("    }$nl$nl")

# --- Textes peints par paint() (group "ptext"), un namespace par fenetre ---
[void]$u.Append("    namespace ptext$nl    {$nl")
$nbPText = 0
foreach ($sk in $layout.states.PSObject.Properties) {
    $pts = @($sk.Value.controls | Where-Object { $_.group -eq 'ptext' })
    if ($pts.Count -eq 0) { continue }
    [void]$u.Append("        namespace " + $sk.Name + $nl + "        {$nl")
    foreach ($c in $pts) {
        $nx = ([int]$c.x).ToString().PadLeft(4)
        $ny = ([int]$c.y).ToString().PadLeft(4)
        $nw = ([int]$c.w).ToString().PadLeft(4)
        $nh = ([int]$c.h).ToString().PadLeft(4)
        $txt   = ($c.text -replace '\\', '\\' -replace '"', '\"')
        $size  = Fmt([double]$c.size)
        $alpha = if ($null -eq $c.alpha) { '1.0' } else { Fmt([double]$c.alpha) }
        $bold  = if ($c.bold) { 'true ' } else { 'false' }
        $al    = AlignCode([string]$c.align)
        [void]$u.Append("            constexpr PText " + $c.id.PadRight(22) + " { " + $nx + ".0f, " + $ny + ".0f, " + $nw + ".0f, " + $nh + '.0f, "' + $txt + '", ' + $size + "f, " + $bold + ", " + $al + ", " + $alpha + "f };" + $nl)
        $nbPText++
    }
    [void]$u.Append("        }$nl")
}
[void]$u.Append("    }$nl$nl")

# --- Éléments ancrés (toujours visibles, hors zones/onglets) : tools/layout.json → clé "global" ---
# --- Hauteur de fenetre par etat (pilote setSize() dans mouseUp) ---
[void]$u.Append("    namespace stateHeight$nl    {$nl")
foreach ($sk in $layout.states.PSObject.Properties) {
    $hh = ([int]$sk.Value.height).ToString().PadLeft(4)
    [void]$u.Append("        constexpr float " + $sk.Name.PadRight(10) + " = " + $hh + ".0f;" + $nl)
}
[void]$u.Append("    }$nl$nl")

$nbAnchor = 0
$globalCtrls = @()
if ($layout.global -and $layout.global.controls) { $globalCtrls = @($layout.global.controls) }
$brCtrls  = @($globalCtrls | Where-Object { $_.anchor -eq 'br' })
$bbCtrls  = @($globalCtrls | Where-Object { $_.anchor -eq 'bottombar' })
[void]$u.Append("    namespace anchor$nl    {$nl")
if ($brCtrls.Count -gt 0) {
    [void]$u.Append("        namespace br$nl        {$nl")
    foreach ($c in $brCtrls) {
        $nr = ([int]$c.right).ToString().PadLeft(4); $nb = ([int]$c.bottom).ToString().PadLeft(4)
        $nw = ([int]$c.w).ToString().PadLeft(4);      $nh = ([int]$c.h).ToString().PadLeft(4)
        [void]$u.Append("            constexpr AnchorBR " + $c.id.PadRight(16) + " { " + $nr + ".0f, " + $nb + ".0f, " + $nw + ".0f, " + $nh + ".0f };" + $nl)
        $nbAnchor++
    }
    [void]$u.Append("        }$nl")
}
if ($bbCtrls.Count -gt 0) {
    [void]$u.Append("        namespace bottomBar$nl        {$nl")
    foreach ($c in $bbCtrls) {
        $nb = ([int]$c.bottom).ToString().PadLeft(4); $nh = ([int]$c.h).ToString().PadLeft(4)
        $nw = ([int]$c.w).ToString().PadLeft(4)
        [void]$u.Append("            constexpr AnchorBottomBar " + $c.id.PadRight(16) + " { " + $nb + ".0f, " + $nh + ".0f, " + $nw + ".0f };" + $nl)
        $nbAnchor++
    }
    [void]$u.Append("        }$nl")
}
[void]$u.Append("    }$nl}$nl")
Write-NoBom (Join-Path $gen 'UILayout.h') $u.ToString()

# ------------------------------------------------- Injection dans Plugin Factory
#  tools/layout.json est recopie dans plugin-factory.html entre les marqueurs, avec
#  une empreinte. L'outil demarre donc TOUJOURS sur le fichier a jour, et son autosave
#  navigateur est automatiquement invalide des que l'empreinte change.
$pfPath = Join-Path $tools 'plugin-factory.html'
$nbInject = 0
if (Test-Path $pfPath) {
    $layoutRaw = [System.IO.File]::ReadAllText((Join-Path $tools 'layout.json'), [System.Text.Encoding]::UTF8)
    $sha   = [System.Security.Cryptography.SHA256]::Create()
    $stamp = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($layoutRaw))) -replace '-','').Substring(0,12).ToLower()
    $sha.Dispose()

    # JSON compact (une seule ligne) pour ne pas gonfler le HTML
    $compact = ($layoutRaw | ConvertFrom-Json) | ConvertTo-Json -Depth 30 -Compress

    $block = "/* @@LAYOUT_BEGIN@@ - bloc regenere par tools/generate.ps1, ne pas editer a la main */$nl" +
             "const LAYOUT_JSON = $compact;$nl" +
             "const SEED_STAMP  = '$stamp';$nl" +
             "/* @@LAYOUT_END@@ */"

    $html = [System.IO.File]::ReadAllText($pfPath, [System.Text.Encoding]::UTF8)
    $rx   = [regex]'(?s)/\* @@LAYOUT_BEGIN@@.*?@@LAYOUT_END@@ \*/'
    if ($rx.IsMatch($html)) {
        # MatchEvaluator : remplacement litteral, aucun caractere du JSON n'est interprete
        $evaluator = [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $block }
        Write-NoBom $pfPath ($rx.Replace($html, $evaluator, 1))
        $nbInject = 1
    } else {
        Write-Warning "Marqueurs @@LAYOUT_BEGIN@@/@@LAYOUT_END@@ introuvables dans plugin-factory.html : injection ignoree."
    }
}

$pfMsg = if ($nbInject -eq 1) { ", plugin-factory.html synchronise (empreinte $stamp)" } else { "" }
"OK - ThemeColors.g.h ($nbColors couleurs), UILayout.h ($nbPanels panneaux, $nbCtrl controles, $nbPText textes peints, $nbAnchor ancres) regeneres$pfMsg."
