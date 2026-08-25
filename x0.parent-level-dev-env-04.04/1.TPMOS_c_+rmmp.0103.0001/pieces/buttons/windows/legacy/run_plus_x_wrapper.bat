@echo off
REM run_plus_x_wrapper.bat - Execute .+x binaries via cmd.exe
REM This tells Windows to treat .+x files as executables via association

REM Register .+x extension to run as binaries (requires admin once)
REM For now, we just execute via the full path with explicit shell

if "%~1"=="" (
    echo Usage: run_plus_x_wrapper.bat ^<path\to\binary.+x^> [args...]
    exit /b 1
)

REM Execute the binary directly - cmd.exe will handle it
"%~1" %*
