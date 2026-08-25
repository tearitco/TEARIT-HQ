# kill.ps1 - Kill button for Windows
# Usage: .\#.buttons\windows\kill.ps1

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$LEGACY_DIR = Join-Path $SCRIPT_DIR "legacy"

Write-Host "=== Killing CHTPM Processes (Windows) ===" -ForegroundColor Cyan
& "$LEGACY_DIR\kill_all.ps1"
