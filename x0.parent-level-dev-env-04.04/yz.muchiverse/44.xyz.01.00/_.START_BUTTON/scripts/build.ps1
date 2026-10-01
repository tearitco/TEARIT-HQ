# build.ps1 - Windows twin of build.sh (_.START_BUTTON)
# ASCII only.
#
# FIXED 2026-09-26. This file previously existed but could not succeed:
#   1. compiled system/prisc+x.c, which does not exist in this tree. The
#      canonical source moved to &.widgits/_shared-lib (see
#      PRISC-X-FORK-CONSOLIDATION.md); build.sh walks up to find it.
#   2. "New-Item ... 'ops/+x system'" did not create ops/+x, so the ops
#      link step failed with "cannot open output file".
#   3. No error checking at all: it printed "build ok" and returned 0
#      while every single compile had failed. build.sh has `set -e`.
# All three are fixed below; $LASTEXITCODE is now authoritative.

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

# Canonical shared lib: same upward walk as build.sh line 2.
$pcd = $SCRIPT_DIR
$SHARED = $null
while ($pcd -and $pcd -ne [System.IO.Path]::GetPathRoot($pcd)) {
    if (Test-Path -LiteralPath (Join-Path $pcd "&.widgits\_shared-lib")) {
        $SHARED = Join-Path $pcd "&.widgits\_shared-lib"
        break
    }
    $parent = Split-Path -Parent $pcd
    if ($parent -eq $pcd) { break }
    $pcd = $parent
}
if (-not $SHARED) {
    Write-Error "Could not locate &.widgits/_shared-lib by walking up from $SCRIPT_DIR"
    exit 1
}
$PRISC_SRC = Join-Path $SHARED "system\prisc+x.c"
if (-not (Test-Path -LiteralPath $PRISC_SRC)) {
    Write-Error "Canonical prisc+x.c not found at $PRISC_SRC"
    exit 1
}
Write-Host "shared lib: $SHARED"

# Create output dirs individually - a single "-Path 'ops/+x system'"
# string is an array PowerShell may not split the way the author intended.
foreach ($d in @("ops\+x", "system")) {
    if (-not (Test-Path -LiteralPath $d)) {
        New-Item -ItemType Directory -Force -Path $d | Out-Null
    }
}

$CFLAGS = @("-Wall", "-Wextra", "-O2")
$failed = New-Object System.Collections.ArrayList

function Build-One([string]$label, [string[]]$extra, [string]$src, [string]$out) {
    Write-Host "  $label"
    $args = @($CFLAGS) + $extra + @($src, "-o", $out)
    & gcc @args
    if ($LASTEXITCODE -ne 0) {
        Write-Host "    FAIL $label" -ForegroundColor Red
        $null = $failed.Add($label)
    } else {
        Write-Host "    OK   $out"
    }
}

Write-Host "--- system ---"
Build-One "prisc+x (canonical shared lib)" @() $PRISC_SRC "system/prisc+x"
Build-One "keyboard_input" @() "system/keyboard_input.c" "system/keyboard_input"
Build-One "renderer" @() "system/renderer.c" "system/renderer"
Build-One "chtpm_parser_pal" @("-Wno-unused-result", "-Wno-stringop-truncation") "system/chtpm_parser_pal.c" "system/chtpm_parser_pal"

Write-Host "--- ops ---"
Build-One "start_scan" @() "ops/start_scan.c" "ops/+x/start_scan.+x"
Build-One "start_compose_frame" @() "ops/start_compose_frame.c" "ops/+x/start_compose_frame.+x"
Build-One "start_menu_input" @() "ops/start_menu_input.c" "ops/+x/start_menu_input.+x"

if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "build FAILED: $($failed.Count) of 7 target(s) did not compile:" -ForegroundColor Red
    foreach ($f in $failed) { Write-Host "  - $f" -ForegroundColor Red }
    exit 1
}

Write-Host "build ok (7/7)"
exit 0
