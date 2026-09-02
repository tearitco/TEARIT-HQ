# build.ps1 - Windows twin of build.sh (TSOTS-OG+01.00)
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

# scripts/build.sh - compile TSOTS (house pal-native, adapted from
# 102.agy-txt/scripts/build.sh's own real shape - same pattern, own ops).

New-Item -ItemType Directory -Force -Path "ops/+x system" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Copying system processes (from wsr-pal) ---"
$WSR = "(Resolve-Path "$SCRIPT_DIR/../..").Path/014.wsr-pal💸️📌️+2"
if ((Test-Path \""$WSR"\")) {
Copy-Item -LiteralPath "$WSR/system/prisc+x" -Destination "system/prisc+x" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/keyboard_input" -Destination "system/keyboard_input" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/renderer" -Destination "system/renderer" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/chtpm_parser_pal" -Destination "system/chtpm_parser_pal" -Force 2>$null
Write-Host "copied system/ from wsr-pal"
} else {
Write-Host "WARN: wsr-pal not found - system/ not copied"
}

Write-Host "--- Building TSOTS ops ---"
& gcc @CFLAGS -o "ops/+x/tsots_compose.+x" "ops/tsots_compose.c"
& gcc @CFLAGS -o "ops/+x/tsots_input.+x" "ops/tsots_input.c"
& gcc @CFLAGS -o "ops/+x/tsots_deal.+x" "ops/tsots_deal.c"

Write-Host "--- Copying GL/RGB system binaries (§35 GL-primary) ---"
if ((Test-Path \""$WSR"\")) {
Copy-Item -LiteralPath "$WSR/system/chtpm_rgb_render" -Destination "system/chtpm_rgb_render" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/gl_mirror" -Destination "system/gl_mirror" -Force 2>$null
Write-Host "copied chtpm_rgb_render + gl_mirror from wsr-pal"

Write-Host "--- Copying font glyph registry (required, project-local by design) ---"
New-Item -ItemType Directory -Force -Path "$SCRIPT_DIR/pieces/registry/fonts/ascii" | Out-Null
Get-ChildItem "$WSR/pieces/registry/fonts/ascii/"*/" | ForEach-Object {
    $dir = $_.FullName
if (-not (Test-Path "$dir")) { continue }
        code="(Split-Path -Leaf "$dir")"
New-Item -ItemType Directory -Force -Path "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code" | Out-Null
Copy-Item -LiteralPath "$dir/glyph.txt" -Destination "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code/glyph.txt" -Force
}
Write-Host "glyphs: @(Get-ChildItem "$SCRIPT_DIR/pieces/registry/fonts/ascii/").Count"
} else {
Write-Host "WARN: wsr-pal not found, GL/RGB binaries not linked (ASCII-only fallback stays available)"
}

Write-Host "build ok"
