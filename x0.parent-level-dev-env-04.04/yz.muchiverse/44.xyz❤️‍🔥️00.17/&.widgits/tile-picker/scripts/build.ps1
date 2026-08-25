# build.ps1 - Windows twin of build.sh (tile-picker)
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

# scripts/build.sh - build tile-picker widget ops + copy system pipeline,
# modeled directly on &.widgits/file-menu/scripts/build.sh.

New-Item -ItemType Directory -Force -Path "ops/+x" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Building tile-picker ops ---"
& gcc @CFLAGS -o "ops/+x/tp_set_brush.+x" "ops/tp_set_brush.c"
& gcc @CFLAGS -o "ops/+x/tp_place.+x" "ops/tp_place.c"
& gcc @CFLAGS -o "ops/+x/tp_place_desktop.+x" "ops/tp_place_desktop.c"
& gcc @CFLAGS -o "ops/+x/tp_import_from_desktop.+x" "ops/tp_import_from_desktop.c"
# tp_desktop_window.c (the original GLX entity renderer) is retired
# (2026-08-12, direct instruction: "we may also get rid of the
# fallback at this point. its just more context to confuse future
# agents") - moved to !.deprecated-2026-08-12/, no longer built here.
# tp_desktop_window_rgb.c + tp_asset_to_sprite.c + tp_range_grid.c +
# stb_image.h moved OUT of tile-picker into the livedesk-taskbar runtime
# (2026-08-14 consolidation: *.monads/*.livedesk-taskbar/ops/, built by
# that folder's build_khtpm_strip.sh) - the entity window has nothing to
# do with this widget anymore. tp_place_desktop still spawns it; it now
# resolves the binary dynamically (see tp_place_desktop.c).
& gcc @CFLAGS -o "ops/+x/tp_compose_frame.+x" "ops/tp_compose_frame.c"
& gcc @CFLAGS -o "ops/+x/tp_menu_input.+x" "ops/tp_menu_input.c"
& gcc @CFLAGS -o "ops/+x/tp_test_send_key.+x" "ops/tp_test_send_key.c" -lX11 -lXtst
# REAL, 2026-08-16 - tp_test_send_key.c's own real mouse-click sibling
# (XTest-direct, no xdotool - see this file's own header comment on why
# 101.drag-drop-test's own dd_drag_drop.c-shaped tools don't actually
# run here: xdotool isn't installed on this machine).
& gcc @CFLAGS -o "ops/+x/tp_test_send_click.+x" "ops/tp_test_send_click.c" -lX11 -lXtst
& gcc @CFLAGS -o "ops/+x/tp_find_window_by_pid.+x" "ops/tp_find_window_by_pid.c" -lX11
& gcc @CFLAGS -o "ops/+x/tp_set_wm_pid.+x" "ops/tp_set_wm_pid.c" -lX11
& gcc @CFLAGS -o "ops/+x/ledger_peers.+x" "ops/ledger_peers.c"
& gcc @CFLAGS -o "ops/+x/tp_arm_placer.+x" "ops/tp_arm_placer.c" -lX11
gcc -Wall -O2 -o "ops/+x/tp_rmmv_character_extract.+x" "ops/tp_rmmv_character_extract.c" -lm

# khtpm_choice_picker.+x + khtpm_show_choices.+x - REAL FIX 2026-08-16
# ("its very old lets fix it to use khtpm"): the real khtpm-based Show
# Choices picker (replaces the old GLX-based tp_picker_window.c, which
# was silently deployed under the khtpm_show_choices.+x name and never
# actually produced a visible window - a real, separate, pre-existing
# bug). 7th real consumer of &.widgits/_shared-lib/khtpm_render_core.c,
# same shared-source-copy convention as build_entity_menu.sh.
SHARED_LIB="(Resolve-Path "$SCRIPT_DIR/../_shared-lib").Path"
Copy-Item -LiteralPath "$SHARED_LIB/khtpm_render_core.c" -Destination "ops/khtpm_render_core.c" -Force
Copy-Item -LiteralPath "$SHARED_LIB/khtpm_css_parser.h" -Destination "ops/khtpm_css_parser.h" -Force
gcc -std=c11 -Wall -O2 $(pkg-config --cflags xft) -o "ops/+x/khtpm_choice_picker.+x" "ops/khtpm_choice_picker.c" -lX11 $(pkg-config --libs xft) -lm
gcc -Wall -O2 -o "ops/+x/khtpm_show_choices.+x" "ops/khtpm_show_choices.c"

Write-Host "--- Copying system binaries (local copies for dev) ---"
$WSR = "(Resolve-Path "$SCRIPT_DIR/../..").Path/014.wsr-pal💸️📌️+2"
if ((Test-Path \""$WSR"\")) {
New-Item -ItemType Directory -Force -Path "system" | Out-Null
Copy-Item -LiteralPath "$WSR/system/prisc+x" -Destination "system/prisc+x" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/chtpm_parser_pal" -Destination "system/chtpm_parser_pal" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/chtpm_rgb_render" -Destination "system/chtpm_rgb_render" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/keyboard_input" -Destination "system/keyboard_input" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/renderer" -Destination "system/renderer" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/gl_mirror" -Destination "system/gl_mirror" -Force 2>$null
Copy-Item -LiteralPath "$WSR/default_op.txt" -Destination "system/default_op.txt" -Force 2>$null
Write-Host "copied from wsr-pal"

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
Write-Host "WARN: wsr-pal not found, system binaries not linked"
}

Write-Host "build ok"
