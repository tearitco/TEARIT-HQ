# build.ps1 - Windows twin of build.sh (045.muchi-pal-agent🤖️+1++)
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

# scripts/build.sh - compile everything, warning-free, matching
# mutaclsym's standing bar (see its dox/01-cdda-architecture.md §8).

New-Item -ItemType Directory -Force -Path "system ops/+x manager/+x" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Building system processes ---"
& gcc @CFLAGS "system/prisc+x.c" -o "system/prisc+x"
& gcc @CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
& gcc @CFLAGS "system/renderer.c" -o "system/renderer"

Write-Host "--- Building chtpm_parser_pal (PERSISTENT process, -Wno-unused-result"
Write-Host "    -Wno-stringop-truncation required - see shared-ops/chtpm_parser_pal.c) ---"
& gcc @CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

Write-Host "--- Building chtpm_rgb_render (PERSISTENT daemon - RGB mirror pipeline,"
Write-Host "    font-rasterizes pieces/display/current_frame.txt into"
Write-Host "    pieces/display/rgb_frame.raw; -Wno-format-truncation for one real"
Write-Host "    gcc warning in the on-demand emoji csv_path snprintf) ---"
& gcc @CFLAGS -Wno-format-truncation "system/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

Write-Host "--- Building gl_mirror (optional GL/GLUT reader - only file allowed to"
Write-Host "    call GL primitives; built best-effort so a machine without GLUT/GL"
Write-Host "    can still build the rest) ---"
if (gcc $CFLAGS -o system/gl_mirror system/gl_mirror.c -lglut -lGL -lGLU 2>/tmp/agent_gl_mirror_build.log) {
Write-Host "    ok"
} else {
Write-Host "    skipped (GLUT/GL not available - see /tmp/agent_gl_mirror_build.log)"
Remove-Item -LiteralPath "/tmp/agent_gl_mirror_build.log" -ErrorAction SilentlyContinue
}

Write-Host "--- Building ops ---"
foreach ($src in @("ops/*.c")) {
    name="(Split-Path -Leaf "$src" -LeafBase)"
    case "$name" in
        emoji_gen_atlas|emoji_xtract) continue ;;
Write-Host "  Compiling $name..."
    & gcc @CFLAGS "$src" -o "ops/+x/$name.+x"
}

Write-Host "--- Building emoji ops (on-demand FreeType emoji generator used by"
Write-Host "    chtpm_rgb_render's generic path - freetype headers required) ---"
& gcc @CFLAGS -I/usr/include/freetype2 -o "ops/+x/emoji_gen_atlas.+x" "ops/emoji_gen_atlas.c" -lfreetype -lm
& gcc @CFLAGS -o "ops/+x/emoji_xtract.+x" "ops/emoji_xtract.c" -lm

Write-Host "--- Building manager (PERSISTENT process - path_nav_manager, see"
Write-Host "    that file's own header comment) ---"
& gcc @CFLAGS "manager/path_nav_manager.c" -o "manager/+x/path_nav_manager.+x"

Write-Host "--- Build Complete ---"
ls -l system/prisc+x system/keyboard_input system/renderer system/chtpm_parser_pal ops/+x/ manager/+x/
