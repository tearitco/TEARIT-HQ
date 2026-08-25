@echo off
REM kill_all.bat - Kill all TPM processes (wrapper for kill_all.ps1)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0kill_all.ps1"
