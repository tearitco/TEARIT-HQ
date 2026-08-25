# build.ps1 - Windows twin of build_self.sh (*.hard-vvar-agent-Q0000)
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

# build_self.sh - v1 build-itself op. Appends a self-improvement request to
# a log the monad (or a human) can act on. Sandboxed to the monad only.
# Usage: build_self.sh "<what>"
# SOURCED: $(cd "(Split-Path -Parent "$0")/../brain" && pwd)/oplib.sh

WHAT="$*"
[ -z "$WHAT" ] && WHAT="(unspecified improvement)"

SELF_LOG="$BRAIN_DIR/self_improvements.txt"
printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$WHAT" >> "$SELF_LOG"
ledger_append "BuildSelf" "wants to improve itself: $WHAT" "build_self.sh"
Write-Host "BuildSelf: logged '$WHAT' to self_improvements.txt"
