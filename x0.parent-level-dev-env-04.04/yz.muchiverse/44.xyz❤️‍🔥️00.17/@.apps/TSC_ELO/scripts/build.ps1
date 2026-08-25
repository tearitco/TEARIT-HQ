# build.ps1 - Windows twin of build.sh (TSC_ELO)
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

# scripts/build.sh - compile TSC_ELO system processes + ops, warning-free.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: system/*.c here are real
# local copies of 041.pal-chain's own proven system/ sources (same
# convention every project in this family follows - see @.apps/my-chara-txt
# scripts/build.sh and ../shared-ops-manifest.txt for the precedent).

# Resolve shared-lib canonical source (symlink-free)
_SS = Split-Path -Parent (Split-Path $SCRIPT_DIR -Parent)
$_SS = Join-Path $_SS "&.widgits/_shared-lib"
