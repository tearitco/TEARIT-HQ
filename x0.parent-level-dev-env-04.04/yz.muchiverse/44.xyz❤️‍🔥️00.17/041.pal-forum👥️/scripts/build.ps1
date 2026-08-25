# build.ps1 - Windows twin of build.sh (041.pal-forum👥️)
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

# scripts/build.sh - compile everything, warning-free.
#
# LOCAL COPIES, NOT A LIVE SHARED_OPS REFERENCE: this project keeps its
# own real, local copy of every file below that also exists in
# yz.muchiverse/2.muchi-verse/shared-ops/ (system/prisc+x.c,
# system/keyboard_input.c, system/chtpm_parser_pal.c,
# system/chtpm_rgb_render.c, ops/palnet_peer.c) - same convention every
# other project in this family follows (see ../shared-ops-manifest.txt).
# To pull in an update from the canonical source, run (from
# yz.muchiverse/2.muchi-verse/):
#   bash sync_shared_op.sh <op_name> pal-forum/<target_dir>

New-Item -ItemType Directory -Force -Path "ops/+x system" | Out-Null

$CFLAGS = @("-Wall", "-Wextra", "-O2")

Write-Host "--- Building system processes ---"
& gcc @CFLAGS "system/prisc+x.c" -o "system/prisc+x"
& gcc @CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
& gcc @CFLAGS "system/renderer.c" -o "system/renderer"

Write-Host "--- Building chtpm_parser_pal (PERSISTENT process, -Wno-unused-result"
Write-Host "    -Wno-stringop-truncation required - see shared-ops/chtpm_parser_pal.c)"
& gcc @CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

Write-Host "--- Building chtpm_rgb_render (local copy, PERSISTENT daemon) ---"
& gcc @CFLAGS "system/chtpm_rgb_render.c" -o "system/chtpm_rgb_render"

Write-Host "--- Building palnet_peer (local copy - reusable P2P op, see"
Write-Host "    ../PAL-NET-STANDARD.txt) ---"
& gcc @CFLAGS -o "ops/+x/palnet_peer.+x" "ops/palnet_peer.c"

Write-Host "--- Building pal-forum ops ---"
& gcc @CFLAGS -o "ops/+x/forum_create_user.+x" "ops/forum_create_user.c"
& gcc @CFLAGS -o "ops/+x/forum_switch_user.+x" "ops/forum_switch_user.c"
& gcc @CFLAGS -o "ops/+x/forum_post.+x" "ops/forum_post.c"
& gcc @CFLAGS -o "ops/+x/forum_follow.+x" "ops/forum_follow.c"
& gcc @CFLAGS -o "ops/+x/forum_like.+x" "ops/forum_like.c"
& gcc @CFLAGS -o "ops/+x/forum_retweet.+x" "ops/forum_retweet.c"
& gcc @CFLAGS -o "ops/+x/forum_dm.+x" "ops/forum_dm.c"
& gcc @CFLAGS -o "ops/+x/forum_compute_feed.+x" "ops/forum_compute_feed.c"
& gcc @CFLAGS -o "ops/+x/forum_inbox_watcher.+x" "ops/forum_inbox_watcher.c"
& gcc @CFLAGS -o "ops/+x/forum_menu_input.+x" "ops/forum_menu_input.c"
& gcc @CFLAGS -o "ops/+x/forum_compose_frame.+x" "ops/forum_compose_frame.c"

Write-Host "build ok"
