# build.ps1 - Windows twin of build_db_hq.sh (*.livedesk-taskbar)
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

# build_db_hq.sh — build the db-hq standalone renderer (khtpm_hq_render.c +
# khtpm_css_parser.c), the first proof of the HQML CSS layer
# (au11-hq/HQML-DESIGN+PLANS.md). Separate from build_khtpm_strip.sh on
# purpose - db-hq is its own binary/process, not part of the taskbar pair,
# so a db-hq build/rebuild never touches or risks the taskbar's own two
# binaries.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
# macOS leg: XQuartz's Xft.pc lives under /opt/X11/lib/pkgconfig, invisible
# to brew's pkg-config by default; guarded — Linux behavior unchanged.
if ([ "$env:OS" = "Darwin" ]) {
} else {
}
$CFLAGS = @("-std=c11", "-Wall", "-O2", "$(pkg-config --cflags xft)")
$PKG_CFLAGS_xft = $(pkg-config --cflags xft) -split " "
$LIBS = @("-lX11", "-lm", "$(pkg-config --libs xft)")
$PKG_LIBS_xft = $(pkg-config --libs xft) -split " "

# Sync shared files from the single canonical source (2026-08-12
# dedup pass - see &.widgits/_shared-lib/README.md).
$SHARED = Join-Path (Split-Path -Parent $SCRIPT_DIR) "../../../&.widgits/_shared-lib"
Copy-Item -LiteralPath "$SHARED/khtpm_css_parser.c" -Destination "khtpm_css_parser.c" -Force
Copy-Item -LiteralPath "$SHARED/khtpm_css_parser.h" -Destination "khtpm_css_parser.h" -Force
Copy-Item -LiteralPath "$SHARED/khtpm_render_core.c" -Destination "khtpm_render_core.c" -Force
New-Item -ItemType Directory -Force -Path "lib" | Out-Null
Copy-Item -LiteralPath "$SHARED/stb_image_write.h" -Destination "lib/stb_image_write.h" -Force

Write-Host "-- db-hq renderer -> +x/khtpm_hq_render.+x"
& $CC @CFLAGS @X11_FLAGS -o +x/khtpm_hq_render.+x \
  khtpm_hq_render.c khtpm_css_parser.c khtpm_taskbar_manager.c @LIBS

Write-Host "OK +x/khtpm_hq_render.+x"

# ===== NEXT SCRIPT =====

# build.ps1 - Windows twin of build_db_hq_manager.sh (*.livedesk-taskbar)
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

# build_db_hq_manager.sh — build db-hq's MANAGER binary (khtpm_hq_manager.c),
# Stage 2d shell/manager split (au11-hq/khtpm-merge-how2.md +
# local-2do-15.txt's own "Stage 2d, REDONE correctly" entry, 2026-08-16).
# Separate from build_db_hq.sh on purpose - the shell (khtpm_hq_render.c)
# and this manager are independent standalone binaries, launched/killed
# together by open_db_hq.sh, same pairing shape as khtpm_strip_parser.c +
# khtpm_taskbar_manager_main.c. No X11/Xft dependency at all - this
# binary never opens a window, it only reads/writes plain files.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-O2")

Write-Host "-- db-hq manager -> +x/khtpm_hq_manager.+x"
& $CC @CFLAGS -o +x/khtpm_hq_manager.+x khtpm_hq_manager.c

Write-Host "OK +x/khtpm_hq_manager.+x"

# ===== NEXT SCRIPT =====

# build.ps1 - Windows twin of build_entity_menu.sh (*.livedesk-taskbar)
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

# build_entity_menu.sh — build khtpm_entity_menu_render.c, Stage 2c PROOF
# (ONE-entity test case, see local-2do-15.txt's own entry). Same real
# shared-source convention as build_db_hq.sh - not invented.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
# macOS leg: XQuartz's Xft.pc lives under /opt/X11/lib/pkgconfig, invisible
# to brew's pkg-config by default; guarded — Linux behavior unchanged.
if ([ "$env:OS" = "Darwin" ]) {
} else {
}
$CFLAGS = @("-std=c11", "-Wall", "-O2", "$(pkg-config --cflags xft)")
$PKG_CFLAGS_xft = $(pkg-config --cflags xft) -split " "
$LIBS = @("-lX11", "-lm", "$(pkg-config --libs xft)")
$PKG_LIBS_xft = $(pkg-config --libs xft) -split " "

$SHARED = Join-Path (Split-Path -Parent $SCRIPT_DIR) "../../../&.widgits/_shared-lib"
Copy-Item -LiteralPath "$SHARED/khtpm_css_parser.c" -Destination "khtpm_css_parser.c" -Force
Copy-Item -LiteralPath "$SHARED/khtpm_css_parser.h" -Destination "khtpm_css_parser.h" -Force
Copy-Item -LiteralPath "$SHARED/khtpm_render_core.c" -Destination "khtpm_render_core.c" -Force
Copy-Item -LiteralPath "$SHARED/khtpm_draw_core.c" -Destination "khtpm_draw_core.c" -Force
New-Item -ItemType Directory -Force -Path "lib" | Out-Null
Copy-Item -LiteralPath "$SHARED/stb_image_write.h" -Destination "lib/stb_image_write.h" -Force

# REAL Stage 1 follow-up (2026-08-16) - dump_frame_png_op.+x is a real,
# standalone, shared op binary (system()-invoked, not text-included -
# see khtpm-merge-how2.md's own "HOUSE STANDARD" section), build it
# once, centrally, if missing.
OPS_BIN="$SHARED/ops/+x/dump_frame_png_op.+x"
if ((-not (Test-Path \""$OPS_BIN"\"))) {
  (cd "$SHARED/ops" && sh build_dump_frame_png_op.sh)
}

# REAL Stage 5 §5d.10 (2026-08-16) - db-hq mode now links
# khtpm_taskbar_manager.c/.h (already live in this ops/ dir, same real
# link line build_db_hq.sh already uses) for ktb_init()/
# ktb_quit_and_save() KtbState persistence.
Write-Host "-- entity-menu renderer -> +x/khtpm_entity_menu_render.+x"
& $CC @CFLAGS @X11_FLAGS -o +x/khtpm_entity_menu_render.+x \
  khtpm_entity_menu_render.c khtpm_css_parser.c khtpm_taskbar_manager.c @LIBS

Write-Host "OK +x/khtpm_entity_menu_render.+x"

# ===== NEXT SCRIPT =====

# build.ps1 - Windows twin of build_khtpm_strip.sh (*.livedesk-taskbar)
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

# build_khtpm_strip.sh — build the two-process strip architecture
# (khtpm_taskbar_manager_main.c manager driver + khtpm_strip_parser.c
# outer parser), per khtpm-strip-parser-design.md.
#
# PRODUCTION BINARY NAMES (2026-08-11): legacy tp_taskbar.c has been
# retired (archived to
# *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip, originals
# deleted — direct instruction: "id like to deprecate the old toolbar
# system now"). This khtpm pair is now the real, only taskbar — dropped
# the "_test" suffix these binaries carried through the whole build-out
# session, now that there's no live legacy binary left to avoid
# clobbering.
New-Item -ItemType Directory -Force -Path "+x" | Out-Null
$CC = "if ($env:CC) { $env:CC } else { "gcc" }"
$CFLAGS = @("-std=c11", "-Wall", "-O2")

# macOS leg (2026-08-22): XQuartz owns X11/Xft under /opt/X11 — brew's
# pkg-config doesn't search there by default, and clang won't find
# Xlib headers/libs without explicit -I/-L. Prebuilt emoji-helper
# binaries are Linux ELF, so on Darwin they build from wsr-pal source
# instead of being copied. Guarded: on Linux every added var stays
# empty and behavior is unchanged.
if ([ "$OS_TYPE" = "Darwin" ]) {
if (Get-Command pkg-config -ErrorAction SilentlyContinue) {
}
}

# Sync shared files from the single canonical source (2026-08-12
# dedup pass - see &.widgits/_shared-lib/README.md for why this is a
# build-time copy, not a runtime shared include path).
$SHARED = Join-Path (Split-Path -Parent $SCRIPT_DIR) "../../../&.widgits/_shared-lib"
Copy-Item -LiteralPath "$SHARED/khtpm_css_parser.c" -Destination "khtpm_css_parser.c" -Force
Copy-Item -LiteralPath "$SHARED/khtpm_css_parser.h" -Destination "khtpm_css_parser.h" -Force

Write-Host "-- khtpm manager driver (pure logic, no Xlib) -> +x/khtpm_taskbar_manager_main.+x"
& $CC @CFLAGS -o +x/khtpm_taskbar_manager_main.+x \
  khtpm_taskbar_manager_main.c khtpm_taskbar_manager.c

Write-Host "-- khtpm strip parser (Xlib + layout engine + hit-testing + manager fork/exec) -> +x/khtpm_strip_parser.+x"
# REAL FIX 2026-08-13: now links Xft (same flags as build_db_hq.sh) -
# khtpm_strip_parser.c grew real XftDrawStringUtf8-based CJK/UTF-8
# text rendering this session (see that file's own header comment on
# its Xft include), replacing plain XDrawString which could only
# render Latin-1 correctly.
& $CC @CFLAGS @X11_FLAGS $(pkg-config --cflags xft) -o +x/khtpm_strip_parser.+x \
  khtpm_strip_parser.c khtpm_strip_layout.c khtpm_taskbar_manager.c \
  -lX11 $(pkg-config --libs xft) -lm

# 2026-08-14 consolidation: the livedesk entity renderer + its helper
# set moved OUT of &.widgits/tile-picker into this runtime folder (the
# entity window is a livedesk-taskbar concern, not a tile-picker one).
# Built here now so the whole runtime is one folder + one build script.
Write-Host "-- entity renderer tp_desktop_window_rgb.c -> +x/tp_desktop_window_rgb.+x"
& $CC @CFLAGS @X11_FLAGS -o +x/tp_desktop_window_rgb.+x tp_desktop_window_rgb.c -lX11 -lXext

Write-Host "-- emoji->sprite helper tp_asset_to_sprite.c -> +x/tp_asset_to_sprite.+x"
& $CC @CFLAGS -o +x/tp_asset_to_sprite.+x tp_asset_to_sprite.c -lm

Write-Host "-- emoji atlas helpers emoji_gen_atlas/emoji_xtract (copied from wsr-pal)"
# emoji_gen_atlas.+x + emoji_xtract.+x ship as prebuilt binaries from the
# 014.wsr-pal toolchain (same source every other widget copies them from)
# - the entity calls them via ops_dir at runtime, so they must sit in the
# same +x/ dir as the entity binary.
$WSR = Join-Path (Split-Path -Parent (Split-Path -Parent $SCRIPT_DIR)) "014.wsr-pal💸️📌️+2"
foreach ($t in @("emoji_gen_atlas", "emoji_xtract")) {
if ((-not (Test-Path \""+x/$t.+x"\"))) {
if ([ "$OS_TYPE" = "Darwin" ]) {
            # macOS: prebuilt copies are Linux ELF — compile from source.
            # emoji_gen_atlas needs freetype; emoji_xtract doesn't.
if ([ -n "$WSR" ] && (Test-Path \""$WSR/ops/$t.c"\")) {
if (& $CC $CFLAGS -I"$WSR/ops" $FT_CFLAGS -o "+x/$t.+x" "$WSR/ops/$t.c" $FT_LIBS -lm 2>/dev/null) {
Write-Host "    $t.+x built from wsr-pal source"
} else {
Write-Host "WARN: $t.+x failed to build from source"
}
} else {
Write-Host "WARN: +x/$t.+x missing (no wsr-pal source to build)"
}
        elif [ -n "$WSR" ] && [ -x "$WSR/ops/+x/$t.+x" ]; then
Copy-Item -LiteralPath "$WSR/ops/+x/$t.+x" -Destination "+x/$t.+x" -Force
Write-Host "    $t.+x copied from wsr-pal"
} else {
Write-Host "WARN: +x/$t.+x missing (wsr-pal copy unavailable)"
}
}
}

Write-Host "-- window-position/range-grid helper tp_range_grid.c -> +x/tp_range_grid.+x"
& $CC @CFLAGS @X11_FLAGS -o +x/tp_range_grid.+x tp_range_grid.c -lX11 -lXext

# 2026-08-18: taskbar's terminal ASCII mirror (HQ menu "cli" row) - two
# binaries, matching TPMOS's real renderer.c/keyboard_input.c split
# (never combined - see khtpm_strip_render_ascii.c's own header comment
# for the real \r\n/staircase bug this split fixes).
Write-Host "-- taskbar ASCII renderer (no termios) -> +x/khtpm_strip_render_ascii.+x"
& $CC @CFLAGS -o +x/khtpm_strip_render_ascii.+x khtpm_strip_render_ascii.c

Write-Host "-- taskbar ASCII keyboard input (raw termios only, never prints) -> +x/khtpm_strip_keyboard_ascii.+x"
& $CC @CFLAGS -o +x/khtpm_strip_keyboard_ascii.+x khtpm_strip_keyboard_ascii.c

Write-Host "OK +x/khtpm_taskbar_manager_main.+x and +x/khtpm_strip_parser.+x (plus entity renderer + helpers)"
