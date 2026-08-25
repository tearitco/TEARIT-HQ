@echo off
REM register_plus_x.bat - Register .+x extension as executable on Windows
REM Run this ONCE (may require admin privileges)

echo === Registering .+x file extension for Windows ===
echo.
echo This allows .+x binaries to be executed directly.
echo.

REM Check if running as admin
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running as administrator...
) else (
    echo Please run this script as Administrator (right-click -^> Run as Administrator)
    echo.
    echo Alternatively, continue using PowerShell to launch .+x files:
    echo   powershell -Command "^& { .\your_binary.+x }"
    pause
    exit /b 1
)

REM Associate .+x with a custom file type
assoc .+x=PlusXExecutable
ftype PlusXExecutable="%%1" %%*

echo.
echo Done! .+x files can now be executed directly.
echo.
echo Test with: .\pieces\chtpm\plugins\+x\orchestrator.+x
pause
