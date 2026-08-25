# build.ps1 - Windows twin of build.sh (setup)
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

# scripts/build.sh - build the Match Setup WIDGIT's ops + copy system
# binaries as LOCAL COPIES (real code over docs - same principle as
# &.widgits/board-viewer/scripts/build.sh, which itself follows
# &.widgits/file-menu/scripts/build.sh's own proven pattern).

New-Item -ItemType Directory -Force -Path "ops/+x" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Building setup widget ops ---"
& gcc @CFLAGS -o "ops/+x/setup_set_focus.+x" "ops/setup_set_focus.c"
& gcc @CFLAGS -o "ops/+x/setup_enqueue_cmd.+x" "ops/setup_enqueue_cmd.c"
& gcc @CFLAGS -o "ops/+x/setup_menu_input.+x" "ops/setup_menu_input.c"
& gcc @CFLAGS -o "ops/+x/setup_compose_frame.+x" "ops/setup_compose_frame.c"
& gcc @CFLAGS -o "ops/+x/ledger_append.+x" "ops/ledger_append.c"

Write-Host "--- Copying system binaries (local copies) ---"
$HOUSE = "(Resolve-Path "$SCRIPT_DIR/../../../..").Path"
$WSR = "$HOUSE/014.wsr-pal💸️📌️+2"
$MUT = "$HOUSE/101.mutaclsym🧟‍♂️️+18.01"
if ((Test-Path \""$WSR"\")) {
New-Item -ItemType Directory -Force -Path "system" | Out-Null
Copy-Item -LiteralPath "$WSR/system/prisc+x" -Destination "system/prisc+x" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/chtpm_parser_pal" -Destination "system/chtpm_parser_pal" -Force 2>$null
    # chtpm_rgb_render: mutaclysm's own fork when available (has the
    # overlay/MAP3D machinery board-viewer needs; for this text-only
    # widget the generic wsr-pal copy would also do, so fall back).
if ((Test-Path \""$MUT"\")) {
Copy-Item -LiteralPath "$MUT/system/chtpm_rgb_render" -Destination "system/chtpm_rgb_render" -Force 2>$null
} else {
Copy-Item -LiteralPath "$WSR/system/chtpm_rgb_render" -Destination "system/chtpm_rgb_render" -Force 2>$null
}
Copy-Item -LiteralPath "$WSR/system/keyboard_input" -Destination "system/keyboard_input" -Force 2>$null
Copy-Item -LiteralPath "$WSR/system/renderer" -Destination "system/renderer" -Force 2>$null
    # gl_mirror MUST be the 014.wsr-pal version (real interact_relay
    # forwarding) - NEVER mutaclysm's own mirror-only copy. Compiled from
    # wsr-pal's SOURCE here (not copied) so it also carries the optional
    # GL_MIRROR_X/GL_MIRROR_Y window-placement support that lets a
    # WIDGIT's own GL window open BESIDE the host's instead of on top of
    # it (live-observed overlap in TSC_ELO: two glut windows, same spot).
if ((Test-Path \""$WSR/system/gl_mirror.c"\")) {
        if gcc -Wall -Wextra -O2 -o "system/gl_mirror" "$WSR/system/gl_mirror.c" \
            -lglut -lGL -lGLU -lX11 2>&1 | Out-File \"/tmp/tsc_widget_gl_mirror_build.log;\" -Encoding utf8 then
Write-Host "    gl_mirror: built ok (wsr-pal source, GL_MIRROR_X/Y support)"
} else {
Write-Host "    gl_mirror: source build failed, copying binary instead"
            cat /tmp/tsc_widget_gl_mirror_build.log >&2
Copy-Item -LiteralPath "$WSR/system/gl_mirror" -Destination "system/gl_mirror" -Force 2>$null
Remove-Item -LiteralPath "/tmp/tsc_widget_gl_mirror_build.log" -ErrorAction SilentlyContinue
}
} else {
Copy-Item -LiteralPath "$WSR/system/gl_mirror" -Destination "system/gl_mirror" -Force 2>$null
}

Write-Host "--- Copying font glyph registry (real local copy) ---"
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
Write-Host "WARN: wsr-pal not found at $WSR, system binaries not copied"
}

Write-Host "build ok"
