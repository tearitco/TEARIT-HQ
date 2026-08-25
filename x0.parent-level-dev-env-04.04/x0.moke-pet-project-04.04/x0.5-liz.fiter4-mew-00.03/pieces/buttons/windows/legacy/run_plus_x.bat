@echo off
REM run_plus_x.bat - Helper to execute .+x binaries on Windows
REM Usage: run_plus_x.bat path\to\binary.+x [args...]

if "%~1"=="" (
    echo Usage: run_plus_x.bat ^<binary.+x^> [args...]
    exit /b 1
)

REM Get the full path
set "BINARY=%~1"
shift

REM Build argument list
set "ARGS="
:loop
if "%~1"=="" goto after_loop
set "ARGS=%ARGS% %~1"
shift
goto loop
:after_loop

REM Execute via PowerShell
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { & '%BINARY%' %ARGS% }"
