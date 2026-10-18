@echo off
setlocal

if not exist "%~dp0wwe2k15.exe" (
    echo WWE 2K15 executable not found beside this launcher.
    pause
    exit /b 1
)

if not exist "%~dp0assets\default.xex" if not exist "%~dp0game\default.xex" if not exist "%USERPROFILE%\Desktop\WWE2K15\default.xex" (
    echo Game files not found.
    echo Put your own dumped game folder in "assets" beside this launcher,
    echo or at "%USERPROFILE%\Desktop\WWE2K15".
    pause
    exit /b 1
)

"%~dp0wwe2k15.exe" --gpu_plugin=xenos --user_data_root="%~dp0user_data"
if errorlevel 1 pause
