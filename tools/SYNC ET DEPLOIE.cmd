@echo off
title InvisibleFix - Synchronisation et deploiement
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0sync-deploy.ps1"
echo.
pause
