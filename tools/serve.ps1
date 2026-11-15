# =============================================================================
#  InvisibleFix - Assistant local de Plugin Factory
#  Lance par "PLUGIN FACTORY.cmd" (double-clic).
#
#  Une page ouverte en file:// ne peut ni ecrire de fichier ni lancer un
#  compilateur (bac a sable du navigateur). Ce petit serveur local comble ce
#  manque : il sert Plugin Factory en http://localhost et expose deux routes que
#  la page appelle en fetch().
#
#    GET  /                -> plugin-factory.html
#    GET  /layout.json     -> tools/layout.json (lecture live)
#    GET  /api/status      -> FL Studio est-il ouvert ?
#    POST /api/sync        -> ecrit layout.json, regenere, compile, deploie
#
#  Aucune elevation necessaire : le prefixe "localhost" est autorise par defaut.
#  Rien n'est expose vers l'exterieur, l'ecoute est strictement sur la boucle locale.
# =============================================================================
$ErrorActionPreference = 'Stop'

$tools  = $PSScriptRoot
$port   = 8765
$prefix = "http://localhost:$port/"

$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add($prefix)
try { $listener.Start() }
catch {
    Write-Host "Impossible d'ouvrir le port $port : $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Un autre Plugin Factory est peut-etre deja ouvert." -ForegroundColor Yellow
    Read-Host "Entree pour fermer"; exit 1
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Plugin Factory - environnement complet" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Ouvert sur  $prefix" -ForegroundColor Green
Write-Host ""
Write-Host "  Laisse cette fenetre ouverte tant que tu travailles." -ForegroundColor Gray
Write-Host "  Ferme-la (ou Ctrl+C) quand tu as fini." -ForegroundColor Gray
Write-Host ""

# PF_NO_BROWSER=1 permet de demarrer l'assistant sans ouvrir de navigateur (tests)
if (-not $env:PF_NO_BROWSER) { Start-Process $prefix | Out-Null }

function Send-Text($ctx, [string]$body, [string]$type = 'text/plain; charset=utf-8', [int]$code = 200) {
    $bytes = [Text.Encoding]::UTF8.GetBytes($body)
    $ctx.Response.StatusCode  = $code
    $ctx.Response.ContentType = $type
    $ctx.Response.ContentLength64 = $bytes.Length
    $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
    $ctx.Response.OutputStream.Close()
}

while ($listener.IsListening) {
    try { $ctx = $listener.GetContext() } catch { break }
    $path   = $ctx.Request.Url.AbsolutePath
    $method = $ctx.Request.HttpMethod

    try {
        if ($method -eq 'GET' -and ($path -eq '/' -or $path -eq '/plugin-factory.html')) {
            $html = [IO.File]::ReadAllText((Join-Path $tools 'plugin-factory.html'), [Text.Encoding]::UTF8)
            Send-Text $ctx $html 'text/html; charset=utf-8'
        }
        elseif ($method -eq 'GET' -and $path -eq '/layout.json') {
            $json = [IO.File]::ReadAllText((Join-Path $tools 'layout.json'), [Text.Encoding]::UTF8)
            Send-Text $ctx $json 'application/json; charset=utf-8'
        }
        elseif ($method -eq 'GET' -and $path -eq '/api/status') {
            $fl = [bool](Get-Process -Name 'FL64','FL' -ErrorAction SilentlyContinue)
            Send-Text $ctx (@{ flStudioOuvert = $fl } | ConvertTo-Json -Compress) 'application/json; charset=utf-8'
        }
        elseif ($method -eq 'POST' -and $path -eq '/api/sync') {
            $reader = New-Object IO.StreamReader($ctx.Request.InputStream, [Text.Encoding]::UTF8)
            $body   = $reader.ReadToEnd(); $reader.Close()

            # Le corps est le layout complet envoye par la page
            $valide = $true
            try { $null = $body | ConvertFrom-Json } catch { $valide = $false }
            if (-not $valide) { Send-Text $ctx "JSON invalide, rien n'a ete ecrit." 'text/plain; charset=utf-8' 400; continue }

            $layout = Join-Path $tools 'layout.json'
            if (Test-Path $layout) { Copy-Item $layout (Join-Path $tools 'layout.backup.json') -Force }
            $enc = New-Object Text.UTF8Encoding($false)
            [IO.File]::WriteAllText($layout, $body, $enc)

            Write-Host "  -> demande de synchronisation recue, compilation..." -ForegroundColor Cyan
            $log = & (Join-Path $tools 'sync-deploy.ps1') 2>&1 | Out-String
            $code = if ($LASTEXITCODE -eq 0 -or $null -eq $LASTEXITCODE) { 200 } else { 500 }
            if ($code -eq 200) { Write-Host "  -> termine" -ForegroundColor Green }
            else               { Write-Host "  -> echec (voir la page)" -ForegroundColor Red }
            Send-Text $ctx $log 'text/plain; charset=utf-8' $code
        }
        else { Send-Text $ctx 'Not found' 'text/plain; charset=utf-8' 404 }
    }
    catch {
        try { Send-Text $ctx "Erreur serveur : $($_.Exception.Message)" 'text/plain; charset=utf-8' 500 } catch {}
    }
}

$listener.Stop(); $listener.Close()
