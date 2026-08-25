# run.ps1 - Run button for Windows
# Usage: .\#.buttons\windows\run.ps1
# Note: Uses MSYS2 bash for reliable path handling

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$PROJECT_ROOT = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $SCRIPT_DIR))
$SHARED_DIR = Join-Path (Split-Path -Parent $SCRIPT_DIR) "shared"

Write-Host "=== Launching CHTPM (Windows via MSYS2) ===" -ForegroundColor Cyan
Write-Host "Working directory: $PROJECT_ROOT" -ForegroundColor Gray

# Ensure MinGW64 bin is in PATH for DLL dependencies
$MSYS_PATH = "C:\msys64\mingw64\bin"
if (Test-Path $MSYS_PATH) {
    if ($env:Path -notlike "*$MSYS_PATH*") {
        $env:Path = "$MSYS_PATH;$env:Path"
        Write-Host "Added $MSYS_PATH to session PATH" -ForegroundColor Gray
    }
}

Set-Location $PROJECT_ROOT
& C:\msys64\usr\bin\bash.exe -lc "cd '$PROJECT_ROOT' && exec '$SHARED_DIR/run_orchestrator.sh'"
