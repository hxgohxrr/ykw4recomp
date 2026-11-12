@echo off
setlocal
set "RUNTIME=%~dp0YKW4.exe"
if not exist "%RUNTIME%" set "RUNTIME=%~dp0YKW4Recomp.exe"
if not exist "%RUNTIME%" (
    echo Coloca este archivo junto al ejecutable de tu instalacion.
    pause
    exit /b 1
)
pushd "%~dp0"
set "SNX_FRAME_PROFILE=1"
"%RUNTIME%"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
