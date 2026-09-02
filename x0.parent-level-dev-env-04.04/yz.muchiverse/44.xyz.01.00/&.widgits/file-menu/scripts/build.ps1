# build.ps1 - Windows twin of build.sh (file-menu)
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

# scripts/build.sh - build file-menu widget ops

New-Item -ItemType Directory -Force -Path "ops/+x" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Building file-menu ops ---"
& gcc @CFLAGS -o "ops/+x/fm_set_focus.+x" "ops/fm_set_focus.c"
& gcc @CFLAGS -o "ops/+x/fm_enqueue_cmd.+x" "ops/fm_enqueue_cmd.c"
& gcc @CFLAGS -o "ops/+x/fm_compose_frame.+x" "ops/fm_compose_frame.c"
& gcc @CFLAGS -o "ops/+x/fm_menu_input.+x" "ops/fm_menu_input.c"
& gcc @CFLAGS -o "ops/+x/fm_scan_dir.+x" "ops/fm_scan_dir.c"
& gcc @CFLAGS -o "ops/+x/ledger_append.+x" "ops/ledger_append.c"
& gcc @CFLAGS -o "ops/+x/ledger_peers.+x" "ops/ledger_peers.c"

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

    # Generic on-demand emoji asset generator (PITFALL 57/!.pal-2do.txt
    # 2DO 1 real fix, 2026-07-30) — see 102.editor-📄️00.00/scripts/
    # build.sh's own identical addition for the full rationale.
New-Item -ItemType Directory -Force -Path "ops/+x" | Out-Null
Copy-Item -LiteralPath "$WSR/ops/+x/emoji_gen_atlas.+x" -Destination "ops/+x/emoji_gen_atlas.+x" -Force 2>$null
Copy-Item -LiteralPath "$WSR/ops/+x/emoji_xtract.+x" -Destination "ops/+x/emoji_xtract.+x" -Force 2>$null
Write-Host "copied emoji_gen_atlas.+x + emoji_xtract.+x from wsr-pal"

    # Copy font glyph registry (local to this widget)
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
