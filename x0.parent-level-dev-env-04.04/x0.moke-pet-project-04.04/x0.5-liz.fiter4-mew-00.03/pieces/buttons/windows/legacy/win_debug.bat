@echo off
REM win_debug.bat - Launch GL-OS Desktop in MSYS2 terminal
REM This runs gl_desktop.+x with proper terminal support

echo === GL-OS Desktop Debug Launch ===
echo.

REM Open mintty (MSYS2 terminal) to run GL-OS desktop
start "GL-OS Desktop" C:\msys64\usr\bin\mintty.exe -i /Cygwin-Terminal.ico -t "GL-OS Desktop" --size 80,24 bash -lc "cd '%CD%' && exec ./run_gl_desktop.sh"

echo.
echo GL-OS Desktop is running in a new window.
echo Close that window to exit.
