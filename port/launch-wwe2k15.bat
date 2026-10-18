@echo off
setlocal
if not exist "%~dp0wwe2k15-launcher.ps1" (
    echo Le lanceur WWE 2K15 est introuvable.
    pause
    exit /b 1
)
start "WWE 2K15 PC Launcher" powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -STA -WindowStyle Hidden -File "%~dp0wwe2k15-launcher.ps1"
