@echo off
REM run_chtpm_debug.bat - Debug launch with verification

echo === CHTPM Pipeline Debug Launch ===
set SCRIPT_DIR=%CD%
echo Script directory: %SCRIPT_DIR%
echo.

REM Check orchestrator exists
if not exist "%SCRIPT_DIR%\pieces\chtpm\plugins\+x\orchestrator.+x" (
    echo ERROR: orchestrator.+x not found!
    echo Run compile_all.ps1 first.
    pause
    exit /b 1
)
echo [OK] orchestrator.+x found

REM Check child executables
echo Checking child executables...
if exist "%SCRIPT_DIR%\pieces\display\plugins\+x\renderer.+x" echo   [OK] renderer.+x
if exist "%SCRIPT_DIR%\pieces\display\plugins\+x\gl_renderer.+x" echo   [OK] gl_renderer.+x
if exist "%SCRIPT_DIR%\pieces\system\clock_daemon\plugins\+x\clock_daemon.+x" echo   [OK] clock_daemon.+x
if exist "%SCRIPT_DIR%\pieces\chtpm\plugins\+x\chtpm_parser.+x" echo   [OK] chtpm_parser.+x
if exist "%SCRIPT_DIR%\pieces\master_ledger\plugins\+x\response_handler.+x" echo   [OK] response_handler.+x
if exist "%SCRIPT_DIR%\pieces\joystick\plugins\+x\joystick_input.+x" echo   [OK] joystick_input.+x
echo.

REM Check layout file
if exist "%SCRIPT_DIR%\pieces\chtpm\layouts\os.chtpm" (
    echo [OK] os.chtpm layout found
) else (
    echo [WARN] os.chtpm not found
)
echo.

REM Kill existing processes
echo Cleaning up existing processes...
taskkill /F /IM "orchestrator.+x" 2>nul
taskkill /F /IM "chtpm_parser.+x" 2>nul
taskkill /F /IM "renderer.+x" 2>nul
taskkill /F /IM "gl_renderer.+x" 2>nul
taskkill /F /IM "clock_daemon.+x" 2>nul
taskkill /F /IM "gl_desktop.+x" 2>nul
echo.

echo Opening CHTPM CLI in new terminal window...
echo ========================================

REM Open new window running bash interactively
start "CHTPM+OS CLI" C:\msys64\usr\bin\bash.exe -l -c "cd '%SCRIPT_DIR%' && ./run_orchestrator.sh"

echo.
echo CHTPM CLI is running in a new window.
