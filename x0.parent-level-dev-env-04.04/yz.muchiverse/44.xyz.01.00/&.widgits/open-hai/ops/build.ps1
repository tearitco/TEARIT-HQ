# build.ps1 - Windows twin of build_open_hai.sh (open-hai)
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

# build_open_hai.sh — build the taskbar cell 14 ("ai") window.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null

$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-O2", "$(pkg-config --cflags xft)")
$PKG_CFLAGS_xft = $(pkg-config --cflags xft) -split " "
$LIBS = @("-lX11", "-lm", "$(pkg-config --libs xft)")
$PKG_LIBS_xft = $(pkg-config --libs xft) -split " "

# NOTE: v1 uses hand-rolled pixel-math layout (same fallback shape
# db-hq/events-hq are built on), NOT the khtpm CSS engine yet - no
# khtpm_css_parser.c dependency to sync/compile this pass. Real CSS-
# driven styling is a documented follow-up, not done here (see
# OPEN-HAI-GUI-DESIGN.md §4 - the file still #includes the header for
# CssStyle types if a future pass wires it in properly).

# Sync stb_image_write.h from the single canonical source (2026-08-12
# dedup pass - see &.widgits/_shared-lib/README.md) - needed for
# dump_frame_png()'s real PNG+receipt verification (learn to rely on
# receipts, not external screen capture - see that function's own
# header comment).
$SHARED = Join-Path (Split-Path -Parent $SCRIPT_DIR) "../../_shared-lib"
New-Item -ItemType Directory -Force -Path "lib" | Out-Null
Copy-Item -LiteralPath "$SHARED/stb_image_write.h" -Destination "lib/stb_image_write.h" -Force

Write-Host "-- open-hai renderer -> +x/khtpm_open_hai_render.+x"
& $CC @CFLAGS -o +x/khtpm_open_hai_render.+x \
  khtpm_open_hai_render.c @LIBS

Write-Host "OK +x/khtpm_open_hai_render.+x"

# ===== NEXT SCRIPT =====

# build.ps1 - Windows twin of build_open_hai_manager.sh (open-hai)
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

# build_open_hai_manager.sh — build open-hai's MANAGER binary
# (khtpm_open_hai_manager.c), Stage 2d shell/manager split, same real
# mechanism proven on db-hq/events-hq/chat-hai (see khtpm_hq_manager.c's
# own build script). No X11/Xft dependency - this binary never opens a
# window, it only reads/writes plain files + forks curl/tool jobs.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-O2")

Write-Host "-- open-hai manager -> +x/khtpm_open_hai_manager.+x"
& $CC @CFLAGS -o +x/khtpm_open_hai_manager.+x khtpm_open_hai_manager.c

Write-Host "OK +x/khtpm_open_hai_manager.+x"
