# compile.ps1 - Compile button for Windows
# Usage: .\#.buttons\windows\compile.ps1

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$LEGACY_DIR = Join-Path $SCRIPT_DIR "legacy"

Write-Host "=== Compiling CHTPM (Windows) ===" -ForegroundColor Cyan
& "$LEGACY_DIR\compile_all.ps1"
