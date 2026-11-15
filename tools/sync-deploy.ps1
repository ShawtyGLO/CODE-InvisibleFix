# =============================================================================
#  InvisibleFix - Synchronisation complete : Plugin Factory -> plugin -> FL Studio
#  Lance par "SYNC ET DEPLOIE.cmd" (double-clic). Ne pas executer a la main.
#
#  Etapes :
#    0. refuse de tourner si FL Studio est ouvert (le VST3 charge est verrouille)
#    1. recupere layout.json depuis le dossier Telechargements s'il est plus recent
#    2. regenere UILayout.h + plugin-factory.html
#    3. compile le code partage puis le VST3 (Release x64)
#    4. deploie vers Common Files\VST3
# =============================================================================
$ErrorActionPreference = 'Stop'

$tools   = $PSScriptRoot
$root    = Split-Path $tools -Parent
$layout  = Join-Path $tools 'layout.json'
$backup  = Join-Path $tools 'layout.backup.json'
$vst3Dst = 'C:\Program Files\Common Files\VST3\InvisibleFix.vst3'
$vst3Src = Join-Path $root 'Builds\VisualStudio2022\x64\Release\VST3\InvisibleFix.dll'

function Titre($t) { Write-Host ""; Write-Host $t -ForegroundColor Cyan }
function Ok($t)    { Write-Host "   $t" -ForegroundColor Green }
function Info($t)  { Write-Host "   $t" -ForegroundColor Gray }
function Souci($t) { Write-Host "   $t" -ForegroundColor Yellow }

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  InvisibleFix : Plugin Factory  -->  plugin  -->  FL Studio" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# --- 0. FL Studio doit etre ferme ------------------------------------------
if (Get-Process -Name 'FL64','FL' -ErrorAction SilentlyContinue) {
    Write-Host ""
    Write-Host "  ARRET : FL Studio est ouvert." -ForegroundColor Red
    Write-Host "  Windows verrouille le VST3 tant qu'il est charge." -ForegroundColor Red
    Write-Host "  Ferme FL Studio, puis relance ce fichier." -ForegroundColor Red
    exit 1
}

# --- 1. Import automatique depuis Telechargements ---------------------------
Titre "[1/4] Recuperation de tes reglages Plugin Factory"
$dl = Join-Path $env:USERPROFILE 'Downloads\layout.json'
if (Test-Path $dl) {
    $dlTime  = (Get-Item $dl).LastWriteTime
    $curTime = if (Test-Path $layout) { (Get-Item $layout).LastWriteTime } else { [DateTime]::MinValue }
    if ($dlTime -gt $curTime) {
        try { $null = Get-Content $dl -Raw -Encoding utf8 | ConvertFrom-Json }
        catch { Write-Host "   Le layout.json telecharge est illisible, import annule." -ForegroundColor Red; exit 1 }
        if (Test-Path $layout) { Copy-Item $layout $backup -Force; Info "sauvegarde de l'ancien -> layout.backup.json" }
        Copy-Item $dl $layout -Force
        Ok "export du $($dlTime.ToString('dd/MM HH:mm')) importe depuis Telechargements"
    } else {
        Info "aucun nouvel export (celui de tools\ est deja le plus recent)"
    }
} else {
    Info "rien dans Telechargements : on utilise tools\layout.json tel quel"
}

# --- 2. Regeneration --------------------------------------------------------
Titre "[2/4] Regeneration des fichiers generes"
# $LASTEXITCODE ne concerne que les executables natifs : pour un script PowerShell,
# c'est $ErrorActionPreference='Stop' qui interrompt en cas de probleme.
& (Join-Path $tools 'generate.ps1')

# --- 3. Compilation ---------------------------------------------------------
$msb = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path $msb)) {
    $cand = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter MSBuild.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -notmatch 'amd64' } | Select-Object -First 1
    if ($cand) { $msb = $cand.FullName } else { throw "MSBuild introuvable" }
}

Titre "[3/4] Compilation (Release x64)"
foreach ($proj in @('InvisibleFix_SharedCode.vcxproj','InvisibleFix_VST3.vcxproj')) {
    Info "compilation de $proj ..."
    $p = Join-Path $root "Builds\VisualStudio2022\$proj"
    $out = & $msb $p -p:Configuration=Release -p:Platform=x64 -m -v:minimal -nologo 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "  ECHEC DE COMPILATION - rien n'a ete deploye :" -ForegroundColor Red
        $out | Select-String -Pattern ' error ' | Select-Object -First 12 | ForEach-Object { Write-Host "   $_" -ForegroundColor Red }
        exit 1
    }
}
Ok "compilation reussie"

# --- 4. Deploiement ---------------------------------------------------------
Titre "[4/4] Deploiement vers Common Files\VST3"
try { Copy-Item $vst3Src $vst3Dst -Force }
catch {
    Write-Host ""
    Write-Host "  Copie refusee. Clic droit sur SYNC ET DEPLOIE.cmd" -ForegroundColor Red
    Write-Host "  puis 'Executer en tant qu'administrateur'." -ForegroundColor Red
    exit 1
}
$h1 = (Get-FileHash $vst3Src -Algorithm SHA256).Hash
$h2 = (Get-FileHash $vst3Dst -Algorithm SHA256).Hash
if ($h1 -ne $h2) { Write-Host "  Le fichier deploye ne correspond pas au build." -ForegroundColor Red; exit 1 }
Ok "deploye et verifie ($($h1.Substring(0,16)))"

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "  TERMINE - relance FL Studio pour charger la nouvelle version" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
exit 0
