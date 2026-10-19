@echo off
setlocal
if not exist "%~dp0WWE 2K15 Launcher.exe" (
    echo Le lanceur WWE 2K15 est introuvable.
    pause
    exit /b 1
)
start "WWE 2K15 PC Launcher" "%~dp0WWE 2K15 Launcher.exe"
