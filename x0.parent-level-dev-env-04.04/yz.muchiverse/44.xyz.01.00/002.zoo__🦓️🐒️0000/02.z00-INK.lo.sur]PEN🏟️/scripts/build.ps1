# build.ps1 - Windows twin of build.sh (02.z00-INK.lo.sur]PEN🏟️)
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
# headers, matching this project family's own convention (see
# mutaclsym/dox/00-HANDOFF.md's own build.sh for the sibling reference
# this was copied in shape from).
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/keyboard_input.c, system/chtpm_parser_pal.c,
# system/chtpm_rgb_render.c, ops/xlector_input.c, ops/move_entity.c,
# ops/pdl_reader.c, ops/pet_export.c, ops/pet_import.c,
# ops/dump_rgb_png.c, ops/lib/) - direct user instruction: "i dont wanna
# use shared ops, in the classic sense... all code should be self
# independant and solo shippable." This build.sh never reaches outside
# this project's own directory. To pull in an update from the canonical
# source, run (from yz.muchiverse/2.muchi-verse/):
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

Write-Host "-- gl_mirror (optional GL/GLUT reader - mutaclsym-style: shows the"
Write-Host "   LEVEL ONLY (map+xlector, no pets - see ops/compose_rgb_frame.c's"
Write-Host "   own header comment), the only file allowed to call GL primitives)"
if (& $CC $CFLAGS -o system/gl_mirror system/gl_mirror.c -lglut -lGL -lGLU 2>/tmp/gl_mirror_build.log) {
Write-Host "   ok"
} else {
Write-Host "   skipped (GLUT/GL not available - see /tmp/gl_mirror_build.log)"
Remove-Item -LiteralPath "/tmp/gl_mirror_build.log" -ErrorAction SilentlyContinue
}

Write-Host "-- zoo_window (optional GL/X11 desktop window per piece - real"
Write-Host "   drag-to-grid-snap, ported from egg-pals' own system/egg_window.c"
Write-Host "   - see dox/pet-import-export-standard.md for the port notes. NOT"
Write-Host "   launched by 'run' - that's now z0.egg-pals+13's job; kept here"
Write-Host "   only for manual './button.sh window <id>' testing.)"
if (& $CC $CFLAGS -o system/zoo_window system/zoo_window.c -lX11 -lXext -lGL -lGLU -lm 2>/tmp/zoo_window_build.log) {
Write-Host "   ok"
} else {
Write-Host "   skipped (X11/GLX not available - see /tmp/zoo_window_build.log)"
Remove-Item -LiteralPath "/tmp/zoo_window_build.log" -ErrorAction SilentlyContinue
}

Write-Host "-- local copies of formerly-shared ops (see this file's own header"
Write-Host "   comment - sync_shared_op.sh propagates canonical updates here)"
& $CC @CFLAGS -o ops/+x/xlector_input.+x ops/xlector_input.c
& $CC @CFLAGS -o ops/+x/move_entity.+x ops/move_entity.c
& $CC @CFLAGS -o ops/+x/pdl_reader.+x ops/pdl_reader.c
& $CC @CFLAGS -o ops/+x/pet_export.+x ops/pet_export.c
& $CC @CFLAGS -o ops/+x/pet_import.+x ops/pet_import.c
Write-Host "-- dump_rgb_png (local copy - DEBUG TOOL, not wired into pal/"
Write-Host "   main_loop.pal or default_op.txt - run manually to see the GL"
Write-Host "   mirror's actual pixels as a real PNG)"
& $CC @CFLAGS -I"ops/lib" -o ops/+x/dump_rgb_png.+x ops/dump_rgb_png.c
Write-Host "-- palnet_peer (local copy - reusable P2P companion process, see"
Write-Host "   PAL-NET-STANDARD.txt - launched by button.sh alongside"
Write-Host "   gl_mirror, not compiled into any GUI process directly)"
& $CC @CFLAGS -o ops/+x/palnet_peer.+x ops/palnet_peer.c

Write-Host "-- zoo_0000-specific ops"
& $CC @CFLAGS -o ops/+x/tick_pets.+x ops/tick_pets.c
& $CC @CFLAGS -o ops/+x/compose_frame.+x ops/compose_frame.c
& $CC @CFLAGS -o ops/+x/compose_rgb_frame.+x ops/compose_rgb_frame.c
& $CC @CFLAGS -o ops/+x/feed_pet.+x ops/feed_pet.c
& $CC @CFLAGS -o ops/+x/pet_pet.+x ops/pet_pet.c
& $CC @CFLAGS -o ops/+x/play_pet.+x ops/play_pet.c

Write-Host "build ok"
