# button.ps1 - Main launcher for CHTPM buttons (Windows)
# Usage: .\button.ps1 <action>
# Actions: compile, run, kill, debug, watchdog, help
#
# If you get execution policy errors, run:
#   powershell -ExecutionPolicy Bypass -File .\button.ps1 <action>
# Or set policy once (as Admin):
#   Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

param([string]$action = "help")

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$BUTTON_DIR = Join-Path $SCRIPT_DIR "pieces\buttons\windows"
$POWERShell_EXE = "powershell"

switch ($action.ToLower()) {
    { $_ -in "setup", "s", "install" } {
        & $POWERShell_EXE -ExecutionPolicy Bypass -File "$SCRIPT_DIR\install_deps.ps1"
    }
    { $_ -in "compile", "c", "build" } {
        & $POWERShell_EXE -ExecutionPolicy Bypass -File "$BUTTON_DIR\compile.ps1"
    }
    { $_ -in "run", "r", "start" } {
        & $POWERShell_EXE -ExecutionPolicy Bypass -File "$BUTTON_DIR\run.ps1"
    }
    { $_ -in "kill", "k", "stop" } {
        & $POWERShell_EXE -ExecutionPolicy Bypass -File "$BUTTON_DIR\kill.ps1"
    }
    { $_ -in "debug", "d", "gl" } {
        & $POWERShell_EXE -ExecutionPolicy Bypass -File "$BUTTON_DIR\debug_gl.ps1"
    }
    { $_ -in "watchdog", "w" } {
        & $POWERShell_EXE -ExecutionPolicy Bypass -File "$BUTTON_DIR\watchdog.ps1"
    }
    { $_ -in "help", "h", "-h", "--help" } {
        Write-Host "CHTPM Button System (Windows)" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Usage: .\button.ps1 <action>"
        Write-Host ""
        Write-Host "Actions:"
        Write-Host "  setup, s, install   - Install dependencies (MSYS2, GCC, etc.)"
        Write-Host "  compile, c, build   - Compile all CHTPM binaries"
        Write-Host "  run, r, start       - Run CHTPM orchestrator"
        Write-Host "  kill, k, stop       - Kill all CHTPM processes"
        Write-Host "  debug, d, gl        - Launch GL-OS desktop"
        Write-Host "  watchdog, w         - Start PAL watchdog"
        Write-Host "  help, h             - Show this help"
    }
    default {
        Write-Error "Unknown action: $action"
        Write-Host "Run '.\button.ps1 help' for usage."
        exit 1
    }
}
