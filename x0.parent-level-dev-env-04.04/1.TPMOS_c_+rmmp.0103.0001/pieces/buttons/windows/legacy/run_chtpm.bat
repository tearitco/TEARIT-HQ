@echo off
REM run_chtpm.bat - Launch CHTPM Pipeline via MSYS2 bash

echo === Launching TPM Pipeline (Windows Batch) ===
echo.

REM Launch via MSYS2 bash (shell script sets PATH internally)
C:\msys64\usr\bin\bash.exe -lc "cd '%CD%' && exec ./run_orchestrator.sh"