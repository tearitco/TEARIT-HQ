# build.ps1 - Windows twin of build_events_hq_manager.sh (events-hq)
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

# build_events_hq_manager.sh — build events-hq's MANAGER binary
# (khtpm_events_hq_manager.c), Stage 2d shell/manager split, same real
# mechanism proven on db-hq (see khtpm_hq_manager.c's own build script).
# No X11/Xft dependency - this binary never opens a window.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-O2")

Write-Host "-- events-hq manager -> +x/khtpm_events_hq_manager.+x"
& $CC @CFLAGS -o +x/khtpm_events_hq_manager.+x khtpm_events_hq_manager.c

Write-Host "OK +x/khtpm_events_hq_manager.+x"
