# build.ps1 - Windows twin of build.sh (my-chara-txt)
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
# real local copies of 041.pal-chain's own proven system/ sources (same
# convention every project in this family follows - see that project's
# own scripts/build.sh header comment / ../shared-ops-manifest.txt).

New-Item -ItemType Directory -Force -Path "ops/+x system" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Building system processes ---"
& gcc @CFLAGS "system/prisc+x.c" -o "system/prisc+x"
& gcc @CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
& gcc @CFLAGS "system/renderer.c" -o "system/renderer"

Write-Host "--- Building chtpm_parser_pal (PERSISTENT process, -Wno-unused-result"
Write-Host "    -Wno-stringop-truncation required - see chtpm_parser_pal.c) ---"
& gcc @CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

Write-Host "--- Building chtpm_rgb_render (local copy, PERSISTENT daemon) ---"
& gcc @CFLAGS "system/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

Write-Host "--- Building orchestrator ---"
& gcc @CFLAGS -o "system/orchestrator" "system/orchestrator.c"

Write-Host "--- Building chtpm_rgb_render (local copy, PERSISTENT daemon) ---"
& gcc @CFLAGS -o "system/chtpm_rgb_render" "system/chtpm_rgb_render.c"

Write-Host "--- Building gl_mirror (optional GL/GLUT reader, skips gracefully"
Write-Host "    if GLUT/GL dev libs aren't available) ---"
if (gcc $CFLAGS -o "system/gl_mirror" "system/gl_mirror.c" -lglut -lGL -lGLU -lX11 2>/tmp/mychara_gl_mirror_build.log) {
Write-Host "    gl_mirror: built ok"
} else {
Write-Host "    gl_mirror: skipped (GLUT/GL not available - see /tmp/mychara_gl_mirror_build.log)"
Remove-Item -LiteralPath "/tmp/mychara_gl_mirror_build.log" -ErrorAction SilentlyContinue
}

Write-Host "--- Building my-chara-txt ops ---"
& gcc @CFLAGS -o "ops/+x/mychara_menu_input.+x" "ops/mychara_menu_input.c"
& gcc @CFLAGS -o "ops/+x/mychara_compose_frame.+x" "ops/mychara_compose_frame.c"
& gcc @CFLAGS -o "ops/+x/mychara_ai_decide.+x" "ops/mychara_ai_decide.c"

Write-Host "build ok"
