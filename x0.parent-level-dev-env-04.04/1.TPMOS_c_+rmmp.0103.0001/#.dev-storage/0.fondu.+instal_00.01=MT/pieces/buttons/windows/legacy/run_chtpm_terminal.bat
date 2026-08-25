@echo off
REM run_chtpm_terminal.bat - Run CHTPM in MSYS2 bash terminal

cd /d "%~dp0"

REM Kill existing processes
taskkill /F /IM "orchestrator.+x" 2>nul
taskkill /F /IM "chtpm_parser.+x" 2>nul
taskkill /F /IM "renderer.+x" 2>nul
taskkill /F /IM "clock_daemon.+x" 2>nul

REM Run orchestrator in bash
C:\msys64\msys2_shell.cmd -defterm -here -no-start -c "./run_orchestrator.sh"
