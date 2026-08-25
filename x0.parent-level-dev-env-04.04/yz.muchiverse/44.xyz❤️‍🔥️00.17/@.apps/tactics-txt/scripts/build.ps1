# build.ps1 - Windows twin of build.sh (tactics-txt)
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

# scripts/build.sh - compile everything, warning-free where possible.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: system/*.c here are
# real local copies of @.apps/my-chara-txt's own proven system/ sources.

# Resolve shared-lib canonical source (symlink-free)
_SS = Split-Path -Parent (Split-Path $SCRIPT_DIR -Parent)
$_SS = Join-Path $_SS "&.widgits/_shared-lib"
