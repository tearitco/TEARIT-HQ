# build.ps1 - Windows twin of build.sh (102.editor-📄️00.00)
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

# scripts/build.sh - compile agy-editor (house pal-native, login-signup shape)

New-Item -ItemType Directory -Force -Path "ops/+x system" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Building system processes ---"
& gcc @CFLAGS "system/prisc+x.c" -o "system/prisc+x"
& gcc @CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
& gcc @CFLAGS "system/renderer.c" -o "system/renderer"

Write-Host "--- Building chtpm_parser_pal ---"
& gcc @CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

Write-Host "--- Building editor ops ---"
& gcc @CFLAGS -o "ops/+x/editor_menu_input.+x" "ops/editor_menu_input.c"
& gcc @CFLAGS -o "ops/+x/editor_compose_frame.+x" "ops/editor_compose_frame.c"
& gcc @CFLAGS -o "ops/+x/editor_widget_cmds.+x" "ops/editor_widget_cmds.c"
& gcc @CFLAGS -o "ops/+x/ledger_append.+x" "ops/ledger_append.c"

Write-Host "--- Copying GL/RGB system binaries (§35 GL-primary — generic, no editor-specific logic) ---"
# NOTE: editor lives directly under the house root (one level up from
# here), unlike &.widgits/* projects which sit one level deeper — do
# not copy &.widgits/file-menu/scripts/build.sh's own "../.." verbatim.
$WSR = "(Resolve-Path "$SCRIPT_DIR/..").Path/014.wsr-pal💸️📌️+2"
if ((Test-Path \""$WSR"\")) {
Copy-Item -LiteralPath "$WSR/system/chtpm_rgb_render" -Destination "system/chtpm_rgb_render" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/gl_mirror" -Destination "system/gl_mirror" -Force 2>$null
Write-Host "copied chtpm_rgb_render + gl_mirror from wsr-pal"

    # Generic on-demand emoji asset generator (PITFALL 57/!.pal-2do.txt
    # 2DO 1 real fix, 2026-07-30) — chtpm_rgb_render.c's own
    # ensure_emoji_asset_generated() shells out to these two ops,
    # resolved relative to THIS project's own PRISC_PROJECT_ROOT, so
    # each project needs its own copy (matching the chtpm_rgb_render/
    # gl_mirror pattern above exactly).
New-Item -ItemType Directory -Force -Path "ops/+x" | Out-Null
Copy-Item -LiteralPath "$WSR/ops/+x/emoji_gen_atlas.+x" -Destination "ops/+x/emoji_gen_atlas.+x" -Force 2>$null
Copy-Item -LiteralPath "$WSR/ops/+x/emoji_xtract.+x" -Destination "ops/+x/emoji_xtract.+x" -Force 2>$null
Write-Host "copied emoji_gen_atlas.+x + emoji_xtract.+x from wsr-pal"

    # Font glyph registry — REQUIRED for chtpm_rgb_render, project-local
    # by design, never shared/symlinked from wsr-pal directly (see
    # &.widgits/file-menu/fm-widget-fix.md's own "Missing font glyph
    # registry" root-cause writeup: without pieces/registry/fonts/ascii/
    # <code>/glyph.txt present in THIS project's own tree, every
    # character renders as invisible — all glyph pixels zero, GL window
    # comes up solid black even though the pipeline is otherwise fully
    # working). Same copy-from-wsr-pal-once, own-a-local-copy pattern
    # file-menu's own build.sh already uses.
Write-Host "--- Copying font glyph registry ---"
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
