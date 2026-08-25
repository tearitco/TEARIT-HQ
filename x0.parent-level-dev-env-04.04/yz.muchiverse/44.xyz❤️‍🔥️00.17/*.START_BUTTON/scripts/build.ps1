# build.ps1 - Windows twin of build.sh (*.START_BUTTON)
# ASCII only.

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location -LiteralPath $SCRIPT_DIR

$MSYS = "C:\msys64\mingw64\bin"
$MSYS_LIB = "C:\msys64\mingw64\lib"
if (Test-Path $MSYS) {
    if ($env:Path -notlike "*$MSYS*") { $env:Path = "$MSYS;$env:Path" }
}

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    Write-Error "gcc not found. Install MSYS2 MinGW64 (mingw-w64-x86_64-gcc, freeglut)."
    exit 1
}

New-Item -ItemType Directory -Force -Path "ops/+x system" | Out-Null
$CFLAGS = @("-Wall", "-Wextra", "-O2")
Write-Host "--- system ---"
& gcc @CFLAGS "system/prisc+x.c" -o "system/prisc+x"
& gcc @CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
& gcc @CFLAGS "system/renderer.c" -o "system/renderer"
& gcc @CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"
Write-Host "--- ops ---"
& gcc @CFLAGS -o "ops/+x/start_scan.+x" "ops/start_scan.c"
& gcc @CFLAGS -o "ops/+x/start_compose_frame.+x" "ops/start_compose_frame.c"
& gcc @CFLAGS -o "ops/+x/start_menu_input.+x" "ops/start_menu_input.c"
Write-Host "build ok"
