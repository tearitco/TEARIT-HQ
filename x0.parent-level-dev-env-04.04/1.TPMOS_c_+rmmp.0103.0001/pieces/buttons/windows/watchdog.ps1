# watchdog.ps1 - PAL Watchdog button for Windows
# Usage: .\#.buttons\windows\watchdog.ps1

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$LEGACY_DIR = Join-Path $SCRIPT_DIR "legacy"

Write-Host "=== Starting PAL Watchdog (Windows) ===" -ForegroundColor Cyan
& "$LEGACY_DIR\pal_watchdog.ps1"
