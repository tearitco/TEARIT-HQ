# debug_gl.ps1 - GL-OS Debug button for Windows
# Usage: .\#.buttons\windows\debug_gl.ps1

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$LEGACY_DIR = Join-Path $SCRIPT_DIR "legacy"

Write-Host "=== Launching GL-OS Desktop (Windows) ===" -ForegroundColor Cyan
& "$LEGACY_DIR\win_debug.bat"
