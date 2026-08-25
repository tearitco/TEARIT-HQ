# build.ps1 - Windows twin of build.sh (01.avatar-creation👤️)
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

# Build avatar-creation system + ops

New-Item -ItemType Directory -Force -Path "ops/+x system" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- system ---"
& gcc @CFLAGS "system/prisc+x.c" -o "system/prisc+x"
& gcc @CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
& gcc @CFLAGS "system/renderer.c" -o "system/renderer"
& gcc @CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

# emoji + desktop window (from muchi-pals sources)
if ((Test-Path \"system/emoji_gen_atlas.c\")) {
  & gcc @CFLAGS -I system/lib @FT_CFLAGS system/emoji_gen_atlas.c -o system/emoji_gen_atlas @FT_LIBS -lm 2>/dev/null \
    && echo "OK   emoji_gen_atlas" || echo "SKIP emoji_gen_atlas"
}
if ((Test-Path \"system/emoji_xtract.c\")) {
  & gcc @CFLAGS -I system/lib system/emoji_xtract.c -o system/emoji_xtract -lm 2>/dev/null \
    && echo "OK   emoji_xtract" || echo "SKIP emoji_xtract"
}
if ((Test-Path \"system/avatar_window.c\")) {
  & gcc @CFLAGS -I system/lib system/avatar_window.c -o system/avatar_window \
    -lGL -lX11 -lXext -lm 2>/dev/null \
    && echo "OK   avatar_window" || echo "SKIP avatar_window (no X/GL?)"
}
if ((Test-Path \"system/character_preview.c\")) {
  & gcc @CFLAGS system/character_preview.c -o system/character_preview \
    -lGL -lGLU -lglut -lm 2>/dev/null \
    && echo "OK   character_preview" || echo "SKIP character_preview (needs GLUT/GL)"
}

Write-Host "--- ops ---"
for op in generate_clone claim_tokens buy_clone cycle_dna apply_name_age \
          toggle_sleep open_avatar_window open_character_preview \
          ensure_user_identity hydrate_avatars make_avatar_sprite \
          avatar_menu_input avatar_compose_frame; do
  & gcc @CFLAGS -o "ops/+x/${op}.+x" "ops/${op}.c" && echo "OK   $op" || echo "FAIL $op"
}
Write-Host "build ok"
