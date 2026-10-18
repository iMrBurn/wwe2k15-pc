@echo off
setlocal

if not exist "%~dp0wwe2k15.exe" (
    echo WWE 2K15 executable not found beside this launcher.
    pause
    exit /b 1
)

pushd "%~dp0"
"%~dp0wwe2k15.exe" --gpu_plugin=xenos --input_backend=sdl --user_data_root="%~dp0user_data"
set "RESULT=%ERRORLEVEL%"
popd
if not "%RESULT%"=="0" pause
exit /b %RESULT%
