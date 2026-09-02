# build.ps1 - Windows twin of build.sh (101.mutaclsym🧟‍♂️️+18.0G)
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

# Builds every binary independently - no shared object files, no shared
# headers, matching cdda-tpm-std-fast.txt sec. 1. Each translation unit
# is compiled and linked on its own line.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/keyboard_input.c, system/chtpm_parser_pal.c,
# system/chtpm_rgb_render.c, ops/pdl_reader.c, ops/dump_rgb_png.c,
# ops/lib/) - direct user instruction: "i dont wanna use shared ops, in
# the classic sense... all code should be self independant and solo
# shippable." This build.sh never reaches outside this project's own
# directory. To pull in an update from the canonical source, run (from
# yz.muchiverse/2.muchi-verse/):
#   bash sync_shared_op.sh <op_name> <target_dir>
# (see that directory's own shared-ops-manifest.txt for available
# op_name values and this project's own consumer list) - a deliberate,
# explicit step, never automatic at build time.

$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-Wextra", "-O2")

Write-Host "-- prisc+x (VM)"
& $CC @CFLAGS -o system/prisc+x system/prisc+x.c

Write-Host "-- keyboard_input (raw termios, no ncurses - local copy, see"
Write-Host "   system/keyboard_input.c's own header comment)"
& $CC @CFLAGS -o system/keyboard_input system/keyboard_input.c

Write-Host "-- renderer (plain stdout, no ncurses)"
& $CC @CFLAGS -o system/renderer system/renderer.c

Write-Host "-- chtpm_parser_pal (PERSISTENT process, not a one-shot op - see"
Write-Host "   chtpm-to-pal-layout-plan.txt and that file's own header"
Write-Host "   comment. -Wno-unused-result -Wno-stringop-truncation are"
Write-Host "   REQUIRED on this one file - confirmed via a real test build"
Write-Host "   this gets to zero warnings.)"
& $CC @CFLAGS -Wno-unused-result -Wno-stringop-truncation -o system/chtpm_parser_pal system/chtpm_parser_pal.c

Write-Host "-- chtpm_rgb_render (local copy, PERSISTENT daemon - real wraith_rgb_daemon.c"
Write-Host "   equivalent: font-rasterizes pieces/display/current_frame.txt"
Write-Host "   verbatim, zero .chtpm awareness - see that file's own header"
Write-Host "   comment. Writes the SAME rgb_frame.raw/receipt gl_mirror already"
Write-Host "   reads, sized to gl_mirror's own hardcoded 640x304 on purpose.)"
& $CC @CFLAGS -o system/chtpm_rgb_render system/chtpm_rgb_render.c

Write-Host "-- gl_mirror (optional GL/GLUT reader - only file allowed to call"
Write-Host "   GL primitives, see GOVERNING CONSTRAINT in"
Write-Host "   2.muchi-verse/GRAND-ARCHITECTURE.md - built best-effort so a"
Write-Host "   machine without GLUT dev headers/libs can still build the rest)"
if (& $CC $CFLAGS -o system/gl_mirror system/gl_mirror.c -lglut -lGL -lGLU -lX11 2>/tmp/gl_mirror_build.log) {
Write-Host "   ok"
} else {
Write-Host "   skipped (GLUT/GL not available - see /tmp/gl_mirror_build.log)"
Remove-Item -LiteralPath "/tmp/gl_mirror_build.log" -ErrorAction SilentlyContinue
}

Write-Host "-- ops"
& $CC @CFLAGS -o ops/+x/move_player.+x ops/move_player.c
& $CC @CFLAGS -o ops/+x/camera_control.+x ops/camera_control.c
& $CC @CFLAGS -o ops/+x/end_turn.+x ops/end_turn.c
& $CC @CFLAGS -o ops/+x/compose_frame.+x ops/compose_frame.c
& $CC @CFLAGS -o ops/+x/compose_rgb_frame.+x ops/compose_rgb_frame.c -lm
Write-Host "-- dump_rgb_png (local copy - DEBUG TOOL, not wired into pal/"
Write-Host "   main_loop.pal or default_op.txt - run manually to see the RGB"
Write-Host "   mirror's actual pixels as a real PNG, since this agent has no"
Write-Host "   way to view the live GLUT window itself)"
& $CC @CFLAGS -I"ops/lib" -o ops/+x/dump_rgb_png.+x ops/dump_rgb_png.c -lm

Write-Host "-- emoji_gen_atlas / emoji_xtract (local copies, own source - same"
Write-Host "   'solo shippable' convention as everything else in this file, NOT"
Write-Host "   a shared-ops reference; ported from #.emoji-studio-501.02.05t/"
Write-Host "   &.emoji-studio-solo.02.01/'s own generic FreeType pipeline, see"
Write-Host "   compose_rgb_frame.c's own ensure_emoji_asset_ready() header"
Write-Host "   comment for why this project needs its own copy on-demand)"
& $CC @CFLAGS -I"ops/lib" -I/usr/include/freetype2 -o ops/+x/emoji_gen_atlas.+x ops/emoji_gen_atlas.c -lfreetype -lm
& $CC @CFLAGS -I"ops/lib" -o ops/+x/emoji_xtract.+x ops/emoji_xtract.c -lm
& $CC @CFLAGS -o ops/+x/pickup.+x ops/pickup.c
& $CC @CFLAGS -o ops/+x/drop.+x ops/drop.c
& $CC @CFLAGS -o ops/+x/eat.+x ops/eat.c
& $CC @CFLAGS -o ops/+x/tick_monsters.+x ops/tick_monsters.c
& $CC @CFLAGS -o ops/+x/craft.+x ops/craft.c
& $CC @CFLAGS -o ops/+x/examine.+x ops/examine.c
& $CC @CFLAGS -o ops/+x/save_game.+x ops/save_game.c
& $CC @CFLAGS -o ops/+x/toggle_emoji.+x ops/toggle_emoji.c
& $CC @CFLAGS -o ops/+x/title_input.+x ops/title_input.c
& $CC @CFLAGS -o ops/+x/compose_title_frame.+x ops/compose_title_frame.c
& $CC @CFLAGS -o ops/+x/pdl_reader.+x ops/pdl_reader.c
& $CC @CFLAGS -o ops/+x/choice.+x ops/choice.c
& $CC @CFLAGS -o ops/+x/game_dispatch.+x ops/game_dispatch.c
& $CC @CFLAGS -o ops/+x/generate_map.+x ops/generate_map.c

Write-Host "build ok"
